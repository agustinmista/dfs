-module(serv).
-compile(export_all).

-define(N_WORKERS, 5).
-define(INIT_FD, 100).

-define(BAD_FD, io_lib:format("BAD FD", [])).
-define(BAD_ARG, io_lib:format("BAD ARG", [])).
-define(F_OPEN, io_lib:format("FILE ALREADY OPEN", [])).
-define(F_CLOSED, io_lib:format("FILE IS CLOSED", [])).
-define(F_EXIST, io_lib:format("FILE ALREADY EXISTS", [])).
-define(F_NOTEXIST, io_lib:format("FILE NOT EXISTS", [])).
-define(F_NOTSPACE, io_lib:format("FILE: NOT ENOUGH SPACE", [])). %ver
-define(F_TOOMANY, io_lib:format("TOO MANY OPEN FILES~n", [])). %ver

% [INIT]------------------------------------------------------------------------------------------------------------------------------------------------------------------------

init(Port)->

    random:seed(now()),

    % Create File Descriptor Pool
    Pool = spawn(?MODULE, pool, [[]]),

    % Create the workers ring
    io:format("DFS_SERVER: Initializing workers...~n", []),
    Workers = init_workers(?N_WORKERS, Pool),

    [ H | T ] = Workers,
    Next_Worker = T ++ [H],
    create_ring(Workers, Next_Worker),

    % Create TCP listener and spawn the dispatcher
    io:format("DFS_SERVER: Opening listener at port ~w ...~n", [Port]),
    case gen_tcp:listen(Port, [list, {active, false}]) of
        {ok, ListenerSocket}  -> io:format("DFS_SERVER: Waiting for connections...~n", []),
                                 dispatcher(ListenerSocket, 1, Workers);
        _                     ->  io:format("DFSSERV: Error creating listening socket.~n", [])
    end.

% [DISPATCHER]------------------------------------------------------------------------------------------------------------------------------------------------------------------------

dispatcher(ListenerSocket, ClientId, Workers)->
    % Wait for new connections
    case gen_tcp:accept(ListenerSocket) of
        {ok, Socket}    ->  WorkerId = random:uniform(?N_WORKERS),
                            io:format("DFS_SERVER: New client! [id: ~p] [worker: ~p]~n", [ClientId, WorkerId]),
                            spawn(?MODULE, handle_client, [Socket, ClientId, lists:nth(WorkerId, Workers)]),
                            dispatcher(ListenerSocket, ClientId+1, Workers);
        % Error
        _               ->  io:format("DFS_SERVER: Error dispatching new connection.~n", []),
                            dispatcher(ListenerSocket, ClientId, Workers)
    end.

% [HANDLER]------------------------------------------------------------------------------------------------------------------------------------------------------------------------

handle_client(Socket, ClientId, W_pid)->
    case gen_tcp:recv(Socket, 0) of

    {ok, "CON\r\n"} ->  gen_tcp:send(Socket, io_lib:format("dfs> OK ID ~p~n", [ClientId])),
                        handle_client2(Socket, ClientId, W_pid);

    {ok, _}         ->  gen_tcp:send(Socket, io_lib:format("dfs> ERROR NOT IDENTIFIED~n", [])),
                        handle_client(Socket, ClientId, W_pid);

    {error, closed} ->  io:format("DFS_SERVER: Client [id: ~p] disconnected.~n", [ClientId])

  end.

handle_client2(Socket, ClientId, W_pid)->
    flush(), % ver - Creo que deberíamos eliminar mensajes que pueden quedar pendientes en after
    gen_tcp:send(Socket, io_lib:format("dfs>", [])),
    case gen_tcp:recv(Socket, 0) of
        {ok, Buff}  ->  L = string:len(Buff),
                        Buffer = string:left(Buff, L-2),
                        if
                            L < 4 ->
                                Answer = io_lib:format("dfs> ERROR BAD COMMAND~n", []),
                                gen_tcp:send(Socket, Answer),
                                handle_client2(Socket, ClientId, W_pid);
                            true ->
                                Req = parseRequest(string:tokens(Buffer, " ")),
                                case Req of
                                    {parse_error, AnswerF} -> gen_tcp:send(Socket, "dfs> ERROR: " ++ AnswerF ++ " \n"),
                                                              handle_client2(Socket, ClientId, W_pid);
                                    {bye}                  -> W_pid ! {Req, self(), 0},
                                                              io:format("dfs> Client [id: ~p] disconnected.~n", [ClientId]),
                                                              gen_tcp:close(Socket);
                                    _                      ->  W_pid ! {Req, self(), 0},
                                                               receive
                                                                   {ok, ok}        -> Ans = "dfs> OK \n",
                                                                                      gen_tcp:send(Socket,Ans);
                                                                   {ok,Answer}     -> Ans = "dfs> OK: " ++ Answer ++ " \n",
                                                                                      gen_tcp:send(Socket,Ans);
                                                                   {error, Answer} -> Ans = "dfs> ERROR: " ++ Answer ++ " \n",
                                                                                      gen_tcp:send(Socket, Ans)
                                                               after 300 -> gen_tcp:send(Socket, "dfs> ERROR 62 ETIME \n" )
                                                               end,
                                                               handle_client2(Socket, ClientId, W_pid)
                                end
                        end;
        {error, closed} ->  W_pid ! {{r_bye}, self(), 0},
                            io:format("DFS_SERVER: Client [id: ~p] disconected.~n", [ClientId])
    end.


parseRequest(Cmd)->
    case Cmd of
        ["CON"]                                 ->  {parse_error, io_lib:format("ALREADY CONECTED", [])};
        ["LSD"]                                 ->  {lsd, []};
        ["DEL", Arg0]                           ->  {del, Arg0};
        ["CRE", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {cre, Arg0};
                                                        false -> {parse_error, ?BAD_ARG}
                                                    end;
        ["OPN", Arg0]                           ->  {opn, Arg0};
        ["WRT", "FD", Arg0, "SIZE", Arg1|Tail]  ->  Arg2 = string:join(Tail, " "), %ver tail
                                                    Len = string:len(Arg2),
                                                    Fd = element(1,string:to_integer(Arg0)),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    if
                                                        Fd < ?INIT_FD   -> {parse_error, ?BAD_FD};
                                                        Size < 0        -> {parse_error, ?BAD_ARG};
                                                        Len /= Size     -> {parse_error, ?BAD_ARG};
                                                        true            -> {wrt, Fd, Size, Arg2}
                                                    end;
        ["REA", "FD", Arg0, "SIZE", Arg1]       ->  Fd = element(1,string:to_integer(Arg0)),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    if
                                                        Fd < ?INIT_FD   -> {parse_error, ?BAD_FD};
                                                        Size < 0        -> {parse_error, ?BAD_ARG};
                                                        true            -> {rea, Fd, Size}
                                                    end;
        ["CLO", "FD", Arg0]                     ->  Fd = element(1,string:to_integer(Arg0)),
                                                    if
                                                        Fd < ?INIT_FD   -> {parse_error, ?BAD_FD};
                                                        true            -> {clo, Fd}
                                                    end;
        ["BYE"]                                 ->  {bye};
        _                                       ->  {parse_error, io_lib:format("BAD COMMAND", [])}
    end.

is_valid(Name)->
    lists:all(
        fun(X) ->
            ((X >= $a) and (X =< $z)) or
            ((X >= $A) and (X =< $Z)) or
            ((X >= $0) and (X =< $9)) or
            (X == $.)
            end,
        Name).
        
flush() ->
    receive
        _ -> flush()
    after 0 -> ok
    end.

% [POOL]------------------------------------------------------------------------------------------------------------------------------------------------------------------------

pool(UsedFDs) ->
    receive
        {get, From} -> NewFD = getFD(UsedFDs),
                       From ! {ok, NewFD},
                       pool([NewFD | UsedFDs]);
        {free, FD, From} ->
            case lists:member(FD, UsedFDs) of
                    true -> NewUsedFDs = lists:filter(fun(X) -> X /= FD end, UsedFDs),
                            From ! ok,
                            pool(NewUsedFDs);
                    false -> From ! fail,
                             pool(UsedFDs)
            end
    end.

getFD([]) -> ?INIT_FD;
getFD(L) -> checkInUse(?INIT_FD, L).

checkInUse(N, L) ->
    case lists:member(N, L) of
        true -> checkInUse(N+1, L);
        false -> N
    end.
    
% [WORKERS]------------------------------------------------------------------------------------------------------------------------------------------------------------------------

init_workers(0, _)        -> [];
init_workers(N, FDPool)   ->
    Pid = spawn(?MODULE, worker, [[], FDPool, 0]),
    [Pid | init_workers(N-1, FDPool)].


create_ring([], [])            -> ok;
create_ring([H1|T1], [H2|T2])  ->
    H1 ! {next, H2},
    create_ring(T1, T2).



% FILE = {Name, Fd, Opener, Cursor, Size, Content} Voy a hacer opener y fd -1 para cerrado

close_files([], _, _)                                            -> [];
close_files([{Name, _, Pid, _, Size, Cont}|T], Pid, Pool)        -> Pool ! {free, Pid},
                                                                    receive
                                                                        ok -> [{Name, 0, -1, 0, Size, Cont}|close_files(T, Pid, Pool)]
                                                                    end;                                                                  
close_files([{Name, Fd, Open, Cursor, Size, Cont}|T], Pid, Pool) -> [{Name, Fd, Open, Cursor, Size, Cont}|close_files(T, Pid, Pool)].
                                                                    

worker(Files, Pool, Next)->
    receive
        {next, Next_W} -> worker(Files, Pool, Next_W);
        {Req, Pid, ?N_WORKERS} -> %vuelta al origen
            case Req of
               {bye}         ->  ok;
               {lsd, List}   ->  Pid ! {ok,List};
               {del, _}      ->  Pid ! {error, ?F_NOTEXIST};
               {cre, Name}   ->  NFiles = [{Name, -1, -1, 0, 0, []}|Files],
                                 Pid ! {ok,ok},
                                 worker(NFiles, Pool, Next);
               {opn, _}      ->  Pid ! {error, ?F_NOTEXIST};
               {wrt, _,_,_}  ->  Pid ! {error, ?F_NOTEXIST};
               {rea, _,_}    ->  Pid ! {error, ?F_NOTEXIST};
               {clo, _}      ->  Pid ! {error, ?F_NOTEXIST};
               {ok, Ans}     ->  Pid ! {ok, Ans};
               {error, Ans}  ->  Pid ! {error, Ans} %solo se comunica con el principal
            end,
            worker(Files, Pool, Next);
  
        {Req, Pid, Count} ->  
            case Req of
               {bye}       -> NFiles = close_files(Files,Pid,Pool),
                              Next ! {{bye}, Pid, Count+1},
                              worker(NFiles, Pool, Next);
               {lsd, List} -> ListAux = string:join(lists:map(fun ({Name,_,_,_,_,_}) -> Name end, Files), " "),
                              L1 = string:len(List),
                              L2 = string:len(ListAux),
                              if
                                  L1 == 0    -> Next ! {{lsd, ListAux}, Pid, Count+1};
                                  L2 == 0    -> Next ! {{lsd, List}, Pid, Count+1};
                                  true       -> Next ! {{lsd, List ++" "++ ListAux}, Pid, Count+1}
                              end;
               {del, Name} -> case lists:keytake(Name, 1, Files) of 
                                   {value, {Name,_,-1,_,_,_},NFiles} -> Next ! {{ok, ok}, Pid, Count+1},
                                                                        worker(NFiles, Pool, Next);
                                   false                             -> Next ! {{del, Name}, Pid, Count+1};
                                   _                                 -> Next ! {{error, ?F_OPEN}, Pid, Count+1}
                              end;
               {cre, Name} -> case lists:keymember(Name, 1, Files) of
                                  true    ->  Next ! {{error, ?F_EXIST}, Pid, Count+1};
                                  false   ->  Next ! {{cre, Name}, Pid, Count+1}
                              end;
               {opn, Name} -> case lists:keyfind(Name, 1, Files) of
                                 {Name, _, -1, _, Size, Cont}   ->  Pool ! {get, self()},
                                                                    receive
                                                                        {ok, N} -> NewFd = N
                                                                    end,
                                                                    NFiles = lists:keyreplace(Name, 1, Files, {Name, NewFd, Pid, 0, Size, Cont}),
                                                                    Next ! {{ok, io_lib:format("FD ~p", [NewFd])}, Pid, Count+1},
                                                                    worker(NFiles, Pool, Next);
                                 false                          ->  Next ! {{opn, Name}, Pid, Count+1};
                                 _                              ->  Next ! {{error, ?F_OPEN}, Pid, Count+1}
                              end;
              {wrt, Fd, Size, Buff} -> case lists:keyfind(Fd, 2, Files) of
                                         {Name, Fd, Pid, Cur, Sz, Cont} ->  NFiles = lists:keyreplace(Fd, 2, Files, {Name, Fd, Pid, Cur, Sz+Size, Cont ++ Buff}),
                                                                            Next ! {{ok, ok}, Pid, Count+1},
                                                                            worker(NFiles, Pool, Next);
                                         false                          ->  Next ! {{wrt, Fd, Size, Buff}, Pid, Count+1};
                                         _                              ->  Next ! {{error, ?F_CLOSED}, Pid, Count+1}
                                        end;
              {rea, Fd, Size}      -> case lists:keyfind(Fd, 2, Files) of
                                         {Name, Fd, Pid, Cur, Sz, Cont} -> Cond = (Size > 0) and (Cur < Sz),
                                                                           case Cond of
                                                                                false -> Next ! {{ok, ok}, Pid, Count+1};
                                                                                true  -> Amount = min(Size, Sz - Cur),
                                                                                         Buff = string:sub_string(Cont, Cur+1, Cur+Amount),
                                                                                         NFiles = lists:keyreplace(Fd, 2, Files, {Name, Fd, Pid, Cur+Amount, Sz, Cont}),
                                                                                         Next ! {{ok, io_lib:format("OK SIZE ~p ~p~n", [Amount, Buff])}, Pid, Count+1},
                                                                                         worker(NFiles, Pool, Next)
                                                                              end;
                                          false                            -> Next ! {{rea, Fd, Size}, Pid, Count+1};
                                          _                                -> Next ! {{error, ?F_CLOSED}, Pid, Count+1}
                                        end;
              {clo, Fd}             -> case lists:keytake(Fd, 2, Files) of
                                         {value, {Name, _, Pid, _, Size, Cont}, NFiles} -> Pool ! {free, Fd, self()}, %ver self()
                                                                                           receive
                                                                                             ok   -> NewFiles = [{Name, -1, -1, 0, Size, Cont}|NFiles],
                                                                                                     Next ! {{ok, ok}, Pid, Count+1},
                                                                                                     worker(NewFiles, Pool, Next);
                                                                                             fail -> Next ! {{error, ?BAD_FD}, Pid, Count+1}
                                                                                           end;
                                         {value, _}                                      ->  Next ! {{error, ?F_CLOSED}, Pid, Count+1}; %ver mas detalle
                                         false                                           ->  Next ! {{clo, Fd}, Pid, Count+1}
                                       end;
              {ok, Msj}    -> Next ! {{ok, Msj}, Pid, Count+1};
              {error, Msj} -> Next ! {{error, Msj}, Pid, Count+1}
            end,
    worker(Files, Pool, Next)
    end.


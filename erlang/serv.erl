-module(serv).
-compile(export_all).

-define(N_WORKERS, 5).
-define(INIT_FD, 100).

-define(BAD_FD, io_lib:format("dfs> ERROR: BAD FD~n", [])).
-define(BAD_ARG, io_lib:format("dfs> ERROR: BAD ARG~n", [])).
-define(F_OPEN, io_lib:format("dfs> ERROR: FILE ALREADY OPEN~n", [])).
-define(F_CLOSED, io_lib:format("dfs> ERROR: FILE IS CLOSED~n", [])).
-define(F_EXIST, io_lib:format("dfs> ERROR: FILE ALREADY EXIST~n", [])).
-define(F_NOTEXIST, io_lib:format("dfs> ERROR: FILE NOT EXIST~n", [])).
-define(F_NOTSPACE, io_lib:format("dfs> ERROR: FILE: NOT ENOUGH SPACE~n", [])).
-define(F_TOOMANY, io_lib:format("dfs> ERROR: TOO MANY OPEN FILES~n", [])).

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



handle_client(Socket, ClientId, W_pid)->
    case gen_tcp:recv(Socket, 0) of

    {ok, "CON\r\n"} ->  gen_tcp:send(Socket, io_lib:format("dfs> OK ID ~p~n", [ClientId])),
                        handle_client2(Socket, ClientId, W_pid);

    {ok, _}         ->  gen_tcp:send(Socket, io_lib:format("dfs> ERROR NOT IDENTIFIED~n", [])),
                        handle_client(Socket, ClientId, W_pid);

    {error, closed} ->  io:format("DFS_SERVER: Client [id: ~p] disconnected.~n", [ClientId])

  end.

handle_client2(Socket, ClientId, W_pid)->
    gen_tcp:send(Socket, io_lib:format("dfs>", [])),
    case gen_tcp:recv(Socket, 0) of
        {ok, Buff}  ->  L = string:len(Buff),
                        Buffer = string:left(Buff, L-2),
                        if
                            L < 4 ->
                                Answer = io_lib:format("ERROR EBADCMD~n", []),
                                gen_tcp:send(Socket, Answer),
                                handle_client2(Socket, ClientId, W_pid);
                            true ->
                                Req = parse_request(string:tokens(Buffer, " ")),
                                case Req of
                                    {error, Answer} ->  gen_tcp:send(Socket, Answer),
                                                        handle_client2(Socket, ClientId, W_pid);
                                    {r_bye}         ->  W_pid ! {Req, self(), 0},
                                                        io:format("Disconnected ClientId ~p~n", [ClientId]),
                                                        gen_tcp:close(Socket);
                                    _               ->  W_pid ! {Req, self(), 0},
                                                        receive
                                                            ABuff -> gen_tcp:send(Socket, ABuff)
                                                        after 500 -> gen_tcp:send(Socket, "ERROR ETIME" ++ "\n")
                                                        end,
                                                        handle_client2(Socket, ClientId, W_pid)
                                end
                        end;
        {error, closed} ->  W_pid ! {{r_bye}, self(), 0},
                            io:format("Disconnected ClientId ~p~n", [ClientId])
    end.

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

init_workers(0, _)        -> [];
init_workers(N, FDPool)   ->
    Pid = spawn(?MODULE, worker, [[], FDPool, 0]),
    [Pid | init_workers(N-1, FDPool)].


create_ring([], [])            -> ok;
create_ring([H1|T1], [H2|T2])  ->
    H1 ! {next, H2},
    create_ring(T1, T2).

parse_request(Cmd)->
    case Cmd of
        ["CON"]                                 ->  {error, io_lib:format("ERROR ALREADY CONECTED~n", [])};
        ["LSD"]                                 ->  {r_lsd, []};
        %["LSD"|_]                               ->  {error, io_lib:format("dfs> ERROR BAD ARGS. Use: LDS~n", [])};
        %["DEL"]                                 ->  {error, io_lib:format("ERROR DEL EINSARG~n", [])};
        ["DEL", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_del, Arg0};
                                                        false -> {error, io_lib:format("ERROR DEL EBADNAME~n", [])} %es necesario?
                                                    end;
        %["DEL", _|_]                            ->  {error, io_lib:format("ERROR DEL ETOOMANYARGS~n", [])};
        %["CRE"]                                 ->  {error, io_lib:format("ERROR CRE EINSARG~n", [])};
        ["CRE", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_cre, Arg0};
                                                        false -> {error, io_lib:format("ERROR CRE EBADNAME~n", [])}
                                                    end;
        %["CRE", _|_]                            ->  {error, io_lib:format("ERROR CRE ETOOMANYARGS~n", [])};
        %["OPN"]                                 ->  {error, io_lib:format("ERROR OPN EINSARG~n", [])};
        ["OPN", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_opn, Arg0};
                                                        false -> {error, io_lib:format("ERROR OPN EBADNAME~n", [])}
                                                    end;
        %["OPN", _|_]                            ->  {error, io_lib:format("ERROR OPN ETOOMANYARGS~n", [])};
        %["WRT"]                                 ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        %["WRT", "FD"]                           ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        %["WRT", "FD", _]                        ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        %["WRT", "FD", _, "SIZE"]                ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        %["WRT", "FD", _, "SIZE", _]             ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        ["WRT", "FD", Arg0, "SIZE", Arg1|Tail]  ->  Arg2Aux = lists:foldl(fun(Word, L) -> L ++ Word ++ " " end, [], Tail), %ver tail
                                                    L = string:len(Arg2Aux),
                                                    Arg2 = string:left(Arg2Aux, L-1),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    Fd = element(1,string:to_integer(Arg0)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, ?BAD_FD};
                                                        Size < 0        -> {error, io_lib:format("ERROR WRT EBADSIZE~n", [])};
                                                        Size /= L-1     -> {error, io_lib:format("ERROR WRT ESIZENOTMATCHINGBUFFER~n", [])};
                                                        true            -> {r_wrt, Fd, Size, Arg2}
                                                    end;
        %["REA"]                                 ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        %["REA", "FD"]                           ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        %["REA", "FD", _]                        ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        %["REA", "FD", _, "SIZE"]                ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        ["REA", "FD", Arg0, "SIZE", Arg1]       ->  Fd = element(1,string:to_integer(Arg0)),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, ?BAD_FD};
                                                        Size < 0        -> {error, io_lib:format("ERROR REA EBADIZE~n", [])};
                                                        true            -> {r_rea, Fd, Size}
                                                    end;
        %["REA", "FD", _, "SIZE", _|_]           ->  {error, io_lib:format("ERROR REA ETOOMANYARGS~n", [])};
        %["CLO"]                                 ->  {error, io_lib:format("ERROR CLO EINSARG~n", [])};
        %["CLO", "FD"]                           ->  {error, io_lib:format("ERROR CLO EINSARG~n", [])};
        ["CLO", "FD", Arg0]                     ->  Fd = element(1,string:to_integer(Arg0)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, ?BAD_FD};
                                                        true            -> {r_clo, Fd}
                                                    end;
        %["CLO", "FD", _|_]                      ->  {error, io_lib:format("EROR CLO ETOOMANYARGS~n", [])};
        ["BYE"]                                 ->  {r_bye};
        _                                       ->  {error, io_lib:format("dfs> ERROR: BAD COMMAND~n", [])}
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

% FILE = {Name, Fd, Opener, Position, Size, Content, To_Delete}

close_files([], _)                                          -> [];
close_files([{Name, _, C_pid, _, Siz, Cont, 0}|T], C_pid)   -> [{Name, 0, 0, 0, Siz, Cont, 0}|close_files(T, C_pid)];
close_files([{_,_,_,_,_,_, 1}|T], C_pid)                    -> close_files(T, C_pid);
close_files([{Name, Fd, Op, Pos, Siz, Cont, D}|T], C_pid)   -> [{Name, Fd, Op, Pos, Siz, Cont, D}|close_files(T, C_pid)].

worker(Files, Pool, Next)->
    receive
        {next, Next_W} -> worker(Files, Pool, Next_W);
        {Req, C_pid, ?N_WORKERS} ->
            case Req of
               {r_bye}         ->  ok;
               {r_lsd, List}   ->  C_pid ! "OK " ++ List ++ "\n";
               {r_del, _}      ->  C_pid ! io_lib:format("ERROR DEL EBADNAME~n", []);
               {r_cre, Name}   ->  C_pid ! io_lib:format("OK~n", []),
                                   NewFiles = [{Name, 0, 0, 0, 0, [], 0}|Files],
                                   worker(NewFiles, Pool, Next);
               {r_opn, _}      ->  C_pid ! io_lib:format("ERROR OPN EBADNAME~n", []);
               {r_wrt, _,_,_}  ->  C_pid ! ?BAD_FD;
               {r_rea, _,_}    ->  C_pid ! ?BAD_FD;
               {r_clo, _}      ->  C_pid ! ?BAD_FD
            end,
            worker(Files, Pool, Next);
        {Req, C_pid, Count} ->  case Req of
                                    {r_bye}       ->  NewFiles = close_files(Files, C_pid),
                                                      Next ! {{r_bye}, C_pid, Count+1},
                                                      worker(NewFiles, Pool, Next);
                                    {r_lsd, List} ->  ListedFiles = List ++ lists:foldl(fun({Name,_,_,_,_,_,_}, L) -> L ++ Name ++ " " end, [], Files),
                                                      Next ! {{r_lsd, ListedFiles}, C_pid, Count+1};
                                    {r_del, Name} ->  case lists:keytake(Name, 1, Files) of
                                                            {value, {_, 0,_,_,_,_,_},NF}                    ->  C_pid ! io_lib:format("OK~n", []),
                                                                                                                NewFiles = NF,
                                                                                                                worker(NewFiles, Pool, Next);
                                                            {value, {Name, Fd, Op, Pos, Siz, Cont, _}, NF}  ->  C_pid ! io_lib:format("~p: SET TO DELETE~n", [Name]),
                                                                                                                NewFiles = [{Name, Fd, Op, Pos, Siz, Cont, 1}|NF],
                                                                                                                worker(NewFiles, Pool, Next);
                                                            false                                           ->  Next ! {{r_del, Name}, C_pid, Count+1}
                                                      end;
                                    {r_cre, Name} ->  case lists:keymember(Name, 1, Files) of
                                                            true    ->  C_pid ! io_lib:format("ERROR CRE ENAMEEXISTS~n", []);
                                                            false   ->  Next ! {{r_cre, Name}, C_pid, Count+1}
                                                      end;
                                    {r_opn, Name} ->  case lists:keysearch(Name, 1, Files) of
                                                            {value, {Name, 0, _, _, Siz, Cont,_}}   ->  Pool ! {get, self()},
                                                                                                        receive
                                                                                                            {ok, N} -> NewFd = N
                                                                                                        end,
                                                                                                        NewFiles = lists:keyreplace(Name, 1, Files, {Name, NewFd, C_pid, 0, Siz, Cont,0}),
                                                                                                        C_pid ! io_lib:format("OK FD ~p~n", [NewFd]),
                                                                                                        worker(NewFiles, Pool, Next);
                                                            {value, _}                              ->  C_pid ! ?F_OPEN;
                                                            false                                   ->  Next ! {{r_opn, Name}, C_pid, Count+1}
                                                      end;
                                    {r_wrt, Fd, Size, Buff}-> case lists:keysearch(Fd, 2, Files) of
                                                                {value, {Name, Fd, C_pid, Pos, Siz, Cont,D}}->  NewFiles = lists:keyreplace(Fd, 2, Files, {Name, Fd, C_pid, Pos, Siz+Size, Cont ++ Buff,D}),
                                                                                                                C_pid ! io_lib:format("OK~n", []),
                                                                                                                worker(NewFiles, Pool, Next);
                                                                {value, _}                                  ->  C_pid ! io_lib:format("ERROR WRT ENOTOPENEDBYCLIENT~n", []);
                                                                false                                       ->  Next ! {{r_wrt, Fd, Size, Buff}, C_pid, Count+1}
                                                              end;
                                    {r_rea, Fd, Size}      -> case lists:keysearch(Fd, 2, Files) of
                                                                {value, {Name, Fd, C_pid, Pos, Siz, Cont,D}}  ->  Check = Siz - Pos,
                                                                                                    case Check =< 0 of
                                                                                                        true  -> C_pid ! io_lib:format("OK SIZE 0 NULL~n",[]);
                                                                                                        false -> if
                                                                                                                    Check >= Size -> Available = Size;
                                                                                                                    true          -> Available = Check
                                                                                                                 end,
                                                                                                                 Read = string:sub_string(Cont, Pos+1, Pos+Available),
                                                                                                                 NewFiles = lists:keyreplace(Fd, 2, Files, {Name, Fd, C_pid, Pos+Available, Siz, Cont,D}),
                                                                                                                 C_pid ! io_lib:format("OK SIZE ~p ~p~n", [Available, Read]),
                                                                                                                 worker(NewFiles, Pool, Next)
                                                                                                    end;
                                                                {value, _}                                  -> C_pid ! io_lib:format("ERROR REA ENOTOPENEDBYCLIENT~n", []);
                                                                false                                       -> Next ! {{r_rea, Fd, Size}, C_pid, Count+1}
                                                              end;
                                    {r_clo, Fd}            -> case lists:keytake(Fd, 2, Files) of
                                                                {value, {Name, Fd, C_pid, _, Siz, Cont, 0}, NF} ->  Pool ! {free, Fd, self()},
                                                                                                                    receive
                                                                                                                        ok -> NewFiles = [{Name, 0, 0, 0, Siz, Cont, 0}|NF],
                                                                                                                              C_pid ! io_lib:format("OK~n", []),
                                                                                                                              worker(NewFiles, Pool, Next);
                                                                                                                        fail -> C_pid ! ?BAD_FD,
                                                                                                                                worker(Files, Pool, Next)
                                                                                                                    end;
                                                                {value, {Name,_, C_pid,_,_,_, 1}, NF}           ->  C_pid ! io_lib:format("~p: WAS PENDING TO DELETE. DELETED~n", [Name]),
                                                                                                                    worker(NF, Pool, Next);
                                                                {value, _}                                      ->  C_pid ! io_lib:format("ERROR CLO ENOTOPENEDBYCLIENT~n", []);
                                                                false                                           ->  Next ! {{r_clo, Fd}, C_pid, Count+1}
                                                              end
                                end,
                                worker(Files, Pool, Next)
    end.

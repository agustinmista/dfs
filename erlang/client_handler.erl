-module(client_handler).

-export([handle_client/3, parse_request/1, is_valid/1]).


handle_client(Socket, ClientId, W_pid)->
    gen_tcp:send(Socket, io_lib:format("dfs>", [])),

    case gen_tcp:recv(Socket, 0) of

    {ok, "CON\r\n"} ->  gen_tcp:send(Socket, io_lib:format("dfs> OK ID ~p~n", [ClientId])),
                        handle_client2(Socket, ClientId, W_pid);

    {ok, _}         ->  gen_tcp:send(Socket, io_lib:format("dfs> ERROR NOT IDENTIFIED~n", [])),
                        handle_client(Socket, ClientId, W_pid);

    {error, closed} ->  io:format("DFS_SERVER: Client [id: ~p] disconnected.~n", [ClientId])

  end.

handle_client2(Socket, ClientId, W_pid)->
    case gen_tcp:recv(Socket, 0) of
        {ok, Buff}  ->  L = string:len(Buff),
                        Buffer = string:left(Buff, L-1),
                        if 
                            L < 4 -> 
                                Answer = io_lib:format("ERROR EBAD CMD~n", []),
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


parse_request(List)->
    case List of
        ["CON"]                                 ->  {error, io_lib:format("ERROR ALREADY CONECTED~n", [])};
        ["LSD"]                                 ->  {r_lsd, []};
        ["LSD"|_]                               ->  {error, io_lib:format("ERROR LSD ETOOMANYARGS~n", [])};
        ["DEL"]                                 ->  {error, io_lib:format("ERROR DEL EINSARG~n", [])};
        ["DEL", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_del, Arg0};
                                                        false -> {error, io_lib:format("ERROR DEL EBADNAME~n", [])}
                                                    end;   
        ["DEL", _|_]                            ->  {error, io_lib:format("ERROR DEL ETOOMANYARGS~n", [])};
        ["CRE"]                                 ->  {error, io_lib:format("ERROR CRE EINSARG~n", [])};
        ["CRE", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_cre, Arg0};
                                                        false -> {error, io_lib:format("ERROR CRE EBADNAME~n", [])}
                                                    end;   
        ["CRE", _|_]                            ->  {error, io_lib:format("ERROR CRE ETOOMANYARGS~n", [])};
        ["OPN"]                                 ->  {error, io_lib:format("ERROR OPN EINSARG~n", [])};
        ["OPN", Arg0]                           ->  case is_valid(Arg0) of
                                                        true  -> {r_opn, Arg0};
                                                        false -> {error, io_lib:format("ERROR OPN EBADNAME~n", [])}
                                                    end;
        ["OPN", _|_]                            ->  {error, io_lib:format("ERROR OPN ETOOMANYARGS~n", [])};
        ["WRT"]                                 ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        ["WRT", "FD"]                           ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        ["WRT", "FD", _]                        ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        ["WRT", "FD", _, "SIZE"]                ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};
        ["WRT", "FD", _, "SIZE", _]             ->  {error, io_lib:format("ERROR WRT EINSARG~n", [])};                
        ["WRT", "FD", Arg0, "SIZE", Arg1|Tail]  ->  Arg2Aux = lists:foldl(fun(Word, L) -> L ++ Word ++ " " end, [], Tail),
                                                    L = string:len(Arg2Aux),
                                                    Arg2 = string:left(Arg2Aux, L-1),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    Fd = element(1,string:to_integer(Arg0)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, io_lib:format("ERROR WRT EBADFD~n", [])};
                                                        Size < 0        -> {error, io_lib:format("ERROR WRT EBADSIZE~n", [])};
                                                        Size /= L-1     -> {error, io_lib:format("ERROR WRT ESIZENOTMATCHINGBUFFER~n", [])};
                                                        true            -> {r_wrt, Fd, Size, Arg2}
                                                    end;
        ["REA"]                                 ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        ["REA", "FD"]                           ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        ["REA", "FD", _]                        ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};
        ["REA", "FD", _, "SIZE"]                ->  {error, io_lib:format("ERROR REA EINSARG~n", [])};      
        ["REA", "FD", Arg0, "SIZE", Arg1]       ->  Fd = element(1,string:to_integer(Arg0)),
                                                    Size = element(1,string:to_integer(Arg1)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, io_lib:format("ERROR REA EBADFD~n", [])};
                                                        Size < 0        -> {error, io_lib:format("ERROR REA EBADIZE~n", [])};
                                                        true            -> {r_rea, Fd, Size}
                                                    end;
        ["REA", "FD", _, "SIZE", _|_]           ->  {error, io_lib:format("ERROR REA ETOOMANYARGS~n", [])};
        ["CLO"]                                 ->  {error, io_lib:format("ERROR CLO EINSARG~n", [])};
        ["CLO", "FD"]                           ->  {error, io_lib:format("ERROR CLO EINSARG~n", [])};
        ["CLO", "FD", Arg0]                     ->  Fd = element(1,string:to_integer(Arg0)),
                                                    if
                                                        Fd < ?INIT_FD   -> {error, io_lib:format("ERROR CLO EBADFD~n", [])};
                                                        true            -> {r_clo, Fd}
                                                    end;
        ["CLO", "FD", _|_]                      ->  {error, io_lib:format("EROR CLO ETOOMANYARGS~n", [])};
        ["BYE"]                                 ->  {r_bye};
        _                                       ->  {error, io_lib:format("ERROR EBADCMD~n", [])}
    end.

is_valid(Name)->
     lists:all(fun(X) -> ((X >= $a) and (X =< $z)) or 
                         ((X >= $A) and (X =< $Z)) or 
                         ((X >= $0) and (X =< $9)) or 
                         (X == $.) 
               end, Name).

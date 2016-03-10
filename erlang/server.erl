-module(server).

-compile(export_all).

-import(worker, [init_workers/2, create_ring/2]).
-import(dispatcher, [dispatcher/3]).

-define(N_WORKERS, 5).
-define(INIT_FD, 0).
 
init(Port)->
    
    random:seed(now()),
    
    % Create File Descriptor Pool
    FDPool = spawn(?MODULE, new_fd, [?INIT_FD]),
    
    % Create the workers ring
    io:format("DFS_SERVER: Initializing workers...~n", []),
    Workers = init_workers(?N_WORKERS, FDPool),
    
    [ H | T ] = Workers,
    Next_Worker = T ++ [H],
    create_ring(Workers, Next_Worker),  
    
    % Create TCP listener and spawn the dispatcher
    io:format("DFS_SERVER: Opening listener at port ~w ...", [Port]),
    case gen_tcp:listen(Port, [list, {active, false}]) of
        {ok, ListenerSocket}  ->  dispatcher(ListenerSocket, 1, Workers);
        _                     ->  io:format("DFSSERV: Error creating listening socket.~n", []),
                                  
    % Succesfull initialization!
    io:format("DFS_SERVER: Waiting for connections...~n", [])
    end.

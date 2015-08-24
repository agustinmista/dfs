-module(dispatcher).

-export([disparcher/3]).

-define(N_WORKERS, 5).

dispatcher(ListenerSocket, ClientId, Workers)->
    case gen_tcp:accept(ListenerSocket) of
        % New connection
        {ok, Socket}    ->  WorkerId = random:uniform(?N_WORKERS),
                            io:format("DFS_SERVER: New client! [id: ~p] [worker: ~p]~n", [ClientId, WorkerId]),
                            spawn(?MODULE, client_handler, [Socket, ClientId, lists:nth(WorkerId, Workers)]),
                            dispatcher(ListenerSocket, ClientId+1, Workers);
        % Error
        _               ->  io:format("DFS_SERVER: Error dispatching new connection.~n", []),
                            dispatcher(ListenerSocket, ClientId, Workers)
    
    end.


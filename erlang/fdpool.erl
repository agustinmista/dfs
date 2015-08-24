-module(fdpool).

-export([new_fd/1]).


new_fd(N) ->
    receive
        {newFD, Pid} -> Pid ! {ok, N},
                        newFD(N+1)
    end. 
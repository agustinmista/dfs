-module(worker).

-compile(export_all).


init_workers(0, _)        -> [];
init_workers(N, FDPool)   ->
    Pid = spawn(?MODULE, worker, [[], FDPool, 0]),
    [Pid | init_workers(N-1, FDPool)].


create_ring([], [])            -> ok;
create_ring([H1|T1], [H2|T2])  ->
    H1 ! {next, H2},
    create_ring(T1, T2).

  


% FILE = {Name, Fd, Opener, Position, Size, Content, To_Delete}

close_files([], _)                                          -> [];
close_files([{Name, _, C_pid, _, Siz, Cont, 0}|T], C_pid)   -> [{Name, 0, 0, 0, Siz, Cont, 0}|close_files(T, C_pid)];
close_files([{_,_,_,_,_,_, 1}|T], C_pid)                    -> close_files(T, C_pid);
close_files([{Name, Fd, Op, Pos, Siz, Cont, D}|T], C_pid)   -> [{Name, Fd, Op, Pos, Siz, Cont, D}|close_files(T, C_pid)].

worker(Files, FDPool, Next)->
    receive
        {next, Next_W}        -> worker(Files, FDPool, Next_W);
        {Req, C_pid, ?N_WORKERS} -> case Req of
                                    {r_bye}         ->  ok;
                                    {r_lsd, List}   ->  C_pid ! "OK " ++ List ++ "\n";
                                    {r_del, _}      ->  C_pid ! io_lib:format("ERROR DEL EBADNAME~n", []);
                                    {r_cre, Name}   ->  C_pid ! io_lib:format("OK~n", []),
                                                        NewFiles = [{Name, 0, 0, 0, 0, [], 0}|Files],
                                                        worker(NewFiles, FDPool, Next);
                                    {r_opn, _}      ->  C_pid ! io_lib:format("ERROR OPN EBADNAME~n", []);
                                    {r_wrt, _,_,_}  ->  C_pid ! io_lib:format("ERROR WRT EBADFD~n", []);
                                    {r_rea, _,_}    ->  C_pid ! io_lib:format("ERROR REA EBADFD~n", []);
                                    {r_clo, _}      ->  C_pid ! io_lib:format("ERROR CLO EBADFD~n", [])
                                end,
                                worker(Files, FDPool, Next);
        {Req, C_pid, Count} ->  case Req of
                                    {r_bye}       ->  NewFiles = close_files(Files, C_pid),
                                                      Next ! {{r_bye}, C_pid, Count+1},
                                                      worker(NewFiles, FDPool, Next);
                                    {r_lsd, List} ->  ListedFiles = List ++ lists:foldl(fun({Name,_,_,_,_,_,_}, L) -> L ++ Name ++ " " end, [], Files),
                                                      Next ! {{r_lsd, ListedFiles}, C_pid, Count+1};
                                    {r_del, Name} ->  case lists:keytake(Name, 1, Files) of
                                                            {value, {_, 0,_,_,_,_,_},NF}                    ->  C_pid ! io_lib:format("OK~n", []),
                                                                                                                NewFiles = NF,
                                                                                                                worker(NewFiles, FDPool, Next);
                                                            {value, {Name, Fd, Op, Pos, Siz, Cont, _}, NF}  ->  C_pid ! io_lib:format("~p: SET TO DELETE~n", [Name]),
                                                                                                                NewFiles = [{Name, Fd, Op, Pos, Siz, Cont, 1}|NF],
                                                                                                                worker(NewFiles, FDPool, Next);
                                                            false                                           ->  Next ! {{r_del, Name}, C_pid, Count+1}
                                                      end;
                                    {r_cre, Name} ->  case lists:keymember(Name, 1, Files) of
                                                            true    ->  C_pid ! io_lib:format("ERROR CRE ENAMEEXISTS~n", []);
                                                            false   ->  Next ! {{r_cre, Name}, C_pid, Count+1}
                                                      end;
                                    {r_opn, Name} ->  case lists:keysearch(Name, 1, Files) of
                                                            {value, {Name, 0, _, _, Siz, Cont,_}}   ->  FDPool ! {new_fd, self()},
                                                                                                        receive
                                                                                                            {ok, N} -> NewFd = N
                                                                                                        end,
                                                                                                        NewFiles = lists:keyreplace(Name, 1, Files, {Name, NewFd, C_pid, 0, Siz, Cont,0}),    
                                                                                                        C_pid ! io_lib:format("OK FD ~p~n", [NewFd]),
                                                                                                        worker(NewFiles, FDPool, Next);
                                                            {value, _}                              ->  C_pid ! io_lib:format("ERROR OPN EFILEALREADYOPENED~n", []);
                                                            false                                   ->  Next ! {{r_opn, Name}, C_pid, Count+1}
                                                      end;                                                                
                                    {r_wrt, Fd, Size, Buff}-> case lists:keysearch(Fd, 2, Files) of
                                                                {value, {Name, Fd, C_pid, Pos, Siz, Cont,D}}->  NewFiles = lists:keyreplace(Fd, 2, Files, {Name, Fd, C_pid, Pos, Siz+Size, Cont ++ Buff,D}),
                                                                                                                C_pid ! io_lib:format("OK~n", []),
                                                                                                                worker(NewFiles, FDPool, Next);
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
                                                                                                                 worker(NewFiles, FDPool, Next)
                                                                                                    end;
                                                                {value, _}                                  -> C_pid ! io_lib:format("ERROR REA ENOTOPENEDBYCLIENT~n", []);
                                                                false                                       -> Next ! {{r_rea, Fd, Size}, C_pid, Count+1}
                                                              end;
                                    {r_clo, Fd}            -> case lists:keytake(Fd, 2, Files) of
                                                                {value, {Name, Fd, C_pid, _, Siz, Cont, 0}, NF} ->  NewFiles = [{Name, 0, 0, 0, Siz, Cont, 0}|NF],    
                                                                                                                    C_pid ! io_lib:format("OK~n", []),
                                                                                                                    worker(NewFiles, FDPool, Next);
                                                                {value, {Name,_, C_pid,_,_,_, 1}, NF}           ->  C_pid ! io_lib:format("~p: WAS PENDING TO DELETE. DELETED~n", [Name]),
                                                                                                                    worker(NF, FDPool, Next); 
                                                                {value, _}                                      ->  C_pid ! io_lib:format("ERROR CLO ENOTOPENEDBYCLIENT~n", []);
                                                                false                                           ->  Next ! {{r_clo, Fd}, C_pid, Count+1}
                                                              end
                                end,
                                worker(Files, FDPool, Next)    
    end.    

#include "Common.h"
#include "Dispatcher.h"
#include "ClientHandler.h"

void *init_dispatcher(void *s){
    
    int socket = *((int *) s);
    int conn_id;    
    
    printf("DFS_SERVER: Waiting for connections...\n");
    while(1){
        if((conn_id = accept(socket, NULL, NULL))<0)
            ERROR("\nDFS_SERVER: Error dispatching new connection");
            
          
        printf("DFS_SERVER: New connection!, id: %d\n", conn_id);
    }

}
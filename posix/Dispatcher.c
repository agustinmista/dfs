#include "Common.h"
#include "Dispatcher.h"
#include "Worker.h"
#include "ClientHandler.h"

int getWorkerId(){
    return (rand() % N_WORKERS);
}


void *init_dispatcher(void *s){
    
    int socket = *((int *) s);
    int conn_id;
    
    pthread_t new_client;	
    
    printf("DFS_SERVER: Waiting for connections...\n");
    while(1){
        
        // Wait for a new connections
        if((conn_id = accept(socket, NULL, NULL))<0)
            ERROR("DFS_SERVER: Error dispatching new connection\n");
        
        // Create a session for the new client and asign a worker
        char *client_name;
        asprintf(&client_name, "/c%d", conn_id);
    
        mqd_t client_queue;
        if((client_queue = mq_open(client_name, O_RDWR | O_CREAT, 0666, NULL)) == (mqd_t) -1)
            ERROR("DFS_SERVER: Error opening message queue for the new client\n");
        
        Session *newSession = (Session *) malloc(sizeof(Session));
        newSession->client_id    = conn_id;   // aprovecho el id del socket
        newSession->worker_id    = getWorkerId();
        newSession->client_queue = client_queue;
        newSession->worker_queue = worker_queues[newSession->worker_id];
            
        // Spawn a ClientHandler for the new client
        if (pthread_create(&new_client, NULL, handle_client, newSession) != 0)
                ERROR("DFS_SERVER: Error creating pthread \n"); 

        printf("DFS_SERVER: New client!\tid: %d\tworker: %d\n", newSession->client_id, newSession->worker_id);
        
    }

}


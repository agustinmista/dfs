#include "Common.h"
#include "Dispatcher.h"
#include "Worker.h"
#include "ClientHandler.h"


// Returns a random worker id.
int getWorkerId(){
    return (rand() % N_WORKERS);
}


// Dispatch new connections, creating a new session 
// and setting a random worker for each client.
void *init_dispatcher(void *s){
    
    // Init random number generator
    srand(time(NULL));

    int socket = *((int *) s);
    int conn_id;

    pthread_t new_client;

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MSG_SIZE;
    attr.mq_curmsgs = 0;
    
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DFS_SERVER: Waiting for connections...\n");
    #else
    printf("DFS_SERVER: The server is ready!\n");
    #endif
    
    // Enter dispatching loop
    while(1){

        // Wait for a new connections
        if((conn_id = accept(socket, NULL, NULL))<0)
            ERROR("DFS_SERVER: Error dispatching new connection.\n");

        // Create a session for the new client and asign a worker
        Session *newSession = (Session *) malloc(sizeof(Session));
        newSession->client_id    = conn_id;   // aprovecho el id del socket
        newSession->worker_id    = getWorkerId();
        newSession->client_queue = malloc(sizeof(mqd_t*));
        newSession->worker_queue = worker_queues[newSession->worker_id];

        char *client_name;
        asprintf(&client_name, "/c%d", conn_id);

        mqd_t *client_queue = (mqd_t*) newSession->client_queue;
        if((*client_queue = mq_open(client_name, O_RDWR | O_CREAT, 0644, &attr)) == (mqd_t)-1)
            ERROR("DFS_SERVER: Error opening message queue for the new client.\n");


        // Spawn a ClientHandler for the new client
        if (pthread_create(&new_client, NULL, handle_client, newSession) != 0)
                ERROR("DFS_SERVER: Error creating pthread.\n");

        #if defined(DEBUG) || defined(DEBUG_REQUEST)
        printf("DFS_SERVER: New client! [id: %d] [worker: %d]\n",
               newSession->client_id, newSession->worker_id);
        #endif

    }

}

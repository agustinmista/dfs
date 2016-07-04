#include "Common.h"
#include "Worker.h"
#include "Dispatcher.h"

int main(int argc, char **argv){
    
    // Parse linstening por from argv, if present
    int port = argc == 2 ? atoi(argv[1]) : 8000;    
    
    int sock;
	struct sockaddr_in server_address;
    pthread_t dispatcher;
    
    // Disable stdout buffering
    setbuf(stdout, NULL);
    
    // Initialize and set listener socket
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DFS_SERVER: Opening listener at port %d... ", port);
    #endif
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR("\nDFS_SERVER: Error creating socket");
	
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family      = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port        = htons(port);
    
	if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
        ERROR("\nDFS_SERVER: Error calling bind");
    
	if (listen(sock, 10) < 0)
		ERROR("\nDFS_SERVER: Error calling listen");
        
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DONE\n");
    #endif
    
    // Initialize workers
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DFS_SERVER: Initializing workers... ");
    #endif
    
    if (init_workers() < 0)
        ERROR("\nDFS_SERVER: Error initializing workers");
    
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DONE\n");
    #endif

    // Spawn dispatcher thread and join it
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DFS_SERVER: Initializing dispatcher... ");    
    #endif
    if(pthread_create(&dispatcher, NULL, init_dispatcher, (void *) &sock) != 0)
        printf("DFS_SERVER: Error initializing dispatcher... ");   
    #if defined(DEBUG) || defined(DEBUG_REQUEST)
    printf("DONE\n");
    #endif
    pthread_join(dispatcher, NULL);
    
    return 0;
}

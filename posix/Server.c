#include "Common.h"
#include "Worker.h"
#include "Dispatcher.h"

int main(int argc, char **argv){
    
    int port = argc == 2 ? atoi(argv[1]) : 8000;    
    int sock;
	struct sockaddr_in server_address;
    pthread_t dispatcher;
    
    setbuf(stdout, NULL);
    
    // Initialize and set listener socket
    printf("DFS_SERVER: Opening listener at port %d... ", port);
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
    
    printf("DONE\n");
    
    
    // Initialize workers
    printf("DFS_SERVER: Initializing workers... ");
    if (init_workers(N_WORKERS) < 0)
        ERROR("\nDFS_SERVER: Error initializing workers");
    printf("DONE\n");
    

    // Spawn dispatcher thread and join it
    printf("DFS_SERVER: Initializing dispatcher... ");    
    if(pthread_create(&dispatcher, NULL, init_dispatcher, (void *) &sock) != 0)
        printf("DFS_SERVER: Error initializing dispatcher... ");   
    printf("DONE\n");
    pthread_join(dispatcher, NULL);
    
    return 0;
}
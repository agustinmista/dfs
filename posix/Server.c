#include "Common.h"
#include "Worker.h"
#include "Dispatcher.h"

int main(int argc, char **argv){

    int sock;
	struct sockaddr_in server_address;
    
    setbuf(stdout, NULL);
    
    // Initialize and set listener socket
    printf("DFS_SERVER: Setting up socket... ");
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR("\nDFS_SERVER: Error creating socket");
	
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family      = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port        = htons(8000);
    
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
    

    // Initialize dispatcher loop
    printf("DFS_SERVER: Initializing dipatcher loop... ");
    if (init_dispatcher(sock) < 0)
        ERROR("\nDFS_SERVER: Error initializing dispatcher loop");
    
    return 0;
}
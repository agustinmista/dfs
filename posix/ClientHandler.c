#include "Common.h"
#include "ClientHandler.h"

#define SEND2CLIENT(fmt, ...)   write(client_id, buffer_out, sprintf(buffer_out, fmt, ## __VA_ARGS__))
#define SEND2WORKER(fmt, ...)   mq_send(worker_queue, buffer_out, sprintf(buffer_out, fmt, ## __VA_ARGS__), 1)


//formato de mensaje a worker: CMD args(dependiendo de CMD) client_queue
//mq_receive con direccion client_name que corresponda no?

//printf(%.*s\n", size, buffer);  imprime los primeros size caracteres de buffer
        
void *handle_client(void *s){
    int identified = 0;
    int readed;
    
    char buffer_in[MSG_SIZE];
    char buffer_out[MSG_SIZE];

    // Parse session args
    int client_id      = ((Session *) s)->client_id;
	int worker_id      = ((Session *) s)->worker_id;
	mqd_t worker_queue = ((Session *) s)->worker_queue;
	mqd_t client_queue = ((Session *) s)->client_queue;
    free(s);
    
    // INTENTO ENVIAR DATOS AL WORKER
    SEND2WORKER("CRE hola.txt");
    
    // Client handle loop
    while(1){
        if ((readed = read(client_id, buffer_in, MSG_SIZE)) <= 0){
            printf("DFS_SERVER: Client (id: %d) disconected.\n", client_id);
            break;
        }
        
        // Remove telnet \r\n characters
        readed-=2;
		buffer_in[readed] = '\0';
		
        // Parse commands
        if(identified){
            if (strlen(buffer_in) < 3)
                SEND2CLIENT("> ERROR: BAD COMMAND\n");
            else if (!strncmp(buffer_in, "CON", 3))
                SEND2CLIENT("> ERROR: ALREADY IDENTIFIED\n");
            else if (!strncmp(buffer_in, "BYE", 3)){
                identified = 0;
                SEND2CLIENT("> OK\n");
            } else if (!strncmp(buffer_in, "LSD", 3)) {     
                // LSD
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "DEL", 3)) {     
                // DEL
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "CRE", 3)) {     
                // CRE
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");    
            } else if (!strncmp(buffer_in, "OPN", 3)) {     
                // OPN
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "WRT FD", 6)) {  
                // WRT
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "REA FD", 6)) {  
                // REA
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");        
            } else if (!strncmp(buffer_in, "CLO FD", 6)) {  
                // CLO
                SEND2CLIENT("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else
                SEND2CLIENT("> ERROR: INVALID COMMAND\n");
                
        } else if(!strncmp(buffer_in, "CON", 3)) {
            identified = 1;
            SEND2CLIENT("> OK ID %d\n", client_id); 
        } else
            SEND2CLIENT("> ERROR NOT IDENTIFIED\n");
    }
    
    // Close everything
    if(mq_close(client_queue) == -1) ERROR("DFS_SERVER: Error closing client message queue.\n");
    if(mq_close(client_queue) == -1) ERROR("DFS_SERVER: Error closing worker message queue.\n");
    //...
    
    return NULL;
}



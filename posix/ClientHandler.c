#include "Common.h"
#include "ClientHandler.h"

#define SEND(fmt, ...)      write(client_id, buffer_out, sprintf(buffer_out, fmt, ## __VA_ARGS__))

//formato de mensaje a worker: CMD args(dependiendo de CMD) client_queue
//mq_receive con direccion client_name que corresponda no?

//printf(%.*s\n", size, buffer);  imprime los primeros size caracteres de buffer
        
void *handle_client(void *s){
    int identified = 0;
    
    char buffer_in[MSG_SIZE];
    char buffer_out[MSG_SIZE];
    
    int readed;
    
    // Parse session args
    int client_id      = ((Session *) s)->client_id;
	int worker_id      = ((Session *) s)->worker_id;
	mqd_t worker_queue = ((Session *) s)->worker_queue;
	mqd_t client_queue = ((Session *) s)->client_queue;
    free(s);
    
    // Client handle loop
    while(1){
        if ((readed = read(client_id, buffer_in, MSG_SIZE)) <= 0)
            ERROR("DFS_SERVER: Error reading bytes from client\n");
		buffer_in[readed-2] = '\0';
        
        if(identified){
            if (strlen(buffer_in) < 3)
                SEND("> ERROR: BAD COMMAND\n");
            else if (!strncmp(buffer_in, "CON", 3))
                SEND("> ERROR: ALREADY IDENTIFIED\n");
            else if (!strncmp(buffer_in, "BYE", 3)){
                identified = 0;
                SEND("> BYE\n");
            } else if (!strncmp(buffer_in, "LSD", 3)) {     
                // LSD
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "DEL", 3)) {     
                // DEL
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "CRE", 3)) {     
                // CRE
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");    
            } else if (!strncmp(buffer_in, "OPN", 3)) {     
                // OPN
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "WRT FD", 6)) {  
                // WRT
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else if (!strncmp(buffer_in, "REA FD", 6)) {  
                // REA
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");        
            } else if (!strncmp(buffer_in, "CLO FD", 6)) {  
                // CLO
                SEND("> COMMAND NOT IMPLEMENTED (YET)\n");
            } else
                SEND("> ERROR: INVALID COMMAND\n");
                
        } else if(!strncmp(buffer_in, "CON", 3)) {
            identified = 1;
            SEND("> OK ID %d\n", client_id); 
        } else
            SEND("> ERROR NOT IDENTIFIED\n");
    }
    
    return NULL;
}



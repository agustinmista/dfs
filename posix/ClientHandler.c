#include "Common.h"
#include "ClientHandler.h"

#define SEND2CLIENT(fmt,...)    write(client_id, buffer_out, sprintf(buffer_out, fmt, ##__VA_ARGS__))
#define SEND_REQ(request)       mq_send(worker_queue, (char *) request, sizeof(request), 1)
#define RECV_ANS()              ;

//printf(%.*s\n", size, buffer);  imprime los primeros size caracteres de buffer

void print_request(Request *r){
    printf("| REQUEST:\n");
    printf("|   op: %d\n", r->op);
    printf("|   arg0: %s\n", r->arg0);
    printf("|   arg1: %s\n", r->arg1);
    printf("|   arg2: %s\n", r->arg2);
}

void *handle_client(void *s){
    int identified = 0;
    int readed;
    
    char buffer_in[MSG_SIZE];
    char buffer_out[MSG_SIZE];

    Request *req;
    
    // Parse session args
    Session *session = (Session *) s;
    int client_id      = session->client_id;
	int worker_id      = session->worker_id;
	mqd_t worker_queue = session->worker_queue;
	mqd_t client_queue = session->client_queue;
    
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
                req = parseRequest(session, LSD, buffer_in);
                print_request(req);
                
                // Send the request, wait for the answer and print results
                // SEND_REQ(req);
                // sleep a bit
                // RECV_ANS();
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
    free(s);
    //...
    
    return NULL;
}

Request *parseRequest(Session *s, Operation op, char *string){
    char *saveptr;
    char *arg0;
    char *arg1;
    char *arg2;
    
    // Allocate a session with default values
    Request *req = (Request *) malloc(sizeof(Request));
    req->op = op;
    req->arg0 = NULL;
    req->arg1 = NULL;
    req->arg1 = NULL;
    req->client_id = s->client_id;
    req->client_queue = &(s->client_queue);
    
    // Set the request parameters
    switch(op){
        case LSD:
            // LSD takes no parameters, nothing to do!
            break;
        case DEL:
            
            break;
        case CRE:
            break;
        case OPN:
            break;
        case WRT:
            break;
        case REA:
            break;
        case CLO:
            break;
        case BYE:
            break;
    }
    return req;
}

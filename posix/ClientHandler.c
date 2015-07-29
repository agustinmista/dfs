#include "Common.h"
#include "ClientHandler.h"

#define SEND2CLIENT(fmt,...)    write(client_id, buffer_out, sprintf(buffer_out, fmt, ##__VA_ARGS__))
#define SEND_REQ(request)       mq_send(worker_queue, (char *) request, sizeof(request), 1)
#define RECV_ANS()              ;

//printf(%.*s\n", size, buffer);  imprime los primeros size caracteres de buffer

void print_request(Request *r){
    printf("REQUEST: [id: %d] [op: %d] [arg0:'%s'] [arg1:'%s\'] [arg2:'%s']\n",
           r->client_id, r->op, r->arg0, r->arg1, r->arg2);
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
            if (strlen(buffer_in) < 3){
                req = NULL;
                SEND2CLIENT("> ERROR: BAD COMMAND\n");
                                
            } else if (!strncmp(buffer_in, "LSD", 3)) {     
                if (!(req = parseRequest(session, LSD, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "DEL", 3)) {
                if (!(req = parseRequest(session, DEL, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "CRE", 3)) {     
                if (!(req = parseRequest(session, CRE, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "OPN", 3)) {     
                if (!(req = parseRequest(session, OPN, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "WRT", 3)) {  
                if (!(req = parseRequest(session, WRT, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "REA", 3)) {  
                if (!(req = parseRequest(session, REA, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "CLO", 3)) {  
                if (!(req = parseRequest(session, CLO, buffer_in)))
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                
            } else if (!strncmp(buffer_in, "BYE", 3)){
                if (!(req = parseRequest(session, BYE, buffer_in))) 
                    SEND2CLIENT("> ERROR: BAD COMMAND\n");
                identified = 0;
                SEND2CLIENT("> OK\n");  
                
            } else if (!strncmp(buffer_in, "CON", 3)){
                req = NULL;
                SEND2CLIENT("> ERROR: ALREADY IDENTIFIED\n");    
                
            } else {
                req = NULL;
                SEND2CLIENT("> ERROR: BAD COMMAND\n");
            }
            
            
            // If we generate a request, send it, wait for response and print results
            if(req){
                print_request(req);
                // SEND_REQ(req);
                // sleep a little bit
                // Reply ans = RECV_ANS();
                
//                switch(ans->err){
//                    // If no errors, print ok + extra data depending command
//                    case NONE:
//                        if (req->op == OPN || req->op == REA || req->op == LSD) 
//                            SEND2CLIENT("> OK: %s\n", ans->answer);
//                        else 
//                            SEND2CLIENT("> OK\n");
//                        break;
//                    
//                    // Otherwise, print error
//                    case BAD_FD:
//                        SEND2CLIENT("> ERROR: BAD FD\n");
//                        break;
//                    case BAD_ARG:
//                        SEND2CLIENT("> ERROR: BAD ARG\n");
//                        break;
//                    case F_OPEN:
//                        SEND2CLIENT("> ERROR: FILE ALREADY OPEN\n");
//                        break;
//                    case F_EXIST:
//                        SEND2CLIENT("> ERROR: FILE ALREADY EXIST\n");
//                        break;                    
//                    case F_NOTEXIST:
//                        SEND2CLIENT("> ERROR: FILE NOT EXIST\n");
//                        break;
//                }
                
                // SEND2CLIENT(answer)
                
                //free(ans);
                free(req);
            }
            
            
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
    char *token_size;
    
    // Allocate a session with default values
    Request *req = (Request *) malloc(sizeof(Request));
    req->op = op;
    req->arg0 = NULL;
    req->arg1 = NULL;
    req->arg1 = NULL;
    req->client_id = s->client_id;
    req->client_queue = &(s->client_queue);
    
    // Ignore "CMD"
    if(!(strtok_r(string, " ", &saveptr))) return NULL;
    
    // Set the request parameters a.k.a. return NULL;  :P
    switch(op){
        case LSD:
            // LSD takes no parameters, nothing to do!
            break;
        
        case DEL:
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
            break;
        
        case CRE:
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
            break;
        
        case OPN:
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
            break;
               
        case WRT:
            // Check if "FD" token is present
            if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
            if(strncmp(token_size, "FD", 2)) return NULL;
               
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
            // Check if "SIZE" token is present
            if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
            if(strncmp(token_size, "SIZE", 4)) return NULL;
        
            if(!(req->arg1 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
//... Something to ignore starting whitespaces
        
            if(!(req->arg2 = strtok_r(NULL, "\n", &saveptr))) return NULL;
            
            // Check if size is correct
            if(atoi(req->arg1) > strlen(req->arg2)) return NULL;
            
            break;
        
        case REA:
            // Check if "FD" token is present
            if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
            if(strncmp(token_size, "FD", 2)) return NULL;
               
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
            // Check if "SIZE" token is present
            if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
            if(strncmp(token_size, "SIZE", 4)) return NULL;
        
            if(!(req->arg1 = strtok_r(NULL, " ", &saveptr))) return NULL;
            
            break;
        
        case CLO:
            // Check if "FD" token is present
            if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
            if(strncmp(token_size, "FD", 2)) return NULL;
        
            if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
            break;
        
        case BYE:
            // BYE takes no parameters, nothing to do!
            break;
    }
    return req;
}
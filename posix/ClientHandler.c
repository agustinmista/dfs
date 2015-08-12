#include "Common.h"
#include "ClientHandler.h"

#define SEND2CLIENT(fmt,...)    write(client_id, buffer_out, sprintf(buffer_out, fmt, ##__VA_ARGS__))
#define SEND_REQ(request)       mq_send(worker_queue, (char *) request, sizeof(Request), 1)
#define RECV_ANS()              mq_receive(client_queue, buffer_in, MSG_SIZE, NULL)

//printf(%.*s\n", size, buffer);  imprime los primeros size caracteres de buffer


int is_valid(char *s){
    if(strlen(s) == 1 && *s == '.') return 0;
    for(int i=0; i<strlen(s); i++) 
        if(!isalnum(s[i]) && s[i] != '.') return 0;
    return 1;
}

void *handle_client(void *s){
    int identified = 0;
    int readed;
    
    char buffer_in[MSG_SIZE];
    char buffer_out[MSG_SIZE];

    Request *req = NULL;
    
    // Parse session args
    Session *session = (Session *) s;
    int client_id      = session->client_id;
	mqd_t worker_queue = session->worker_queue;
	mqd_t client_queue = *(session->client_queue);
	Reply *ans; //malloc en Worker
	
    // Client handle loop
    while(1){
        
        SEND2CLIENT("dfs> ");
        
        if ((readed = read(client_id, buffer_in, MSG_SIZE)) <= 0){
            printf("DFS_SERVER: Client [id: %d] disconected.\n", client_id);
            break;
        }
        
        // Remove telnet \r\n characters
        readed-=2;
		buffer_in[readed] = '\0';
		
        // Ignore empty lines
        if (!strlen(buffer_in)) continue;               
        
        // Parse commands
        else if(identified){
                
            if (strlen(buffer_in) < 3)
                SEND2CLIENT("dfs> ERROR: BAD COMMAND\n");
            else if(!strncmp(buffer_in, "CON", 3))
                SEND2CLIENT("dfs> ERROR: ALREADY IDENTIFIED\n");    
            else if(!(req = parseRequest(session, buffer_in)))
                SEND2CLIENT("dfs> ERROR: BAD COMMAND\n");
            
            // If the parser has generated a request, send it, wait for response and print results
            if(req){
                    
                if(SEND_REQ(req) != 0) ERROR("DFS_SERVER: Error sending request to worker.\n");
                
                if((readed = RECV_ANS()) == -1) ERROR("DFS_SERVER: Error receiving answer from worker.\n");
                
                // Sample data
                //ans->err = NONE;
                //ans->answer = "Todo bien!";
                
				ans = (Reply *)buffer_in;
                
                switch(ans->err){
                    // If no errors, print ok + extra data depending on command
                    case NONE:
                        if (req->op == OPN || req->op == REA || req->op == LSD) 
                            SEND2CLIENT("dfs> OK: %s\n", ans->answer);
                        else 
                            SEND2CLIENT("dfs> OK\n");
                        break;
                        
                    // Otherwise, print error
                    case BAD_FD:     SEND2CLIENT("dfs> ERROR: BAD FD\n");                  break;
                    case BAD_ARG:    SEND2CLIENT("dfs> ERROR: BAD ARG\n");                 break;
                    case F_OPEN:     SEND2CLIENT("dfs> ERROR: FILE ALREADY OPEN\n");       break;                    
                    case F_CLOSED:   SEND2CLIENT("dfs> ERROR: FILE IS CLOSED\n");          break;
                    case F_EXIST:    SEND2CLIENT("dfs> ERROR: FILE ALREADY EXIST\n");      break;                    
                    case F_NOTEXIST: SEND2CLIENT("dfs> ERROR: FILE NOT EXIST\n");          break;
                    case NOT_IMP:    SEND2CLIENT("dfs> ERROR: COMMAND NOT IMPLEMENTED\n"); break;
                }
                
                // If operation was BYE, logout after receive workers OK
                if(req->op == BYE) identified = !identified;
        
                
          
            }
            
            
        } else if(!strncmp(buffer_in, "CON", 3)) {
            identified = 1;
            SEND2CLIENT("dfs> OK ID %d\n", client_id); 
        } else
            SEND2CLIENT("dfs> ERROR NOT IDENTIFIED\n");
    }
    
    // Close everything
    if(mq_close(client_queue) == -1) ERROR("DFS_SERVER: Error closing client message queue.\n");
    if(mq_close(client_queue) == -1) ERROR("DFS_SERVER: Error closing worker message queue.\n");
    free(s);
    
    // Free everything
    free(ans);
    free(req);
     
    return NULL;
}


Request *parseRequest(Session *s, char *cmd){
    char *saveptr;
    char *token_size;
    char *token_fd;
    
    // Allocate a session with default values
    Request *req = (Request *) malloc(sizeof(Request));
    req->arg0 = NULL;
    req->arg1 = NULL;
    req->arg2 = NULL;
    req->external = 1;
    req->main_worker = s->worker_id;
    req->client_id = s->client_id;
    req->client_queue = s->client_queue;
    
    // Get the operation token
    char *opcode = strtok_r(cmd, " ", &saveptr);
    
    // Set the request parameters a.k.a. "return NULL"
    if (!strncmp(opcode, "LSD", 3)){
        req->op = LSD;
        
        return req;
        
    } else if (!strncmp(opcode, "DEL", 3)) {
        req->op = DEL;
        
        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "CRE", 3)) {
        req->op = CRE;
        
        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL; 
        
        // Check name
        if(!is_valid(req->arg0)) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "OPN", 3)) {
        req->op = OPN;
        
        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "WRT", 3)) {
        req->op = WRT;
        
        // Check if "FD" token is present
        if(!(token_fd = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(strncmp(token_fd, "FD", 2)) return NULL;

        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;

        // Check if "SIZE" token is present
        if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(strncmp(token_size, "SIZE", 4)) return NULL;

        if(!(req->arg1 = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(!(req->arg2 = strtok_r(NULL, "\n", &saveptr))) return NULL;

        // Ignore starting whitespaces
        while(!strncmp(req->arg2, " ", 1)) req->arg2++;

        // Check if size is correct
        if(atoi(req->arg1) > strlen(req->arg2)) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "REA", 3)) {
        req->op = REA;
            
        // Check if "FD" token is present
        if(!(token_fd = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(strncmp(token_fd, "FD", 2)) return NULL;

        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;

        // Check if "SIZE" token is present
        if(!(token_size = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(strncmp(token_size, "SIZE", 4)) return NULL;

        if(!(req->arg1 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "CLO", 3)) {
        req->op = CLO;
        
        // Check if "FD" token is present
        if(!(token_fd = strtok_r(NULL, " ", &saveptr))) return NULL;
        if(strncmp(token_fd, "FD", 2)) return NULL;

        if(!(req->arg0 = strtok_r(NULL, " ", &saveptr))) return NULL;
        
        return req;
        
    } else if (!strncmp(opcode, "BYE", 3)){
        req->op = BYE;
        
        return req;
    
    } else return NULL;
}

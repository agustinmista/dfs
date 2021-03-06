#include "Common.h"
#include "ClientHandler.h"

#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"

#define SEND2CLIENT(fmt,...)    write(client_id, buffer_out, sprintf(buffer_out, fmt, ##__VA_ARGS__))
#define SEND_REQ(request)       mq_send(worker_queue, (char *) request, sizeof(Request), 1)
#define RECV_ANS()              mq_receive(client_queue, buffer_in, MSG_SIZE, NULL)


// Check for a valid filename.
int is_valid(char *s){
    if(strlen(s) == 1 && *s == '.') return 0;
    for(int i=0; i<strlen(s); i++) 
        if(!isalnum(s[i]) && s[i] != '.') return 0;
    return 1;
}


// Handle the communication between a client and his assigned worker.
void *handle_client(void *s){
    int identified = 0;
    int readed;
    
    char buffer_in[MSG_SIZE];
    char buffer_out[MSG_SIZE];

    Request *req = NULL;
    Reply *ans;
    
    // Parse session args
    Session *session = (Session *) s;
    int client_id      = session->client_id;
	mqd_t worker_queue = session->worker_queue;
	mqd_t client_queue = *(session->client_queue);
	
	
    // Client handle loop
    while(1){
        
        SEND2CLIENT(COLOR_YELLOW "dfs> " COLOR_RESET);
        
        if ((readed = read(client_id, buffer_in, MSG_SIZE)) <= 0){
            #if defined(DEBUG) || defined(DEBUG_REQUEST)
            printf("DFS_SERVER: Client [id: %d] disconected.\n", client_id);
            #endif
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
                SEND2CLIENT(COLOR_RED "ERROR: BAD COMMAND\n" COLOR_RESET);
            else if(!strncmp(buffer_in, "CON", 3))
                SEND2CLIENT(COLOR_RED "ERROR: ALREADY IDENTIFIED\n" COLOR_RESET);    
            else if(!(req = parseRequest(session, buffer_in)))
                SEND2CLIENT(COLOR_RED "ERROR: BAD COMMAND\n" COLOR_RESET);
            
            // If the parser has generated a request, send it, wait for response and print results
            if(req){
                    
                if(SEND_REQ(req) != 0) ERROR("DFS_SERVER: Error sending request to worker.\n");
                
                // Workers doing their job...
                
                if((readed = RECV_ANS()) == -1) ERROR("DFS_SERVER: Error receiving answer from worker.\n");
				ans = (Reply *)buffer_in;
                
                switch(ans->err){
                    // If no errors, print ok + extra data depending on command
                    case NONE:
                        if (req->op == LSD) { 
                            SEND2CLIENT(COLOR_GREEN "OK: " COLOR_RESET);
                            if (!strncmp(ans->answer, "", 1)) 
                                SEND2CLIENT("NO FILES\n");
                            else 
                                SEND2CLIENT("%s\n", ans->answer);
                        } else if (req->op == OPN || req->op == REA) { 
                            SEND2CLIENT(COLOR_GREEN "OK: " COLOR_RESET);
                            SEND2CLIENT(COLOR_RESET "%s\n", ans->answer);
                        } else if (req->op == BYE) {
                            SEND2CLIENT(COLOR_GREEN "BYE!\n" COLOR_RESET);
                            identified = !identified;
                        } else    
                            SEND2CLIENT(COLOR_GREEN "OK\n" COLOR_RESET);
                        break;
                        
                    // Otherwise, print error
                    case BAD_FD:     SEND2CLIENT(COLOR_RED "ERROR: BAD FD\n" COLOR_RESET);                  break;
                    case BAD_ARG:    SEND2CLIENT(COLOR_RED "ERROR: BAD ARG\n" COLOR_RESET);                 break;
                    case F_OPEN:     SEND2CLIENT(COLOR_RED "ERROR: FILE ALREADY OPEN\n" COLOR_RESET);       break;                    
                    case F_CLOSED:   SEND2CLIENT(COLOR_RED "ERROR: FILE IS CLOSED\n" COLOR_RESET);          break;
                    case F_EXIST:    SEND2CLIENT(COLOR_RED "ERROR: FILE ALREADY EXIST\n" COLOR_RESET);      break;                    
                    case F_NOTEXIST: SEND2CLIENT(COLOR_RED "ERROR: FILE NOT EXIST\n" COLOR_RESET);          break;
                    case F_NOTSPACE: SEND2CLIENT(COLOR_RED "ERROR: FILE: NOT ENOUGH SPACE\n"  COLOR_RESET); break;
                    case F_TOOMANY:  SEND2CLIENT(COLOR_RED "ERROR: TOO MANY OPEN FILES\n" COLOR_RESET);     break;
                    case NOT_IMP:    SEND2CLIENT(COLOR_RED "ERROR: COMMAND NOT IMPLEMENTED\n" COLOR_RESET); break;
                }

            }
            
        } else if(!strncmp(buffer_in, "CON", 3)) {
            identified = 1;
            SEND2CLIENT(COLOR_GREEN "OK: " COLOR_RESET); 
            SEND2CLIENT("ID %d\n", client_id); 
        } else
            SEND2CLIENT(COLOR_RED "ERROR NOT IDENTIFIED\n" COLOR_RESET);
    }
    
    // Close message queue
    if(mq_close(client_queue) == -1) ERROR("DFS_SERVER: Error closing client message queue.\n");

    // Free everything
    if(req) free(req);
    // If ans is freed here it causes an error when a client window closes.
    // It tries to free it again and the server crashes.
    free(s);
    
    return NULL;
}



// Parse a string into a Request structure, return NULL if something went wrong.
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

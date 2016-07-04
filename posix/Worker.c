#include "Common.h"
#include "Worker.h"


// Message queues sending macros
#define SEND_ANS()          mq_send(*(request->client_queue), (char *) ans, sizeof(*ans), 1)
#define SEND_REQ_MAIN(req)  mq_send(wqueue[request->main_worker], (char *) req, sizeof(Request), 1);


struct mq_attr attr;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;


// Print a request, used if DEBUG_REQUEST is enabled
#ifdef DEBUG_REQUEST
void print_request(int wid, Request *r) {
    printf("DFS_SERVER: New Request: [worker: %d] [id: %d] [op: %d] [external: %d] [arg0:'%s'] [arg1:'%s\'] [arg2:'%s']\n",
           wid, r->client_id, r->op, r->external, r->arg0, r->arg1, r->arg2);
}
#endif

// Reply filling shortcut
void fill_reply(Reply *ans, Error werror, char *wreply) {
	ans->err = werror;
	ans->answer = wreply;
}

// Request filling shortcut
void fill_request(Request *int_req, Operation op, int external,
                         int main_worker,  char *arg0, char *arg1,
                         char *arg2, int client_id, mqd_t *client_queue) {
    int_req->op = op;
   	int_req->external = external;
    int_req->main_worker = main_worker;
    int_req->arg0 = arg0;
    int_req->arg1 = arg1;
    int_req->arg2 = arg2;
    int_req->client_id = client_id;
    int_req->client_queue = client_queue;            
}


// Create a new file, return NULL if something went wrong
File *create_file () {
    File *newFile;
    
	if (!(newFile = malloc(sizeof(File))))
        return NULL;
    
	if (!(newFile->name = malloc(F_NAME_SIZE))){
        free(newFile);
        return NULL;
    }
    
	if (!(newFile->content = malloc(F_CONTENT_SIZE))) {
		free(newFile->name);
		free(newFile);
        return NULL;
	}

	return newFile;
}


// Send a request to the next worker in the ring
int send_next_worker(int id, mqd_t *queue, Request *req){
    return mq_send(queue[(id+1)%N_WORKERS], (char *)req, sizeof(*req), 0);
}


// Check for an open file, return -1 if its closed, 
// -2 if inexistent, or the opener id oterwise
int is_open(File *files, char *nombre){//Ok
	while (files) {
		if (!strcmp(files->name, nombre)) 
            return (files->open);
		files = files->next;
	}
	return -2;
}


// Close a file, return 0 if success, -1 if already closed,
// or -2 if inexistent
int close_file(File *files, FDPool *fd_pool, int fd){ 
	while (files) {
		if ((files->fd) == fd) {
			if((files->open) == -1)
				return -1;
			else {
				files->open = -1;
				files->cursor = 0;
                files->fd = freeFD(fd_pool, files->fd);
				return 0;
			}
		}
		files = files->next;
	}
	return -2;
}


// Open a file, return -1 if already opened, -2 if inexistent, -3 if the pool is full,
// otherwise, return the file descriptor associated to it
int open_file(int client_id, File *files, FDPool *fd_pool, char *nombre){ //Ok

	while (files) {
        
	   if (!strcmp(files->name, nombre)) {
           
           if ((files->open) != -1)	return -1;

           if ((files->fd = newFD(fd_pool)) < 0) return -3;

           files->open = client_id;
           return (files->fd);
        }
        
		files = files->next;
	}
    
	return -2;
}


// Close all files in the files list
void close_all_files(File *files, FDPool *fd_pool){ //OK
	
	while (files) {
		if (files->open) {
			files->open = -1;
			files->cursor = 0;
			files->fd = freeFD(fd_pool, files->fd);
		}	
		files = files->next;
	}
}


// List all files allocated in a single worker
char *list_files(File *files){
	if (!files) return "";
    
    int i;
    File *tmp = files;
    
    for (i=0; tmp; tmp = tmp->next) i++;
    
    char *lista = calloc((F_NAME_SIZE+1)*i, sizeof(char));

    while (files) {
        strcat(lista, files->name);
		if (files->next) strcat(lista, " ");
        files=files->next;
    }
    return lista;
}


// Main worker function
void *worker(void *w_info) {
	
	File *files, *files_init = NULL;
    Request *request;
    int readed;
    
    // Allocate space for the communication structures
    char buffer_in[MSG_SIZE+1];
    Request *intern_request = malloc(sizeof(Request));
	Reply *ans = malloc(sizeof(Reply));
	
    // Parse worker args
    int wid         = ((Worker_Info *)w_info)->id;
    mqd_t *wqueue   = ((Worker_Info *)w_info)->queue;
    FDPool *fd_pool = ((Worker_Info *)w_info)->fd_pool;
    free(w_info);	   
    
    
    // Enter working loop
    while (1) {
        
        // Wait for incoming requests
        if ((readed = mq_receive(wqueue[wid], buffer_in, MSG_SIZE+1, NULL)) >= 0) {
            request = (Request *) buffer_in;
            
            #ifdef DEBUG_REQUEST
            print_request(wid, request);
            #endif
    
            files = files_init;
            
            // Evaluate the requested operation
            switch (request->op) {
                
            //---------------------------------------
            //                 LSD
            //---------------------------------------
            case LSD:
                
                // The request comes from a client
                if (request->external) {
                    char *tmp = list_files(files);

                    if (N_WORKERS > 1) {
                        intern_request = request;
                        intern_request->external = 0;
                        intern_request->arg0 = tmp;
                        send_next_worker(wid, wqueue, intern_request);
                        } else {
                            fill_reply(ans, NONE, tmp);
                            SEND_ANS();
                        }
                // The request come from another worker, and this worker is not
                // the one associated to the client
                } else if (!(request->external) && ((request->main_worker) != wid)) {
                    char *tmp = list_files(files);
                    intern_request = request;
                    intern_request->external = 0;
                    
                    if (strlen(tmp)>0 && (strlen(request->arg0)>0)) {
                        char *aux;
                        asprintf(&aux, "%s %s", request->arg0, tmp);
                        intern_request->arg0 = aux;
                    } else if (strlen(tmp)>0) {
                        intern_request->arg0 = tmp;
                    }
                    send_next_worker(wid, wqueue, intern_request);
               
                // The request has completed the ring, 
                // send back the answer to the client
                } else {
                    fill_reply(ans, NONE, request->arg0);
                    SEND_ANS();
                }
                break;

                
            //---------------------------------------
            //                DEL 
            //---------------------------------------
            case DEL:
                
                // The request has completed the ring, 
                // send back the answer to the client
                if (!(request->external) && ((request->main_worker) == wid)) {
                    
                    if (strcmp(request->arg1, "-1") == 0)
                        fill_reply(ans, F_OPEN, NULL);
                    else if (strcmp(request->arg1, "-2") == 0)
                        fill_reply(ans, F_NOTEXIST, NULL);
                    else {
                        #if defined(DEBUG) || defined(DEBUG_REQUEST)
                        printf("DFS_SERVER: File deleted: [name: %s]\n", request->arg0);
                        #endif
                        fill_reply(ans, NONE, NULL);
                    }

                    SEND_ANS();	
                
                // The request is either external, or internal and the ring 
                // communication still in process
                } else {

                    int status = -2;
                    File *prev = NULL;
                    
                    // Search for the file
                    while (files) {
                        if (!strcmp(files->name, request->arg0)) {
                            
                            if (files->open == -1) {
                                
                                if (!prev) files_init = files->next;
                                else prev->next = files->next;
                                
                                free(files);
                                status = 0;
                                break;
                            } else
                                status = -1;
                            }

                            prev = files;
                            files = files->next;
                    }
                        
                    // If the request is external, if the file was in this worker
                    // then send back the answer to the client
                    if (request->external) {

                        if (status == -2) {
                            if (N_WORKERS > 1) {
                                intern_request = request;
                                intern_request->external = 0;
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, F_NOTEXIST, NULL);
                                SEND_ANS();
                            }

                            } else {
                                if (status == -1) fill_reply(ans, F_OPEN, NULL);
                                else {
                                    #if defined(DEBUG) || defined(DEBUG_REQUEST)
                                    printf("DFS_SERVER: File deleted: [name: %s]\n", request->arg0);
                                    #endif
                                    fill_reply(ans, NONE, NULL);
                                }

                                SEND_ANS();
                            }
                        
                        // The request is internal, if file was in this worker,
                        // the send answer direclty to the main worker, 
                        // otherwise, let the next worker find the file
                        } else {
                            intern_request = request;
                            intern_request->external = 0;
                            if (status == -2) {
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                if (status == -1)
                                    intern_request->arg1 = "-1";
                                else
                                    intern_request->arg1 = "0";

                                SEND_REQ_MAIN(intern_request);
                            }
                        }
                    }
                    break; 

            //---------------------------------------
            //                CRE 
            //---------------------------------------
            case CRE:
                
                // The request is either external, or has completed the ring
                if ((request->main_worker) == wid) {
                
                    // The request has completed the ring, and the filename was not
                    // found, if is not present here, the worker can create it
                    if ((!(request->external) && (strcmp(request->arg1, "0") == 0)) || (N_WORKERS == 1)) {	
                        
                        
                        if (is_open(files, request->arg0) == -2) {

                            File *new = create_file();
                            if (new) {
                                memcpy(new -> name, request->arg0, F_NAME_SIZE-1);
                                new -> fd = -1;
                                new -> open = -1;
                                (new->content)[0] = '\0';
                                new -> cursor = 0;
                                new -> size = 0; 
                                new -> next = files_init; 
                            }	
                            files_init = new;
                            #if defined(DEBUG) || defined(DEBUG_REQUEST)
                            printf("DFS_SERVER: New file created: [name: %s] [fd: %d]\n", new->name, new->fd);
                            #endif
                            fill_reply(ans, NONE, NULL);	

                        } else fill_reply(ans, F_EXIST, NULL);
                        
                        SEND_ANS();

                    // The request is internal and the filename has been found somewhere
                    } else if (!(request->external) && (strcmp(request->arg1, "-1") == 0)) {
                        fill_reply(ans, F_EXIST, NULL);
                        SEND_ANS();
                    
                    // The request is external, if the file exists here send back the error
                    // otherwise, check it in the other workers
                    } else {
                        if (is_open(files, request->arg0) == -2) {
                            intern_request = request;
                            intern_request->external = 0;
                            intern_request->arg1 = "0";
                            send_next_worker(wid, wqueue, intern_request);
                        } else {
                            fill_reply(ans, F_EXIST, NULL);
                            SEND_ANS();
                        }
                    }

                // The request is internal, check the filename here
                } else {
                    if (is_open(files, request->arg0) == -2) {
                        intern_request=request;
                        intern_request->external = 0;
                        intern_request->arg1 = "0";
                        send_next_worker(wid, wqueue, intern_request);
                    } else {
                        intern_request=request;
                        intern_request->external = 0;
                        intern_request->arg1 = "-1";
                        SEND_REQ_MAIN(intern_request);
                    }	
                }			

                break; 

            //---------------------------------------
            //                OPN
            //---------------------------------------
            case OPN:

                // The request is internal and has completed
                // the ring, send back the answer
                if (!(request->external) && ((request->main_worker) == wid)) {
                    if(strcmp(request->arg1, "-1") == 0)
                        fill_reply(ans, F_OPEN, NULL);
                    else if (strcmp(request->arg1, "-2") == 0)
                        fill_reply(ans, F_NOTEXIST, NULL);                        
                    else
                        fill_reply(ans, NONE, request->arg1);

                    SEND_ANS();	
                    
                
                // The request is either external, or internal and the ring
                // is still in process, check for the file here
                } else {

                    int tmp = open_file(request->client_id, files, fd_pool, request->arg0);

                    // The request is external, try to open the file and send the request, 
                    // or start the ring communication
                    if (request->external) {
                        if (tmp == -2) {
                            if (N_WORKERS > 1) {
                                intern_request = request;
                                intern_request->external = 0;
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, F_NOTEXIST, NULL);
                                SEND_ANS();
                            }
                        } else {
                            if (tmp == -1)
                                fill_reply(ans, F_OPEN, NULL);
                            else if(tmp == -3)
                                fill_reply(ans, F_TOOMANY, NULL);
                            else {
                                char *aux;
                                asprintf(&aux, "FD %d", tmp);
                                fill_reply(ans, NONE, aux);
                            }
                            SEND_ANS();
                        }

                    // The request is internal, if the file is here, send answer to 
                    // main worker, otherwise, keep processing the ring communication
                    } else {

                        intern_request = request;
                        intern_request->external = 0;
                         
                        if (tmp == -2) {
                            intern_request->arg1 = "-2";
                            send_next_worker(wid, wqueue, intern_request);
                             
                        } else {
                            if (tmp == -1) {
                                intern_request->arg1 = "-1";
                            } else if (tmp == -1) {
                                intern_request->arg1 = "-3";
                            } else {
                                char *aux;
                                asprintf(&aux, "FD %d", tmp);
                                intern_request->arg1 = aux;
                            }
                            #if defined(DEBUG) || defined(DEBUG_REQUEST)
                            printf("DFS_SERVER: File opened: [name: %s] [fd: %d]\n", request->arg0, tmp);
                            #endif
                            SEND_REQ_MAIN(intern_request);
                            
                        }
                            
                        
                    }
                    
                }
                break; 

            //---------------------------------------
            //                WRT 
            //---------------------------------------
            case WRT:
                
                // The request is internal and has completed
                // the ring, send back the answer
                if (!(request->external) && ((request->main_worker) == wid)) {
                    if (strcmp(request->arg0, "-1") == 0)
                        fill_reply(ans, F_OPEN, NULL);
                    else if (strcmp(request->arg0, "-2") == 0)
                        fill_reply(ans, BAD_FD, NULL);
                    else if (strcmp(request->arg0, "-3") == 0)
                        fill_reply(ans, F_NOTSPACE, NULL);
                    else
                        fill_reply(ans, NONE, request->arg1);

                    SEND_ANS();	
                
                // The request is either external, or internal and the ring
                // is still in process, check for the file here
                } else {

                    int status = -2;

                    while(files){
                        if (files->fd == atoi(request->arg0)) { 
                            status = -1;
                            if (files->open == request->client_id) {
                                int sz = strlen(files->content);
                                if ((sz+atoi(request->arg1)+2)<(F_CONTENT_SIZE)) {
                                    if (sz > 0)
                                        strcat(files->content, " ");
                                        strcat(files->content, request->arg2);
                                        files->size = strlen(files->content);
                                        #if defined(DEBUG) || defined(DEBUG_REQUEST)
                                        printf("DFS_SERVER: Content updated: [name: %s] [content: %s]\n", files->name, files->content);
                                        #endif
                                        status = 0;
                                } else
                                    status = -3;
                            }
                            break;
                        }
                        files=files->next;
                    }
                    
                    // The request is external, if the file is not here, start
                    // the ring communication, otherwise, send back the answer
                    if (request->external) {
                        
                        if (status == -2) {
                            if (N_WORKERS > 1) {
                                intern_request = request;
                                intern_request->external = 0;
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, BAD_FD, NULL);
                                SEND_ANS();
                            }

                        } else {
                            if (status == -1)
                                fill_reply(ans, F_OPEN, NULL);
                            else if (status == -3)
                                fill_reply(ans, F_NOTSPACE, NULL);
                            else
                                fill_reply(ans, NONE, NULL);
                            
                            SEND_ANS();
                        }

                    // The request is internal, if the file is here, send back the
                    // answer to the main worker, otherwise, keep the ring 
                    // communication process running
                    } else {
                        if (status == -2) {
                            
                            if ((wid == request->main_worker-1)||((wid == N_WORKERS-1) &&(request->main_worker == 0)))
                                fill_request(intern_request, WRT, 0, request->main_worker, "-2", NULL, NULL, request->client_id, request->client_queue);
                            else {
                                intern_request = request;
                                intern_request->external = 0;
                            }
                            
                            send_next_worker(wid, wqueue, intern_request);
                        
                        } else {
                        
                            if (status == -1)
                                fill_request(intern_request, WRT, 0, request->main_worker, "-1", NULL, NULL, request->client_id, request->client_queue);
                            else if (status == -3)
                                fill_request(intern_request, WRT, 0, request->main_worker, "-3", NULL, NULL, request->client_id, request->client_queue);
                            else
                                fill_request(intern_request, WRT, 0, request->main_worker, "0", NULL, NULL, request->client_id, request->client_queue);
                            
                            SEND_REQ_MAIN(intern_request);
                        }
                    }
                }
                break; 

            //---------------------------------------
            //                REA 
            //---------------------------------------
            case REA:

                // The request is internal and has completed
                // the ring, send back the answer
                if (!(request->external) && ((request->main_worker) == wid)) {
                    if (strcmp(request->arg0, "-1") == 0)
                        fill_reply(ans, F_OPEN, NULL);
                    else if (strcmp(request->arg0, "-2") == 0)
                        fill_reply(ans, BAD_FD, NULL);
                    else if (strcmp(request->arg0, "-3") == 0)
                        fill_reply(ans, NONE, "");
                    else
                        fill_reply(ans, NONE, request->arg1);
                
                    SEND_ANS();	
                
                
                // The request is either external, or internal and the ring
                // is still in process, check for the file here
                } else {
                     int status = -2;
                     char *ret;
                     
                     while(files){
                        if (files->fd == atoi(request->arg0)){ 
                            status = -1;
                            if (files->open == request->client_id){
                                status = -3;
                                int sz1 = atoi(request->arg1);
                                if ((sz1 > 0) && (files->cursor < files->size)){
                                    int sz2 = MIN(sz1, (files->size-1) - files->cursor);
                                    ret=calloc(sz2+1, sizeof(char));
                                    int i=0;
                                    while((sz1 > 0) && (files->cursor < files->size)){
                                        ret[i] = (files->content)[files->cursor];
                                        i++;
                                        files->cursor++;
                                        sz1--;
                                    }
                                    status = 0;
                                }
                            }
                            break;
                        }
                        files=files->next;
                    }
                    
                    // The request is external, if file is not here, start the
                    // ring communication, otherwise send back the answer
                    if (request->external){
                        if (status == -2){
                            if (N_WORKERS > 1){
                                intern_request = request;
                                intern_request->external = 0;
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, BAD_FD, NULL);
                                SEND_ANS();
                            }
                        } else {
                            if (status == -1)
                                fill_reply(ans, F_OPEN, NULL);
                            else if (status == -3)
                                fill_reply(ans, NONE, "");
                            else
                                fill_reply(ans, NONE, ret);
                             
                            SEND_ANS();
                        }
                    
                    
                    // The request is internal, if the file is here, send back the
                    // answer to the main worker, otherwise, keep the ring 
                    // communication process running
                    } else {
                        if (status == -2){
                            if ((wid == request->main_worker-1)||((wid == N_WORKERS-1) &&(request->main_worker == 0)))
                                fill_request(intern_request, WRT, 0, request->main_worker, "-2", NULL, NULL, request->client_id, request->client_queue);
                            else {
                                intern_request = request;
                                intern_request->external = 0;
                            }
                            send_next_worker(wid, wqueue, intern_request);
                        } else {
                            if (status == -1) //Aca hay que cambiar mucho el request.. por eso es mejor hacerlo con fill_request
                                fill_request(intern_request, WRT, 0, request->main_worker, "-1", NULL, NULL, request->client_id, request->client_queue);
                            else if (status == -3)
                                fill_request(intern_request, WRT, 0, request->main_worker, "-3", NULL, NULL, request->client_id, request->client_queue);
                            else
                                fill_request(intern_request, WRT, 0, request->main_worker, "0", ret, NULL, request->client_id, request->client_queue);
                           
                            SEND_REQ_MAIN(intern_request);
                        }
                    }
                }
                break; 

            //---------------------------------------
            //                CLO 
            //---------------------------------------
            case CLO:

                // The request is internal and has completed the ring,
                // send back the answer
                if (!(request->external) && ((request->main_worker) == wid)) {
                    if (strcmp(request->arg1, "-1") == 0)
                        fill_reply(ans, F_CLOSED, NULL);
                    else if (strcmp(request->arg1, "-2") == 0)
                        fill_reply(ans, BAD_FD, NULL);
                    else
                        fill_reply(ans, NONE, NULL);
                      
                   SEND_ANS();	
                
                
                // The request is either external, or internal and the ring
                // is still in process, check for the file here
                } else {
                
                    int status = close_file(files, fd_pool, atoi(request->arg0));

                    // The request is external, if file is not here, start the
                    // ring communication, otherwise send back the answer
                    if (request->external){
                        if (status == -2){
                            if (N_WORKERS > 1){
                                intern_request = request;
                                intern_request->external = 0;
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, BAD_FD, NULL);
                                SEND_ANS();
                            }
                        } else {
                            if (status == -1)
                                fill_reply(ans, F_CLOSED, NULL);
                            else{
                                fill_reply(ans, NONE, NULL);
                            }
                            SEND_ANS();
                        }
                    
                    
                    // The request is internal, if the file is here, send back the
                    // answer to the main worker, otherwise, keep the ring 
                    // communication process running
                    } else {
                        intern_request = request;
                        intern_request->external = 0;
                        if (status == -2) {
                            intern_request->arg1 = "-2";
                            send_next_worker(wid, wqueue, intern_request);
                        } else {
                            if (status == -1)
                                intern_request->arg1 = "-1";
                            else{
                                intern_request->arg1 = "0";
                            }
                            
                            SEND_REQ_MAIN(intern_request);
                        }
                    }
                }
                break;

            //---------------------------------------
            //                BYE 
            //---------------------------------------
            case BYE:

                // This worker is the associated with the request
                // if the request has completed the ring, close all files
                // and send answer back to the client
                if (request->main_worker == wid){
                    if (!(request->external) || N_WORKERS == 1){
                        close_all_files(files, fd_pool);
                        fill_reply(ans, NONE, NULL);
                        SEND_ANS();
                    } else {
                        intern_request = request;
                        intern_request->external = 0;
                        send_next_worker(wid, wqueue, intern_request);
                    }

                // This worker in not the associated with the request,
                // close all files, and send the request to the next worker
                } else {
                    close_all_files(files, fd_pool);
                    send_next_worker(wid, wqueue, request);
                }

                break;
            }
        }
	}

    mq_close(wqueue[wid]);
    return 0;
}


// Initialize the workers
int init_workers(){
    
    struct mq_attr attr;
    attr.mq_flags = 0;  
    attr.mq_maxmsg = MAX_MESSAGES;  
    attr.mq_msgsize = MSG_SIZE;  
    attr.mq_curmsgs = 0;
    
    for(int i = 0; i<N_WORKERS; i++){
        
        // Instance the worker message queue
        char *worker_name;
        asprintf(&worker_name, "/w%d", i);
        
        if ((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0644, &attr)) == (mqd_t) -1)
            ERROR("\nDFS_SERVER: Error opening message queue for workers\n");
        
        // Create the global file descriptors pool
        FDPool *fd_pool = createFDPool(MAX_OPEN_FILES);
        
        // Create the worker session
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues;
        newWorker->fd_pool = fd_pool;
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

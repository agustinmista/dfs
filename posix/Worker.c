#include "Common.h"
#include "Worker.h"


#define SEND_ANS()          mq_send(*(request->client_queue), (char *) ans, sizeof(*ans), 1)
#define SEND_REQ_MAIN(req)  mq_send(wqueue[request->main_worker], (char *) req, sizeof(Request), 1);

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define DEBUG_REQUEST 1

struct mq_attr attr;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;


void print_request(int wid, Request *r){    //Ok
    printf("DFS_SERVER: New Request: [worker: %d] [id: %d] [op: %d] [external: %d] [arg0:'%s'] [arg1:'%s\'] [arg2:'%s']\n",
           wid, r->client_id, r->op, r->external, r->arg0, r->arg1, r->arg2);
}

void fill_reply(Reply *ans, Error werror, char *wreply){ //Ok
	ans->err = werror;
	ans->answer = wreply;
}

void fill_request (Request *int_req, Operation op, int external, int main_worker,
                   char *arg0, char *arg1, char *arg2, int client_id, mqd_t *client_queue){
    int_req->op = op;
   	int_req->external = external;
    int_req->main_worker = main_worker;
    int_req->arg0 = arg0;
    int_req->arg1 = arg1;
    int_req->arg2 = arg2;
    int_req->client_id = client_id;
    int_req->client_queue = client_queue;            
}

File *create_file(){	//Ok
    File *newFile;
    
	if (!(newFile = malloc(sizeof(File))))
        return NULL;
    
	if (!(newFile->name = malloc(F_NAME_SIZE))){
        free(newFile);
        return NULL;
    }
    
	if(!(newFile->content = malloc(F_CONTENT_SIZE))){
		free(newFile->name);
		free(newFile);
        return NULL;
	}

	return newFile;
}

int send_next_worker(int id, mqd_t *queue, Request *req){ //Ok
    return mq_send(queue[ (id+1)%N_WORKERS ], (char *)req, sizeof(*req), 0);
}

 //-2 no existe, -1 existe cerrado, id existe abierto por id 
int is_open(File *files, char *nombre){//Ok
	while(files){
		if(!strcmp(files->name, nombre)) 
            return (files->open);
		files = files->next;
	}
	return -2;
}

//0 si se pudo cerrar, -1 si ya estaba cerrado, -2 si no existe
int close_file(File *files, FDPool *fd_pool, int fd){ 
	while(files){
		if((files->fd) == fd){
			if((files->open) == -1)
				return -1;
			else{
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

//fd si se pudo abrir, -1 si ya estaba abierto, -2 si no existe, -3 si ya no hay lugar en el pool
int open_file(int client_id, File *files, FDPool *fd_pool, char *nombre){ //Ok

	while(files){
        
	   if(!strcmp(files->name, nombre)) {
           
           if((files->open) != -1)	return -1;

           if((files->fd = newFD(fd_pool)) < 0) return -3;

           files->open = client_id;
           return (files->fd);
        }
        
		files = files->next;
	}
    
	return -2;
}

void close_all_files(File *files, FDPool *fd_pool){ //OK
	
	while(files){
		if(files->open){
			files->open = -1;
			files->cursor = 0;
			files->fd = freeFD(fd_pool, files->fd);
		}	
		files = files->next;
	}
}

char *list_files(File *files){ //OK
	if(!files) return "";
    
    int i;
    File *tmp = files;
    
    for(i=0; tmp; tmp = tmp->next) i++;
    
    char *lista = calloc((F_NAME_SIZE+1)*i, sizeof(char));

    while(files){
        strcat(lista, files->name);
		if(files->next) strcat(lista, " ");
        files=files->next;
    }
    return lista;
}

void *worker(void *w_info){
	
	File *files;
	File *files_init = NULL;
    
    char buffer_in[MSG_SIZE+1];
    int readed;
    
    Request *request;
    Request *intern_request = malloc(sizeof(Request));
	Reply *ans = malloc(sizeof(Reply));
	
    // Parse worker args
    int wid         = ((Worker_Info *)w_info)->id;
    mqd_t *wqueue   = ((Worker_Info *)w_info)->queue;
    FDPool *fd_pool = ((Worker_Info *)w_info)->fd_pool;
    free(w_info);	   
    
    // Loop infinito hasta que resolvamos las operaciones, vamos metiendolas adentro 
    // a medida que funcionan
    while(1){
        if((readed = mq_receive(wqueue[wid], buffer_in, MSG_SIZE+1, NULL)) >= 0){
            request = (Request *) buffer_in;
            
            if(DEBUG_REQUEST) print_request(wid, request);
    
            files = files_init;

            switch(request->op){

                case LSD:

                    if(request->external){ //hacer free
                        char *tmp = list_files(files);

                        if(N_WORKERS > 1){
                            intern_request = request;
                            intern_request->external = 0;
                            intern_request->arg0 = tmp;
                            send_next_worker(wid, wqueue, intern_request);
                        } else {
                            fill_reply(ans, NONE, tmp);
                            SEND_ANS();
                        }
                    } else if(!(request->external) && ((request->main_worker) != wid)){
                        char *tmp = list_files(files);
                        intern_request = request;
                        intern_request->external = 0;
                        if(strlen(tmp)>0 && (strlen(request->arg0)>0)){
                            char *aux;
                            asprintf(&aux, "%s %s", request->arg0, tmp);
                            intern_request->arg0 = aux;
                        } else if(strlen(tmp)>0) {
                            intern_request->arg0 = tmp;
                        }
                        send_next_worker(wid, wqueue, intern_request);
                    } else {
                        fill_reply(ans, NONE, request->arg0);
                        SEND_ANS();
                    }
                    break;

                case DEL:
                    if(!(request->external) && ((request->main_worker) == wid)){
                        if(strcmp(request->arg1, "-1") == 0)
                            fill_reply(ans, F_OPEN, NULL);
                        else if (strcmp(request->arg1, "-2") == 0)
                            fill_reply(ans, F_NOTEXIST, NULL);
                        else{
                            //BORRAR
                            printf("DFS_SERVER: File deleted: [name: %s]\n", request->arg0);
                            //
                            fill_reply(ans, NONE, NULL);
                        }
                        SEND_ANS();	
                    } else {

                        int status = -2;
                        File *prev = NULL;

                        while(files){
                            if(!strcmp(files->name, request->arg0)){
                                if(files->open == -1){
                                    if(!prev)
                                        files_init = files->next;
                                    else
                                        prev->next = files->next;
                                    free(files);
                                    status = 0;
                                    break;
                                } else
                                    status = -1;
                            }
                            prev = files;
                            files = files->next;
                        }

                        if(request->external){
                            if(status == -2){
                                if(N_WORKERS > 1){
                                    intern_request = request;
                                    intern_request->external = 0;
                                    intern_request->arg1 = "-2";
                                    send_next_worker(wid, wqueue, intern_request);
                                } else {
                                    fill_reply(ans, F_NOTEXIST, NULL);
                                    SEND_ANS();
                                }
                            } else {
                                if(status == -1)
                                    fill_reply(ans, F_OPEN, NULL);
                                else{
                                    //BORRAR
                                    printf("DFS_SERVER: File deleted: [name: %s]\n", request->arg0);
                                    //
                                    fill_reply(ans, NONE, NULL);
                                }
                                SEND_ANS();
                            }
                        } else {
                            intern_request = request;
                            intern_request->external = 0;
                            if(status == -2){
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                if(status == -1)
                                    intern_request->arg1 = "-1";
                                else
                                    intern_request->arg1 = "0";

                                SEND_REQ_MAIN(intern_request);
                            }
                        }
                    }
                    break; 

                case CRE:
                    if((request->main_worker) == wid){
                        if((!(request->external) && (strcmp(request->arg1, "0") == 0)) || (N_WORKERS == 1)){	
                            if(is_open(files, request->arg0) == -2){

                                File *new = create_file();
                                if(new){
                                    memcpy(new -> name, request->arg0, F_NAME_SIZE-1);
                                    new -> fd = -1;
                                    new -> open = -1;
                                    (new->content)[0] = '\0'; //Ver--
                                    new -> cursor = 0;
                                    new -> size = 0; 
                                    new -> next = files_init; 
                                }	
                                files_init = new;

                                //BORRAR
                                printf("DFS_SERVER: New file created: [name: %s] [fd: %d]\n", new->name, new->fd);
                                //	

                                fill_reply(ans, NONE, NULL);	

                            } else fill_reply(ans, F_EXIST, NULL);

                            SEND_ANS();

                        } else if(!(request->external) && (strcmp(request->arg1, "-1") == 0)){
                            fill_reply(ans, F_EXIST, NULL);
                            SEND_ANS();

                        } else {

                            if(is_open(files, request->arg0) == -2){
                                intern_request = request;
                                intern_request->external = 0;
                                intern_request->arg1 = "0";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                fill_reply(ans, F_EXIST, NULL);
                                SEND_ANS();
                            }
                        }
                    } else {

                        if(is_open(files, request->arg0) == -2){
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

                case OPN:
                    if(!(request->external) && ((request->main_worker) == wid)){
                        if(strcmp(request->arg1, "-1") == 0)
                            fill_reply(ans, F_OPEN, NULL);
                        else if (strcmp(request->arg1, "-2") == 0)
                            fill_reply(ans, F_NOTEXIST, NULL);                        
                        else
                            fill_reply(ans, NONE, request->arg1);

                        SEND_ANS();	
                    } else {

                        int tmp = open_file(request->client_id, files, fd_pool, request->arg0);

                        if(request->external){
                            if(tmp == -2){
                                if(N_WORKERS > 1){
                                    intern_request = request;
                                    intern_request->external = 0;
                                    intern_request->arg1 = "-2";
                                    send_next_worker(wid, wqueue, intern_request);
                                } else {
                                    fill_reply(ans, F_NOTEXIST, NULL);
                                    SEND_ANS();
                                }
                            } else {
                                if(tmp == -1)
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
                        } else {
                            intern_request = request;
                            intern_request->external = 0;
                            
                            if(tmp == -2){
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                                
                            } else {
                                if(tmp == -1){
                                    intern_request->arg1 = "-1";
                                } else if(tmp == -1){
                                    intern_request->arg1 = "-3";
                                } else {
                                    char *aux;
                                    asprintf(&aux, "FD %d", tmp);
                                    intern_request->arg1 = aux;
                                }
                                printf("DFS_SERVER: File opened: [name: %s] [fd: %d]\n", request->arg0, tmp);
                                SEND_REQ_MAIN(intern_request);
                            }
                            
                        }
                    }
                    break; 

                case WRT:

                    if(!(request->external) && ((request->main_worker) == wid)){
                        if(strcmp(request->arg0, "-1") == 0)
                            fill_reply(ans, F_OPEN, NULL);
                        else if (strcmp(request->arg0, "-2") == 0)
                            fill_reply(ans, BAD_FD, NULL);
                        else if (strcmp(request->arg0, "-3") == 0)
                            fill_reply(ans, F_NOTSPACE, NULL);
                        else
                            fill_reply(ans, NONE, request->arg1);

                        SEND_ANS();	
                    } else {

                        int status = -2;

                        while(files){
                            if(files->fd == atoi(request->arg0)){ 
                                status = -1;
                                if(files->open == request->client_id){
                                    int sz = strlen(files->content);
                                    if((sz+atoi(request->arg1)+2)<(F_CONTENT_SIZE)){
                                        if(sz > 0)
                                            strcat(files->content, " ");
                                        strcat(files->content, request->arg2);
                                        files->size = strlen(files->content);
                                        printf("DFS_SERVER: Content updated: [name: %s] [content: %s]\n", files->name, files->content);
                                        status = 0;
                                    }else
                                        status = -3;
                                }
                                break;
                            }
                            files=files->next;
                        }
                        if(request->external){
                            if(status == -2){
                                if(N_WORKERS > 1){
                                    intern_request = request;
                                    intern_request->external = 0;
                                    send_next_worker(wid, wqueue, intern_request);
                                } else {
                                    fill_reply(ans, BAD_FD, NULL);
                                    SEND_ANS();
                                }
                            } else {
                                if(status == -1)
                                    fill_reply(ans, F_OPEN, NULL);
                                else if(status == -3)
                                    fill_reply(ans, F_NOTSPACE, NULL);
                                else
                                    fill_reply(ans, NONE, NULL);

                                SEND_ANS();
                            }
                        } else {
                            if(status == -2){
                                if((wid == request->main_worker-1)||((wid == N_WORKERS-1) &&(request->main_worker == 0)))
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-2", NULL, NULL, request->client_id, request->client_queue);
                                else {
                                    intern_request = request;
                                    intern_request->external = 0;
                                }
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                if(status == -1) //Aca hay que cambiar mucho el request.. por eso es mejor hacerlo con fill_request
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-1", NULL, NULL, request->client_id, request->client_queue);
                                else if(status == -3)
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-3", NULL, NULL, request->client_id, request->client_queue);
                                else
                                    fill_request(intern_request, WRT, 0, request->main_worker, "0", NULL, NULL, request->client_id, request->client_queue);

                                SEND_REQ_MAIN(intern_request);
                            }
                        }
                    }
                    break; 

                case REA:

                    if(!(request->external) && ((request->main_worker) == wid)){
                        if(strcmp(request->arg0, "-1") == 0)
                            fill_reply(ans, F_OPEN, NULL);
                        else if (strcmp(request->arg0, "-2") == 0)
                            fill_reply(ans, BAD_FD, NULL);
                        else if (strcmp(request->arg0, "-3") == 0)
                            fill_reply(ans, NONE, "");
                        else
                            fill_reply(ans, NONE, request->arg1);

                        SEND_ANS();	
                    } else {

                        int status = -2;
                        char *ret;

                        while(files){
                            if(files->fd == atoi(request->arg0)){ 
                                status = -1;
                                if(files->open == request->client_id){
                                    status = -3;
                                    int sz1 = atoi(request->arg1);
                                    if((sz1 > 0) && (files->cursor < files->size)){
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
                        if(request->external){
                            if(status == -2){
                                if(N_WORKERS > 1){
                                    intern_request = request;
                                    intern_request->external = 0;
                                    send_next_worker(wid, wqueue, intern_request);
                                } else {
                                    fill_reply(ans, BAD_FD, NULL);
                                    SEND_ANS();
                                }
                            } else {
                                if(status == -1)
                                    fill_reply(ans, F_OPEN, NULL);
                                else if(status == -3)
                                    fill_reply(ans, NONE, "");
                                else
                                    fill_reply(ans, NONE, ret);

                                SEND_ANS();
                            }
                        } else {
                            if(status == -2){
                                if((wid == request->main_worker-1)||((wid == N_WORKERS-1) &&(request->main_worker == 0)))
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-2", NULL, NULL, request->client_id, request->client_queue);
                                else {
                                    intern_request = request;
                                    intern_request->external = 0;
                                }
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                if(status == -1) //Aca hay que cambiar mucho el request.. por eso es mejor hacerlo con fill_request
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-1", NULL, NULL, request->client_id, request->client_queue);
                                else if(status == -3)
                                    fill_request(intern_request, WRT, 0, request->main_worker, "-3", NULL, NULL, request->client_id, request->client_queue);
                                else
                                    fill_request(intern_request, WRT, 0, request->main_worker, "0", ret, NULL, request->client_id, request->client_queue);

                                SEND_REQ_MAIN(intern_request);
                            }
                        }
                    }
                    break; 

                case CLO:
                    if(!(request->external) && ((request->main_worker) == wid)){
                        if(strcmp(request->arg1, "-1") == 0)
                            fill_reply(ans, F_CLOSED, NULL);
                        else if (strcmp(request->arg1, "-2") == 0)
                            fill_reply(ans, BAD_FD, NULL);
                        else
                            fill_reply(ans, NONE, NULL);
                        
                        SEND_ANS();	
                    } else {

                        int status = close_file(files, fd_pool, atoi(request->arg0));

                        if(request->external){
                            if(status == -2){
                                if(N_WORKERS > 1){
                                    intern_request = request;
                                    intern_request->external = 0;
                                    intern_request->arg1 = "-2";
                                    send_next_worker(wid, wqueue, intern_request);
                                } else {
                                    fill_reply(ans, BAD_FD, NULL);
                                    SEND_ANS();
                                }
                            } else {
                                if(status == -1)
                                    fill_reply(ans, F_CLOSED, NULL);
                                else{
                                    fill_reply(ans, NONE, NULL);
                                }
                                SEND_ANS();
                            }
                        } else {
                            intern_request = request;
                            intern_request->external = 0;
                            if(status == -2){
                                intern_request->arg1 = "-2";
                                send_next_worker(wid, wqueue, intern_request);
                            } else {
                                if(status == -1)
                                    intern_request->arg1 = "-1";
                                else{
                                    intern_request->arg1 = "0";
                                }

                                SEND_REQ_MAIN(intern_request);
                            }
                        }
                    }
                    break;

                case BYE:
                    if(request->main_worker == wid){
                        if(!(request->external) || N_WORKERS == 1){
                            close_all_files(files, fd_pool);
                            fill_reply(ans, NONE, NULL);
                            SEND_ANS();
                        } else {
                            intern_request = request;
                            intern_request->external = 0;
                            send_next_worker(wid, wqueue, intern_request);
                        }
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
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0644, &attr)) == (mqd_t) -1)
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

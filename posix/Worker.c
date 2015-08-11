#include "Common.h"
#include "Worker.h"
#define SEND_ANS()              mq_send(*(request->client_queue), (char *) ans, sizeof(*ans), 1)
#define SEND_REQ_MAIN(wrequest) mq_send(wqueue[request->main_worker], (char *) wrequest, sizeof(Request), 0);


struct mq_attr attr;

int file_descriptor = INIT_FD;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

//Ver comprobaciones de malloc y otras..

void print_request(int wid, Request *r){    //Ok
    printf("DFS_SERVER_REQ: [worker: %d] [id: %d] [op: %d] [external: %d] [arg0:'%s'] [arg1:'%s\'] [arg2:'%s']\n",
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
        return NULL; //si no retornamos null acá, como hicimos free(newFile)
                     //entonces newFile->content no sería válido
    }
    
	if(!(newFile->content = malloc(F_CONTENT_SIZE))){
		free(newFile->name);
		free(newFile);
        return NULL;    // este return NULL es redundante pero ayuda a que se entienda que se retorna NULL
	}

	return newFile;
}

int send_next_worker(int id, mqd_t *queue, Request *req){ //Ok
    return mq_send(queue[ (id+1)%N_WORKERS ], (char *)req, sizeof(*req), 0);
}

int is_open(File *files, char *nombre){ //-2 no existe, -1 existe cerrado, id existe abierto por id //Ok
	while(files){
		if(!strcmp(files->name, nombre)) 
            return (files->open);           // no hace falta ver que sea <0, si está cerrado ya es -1 y devolvemos eso
		files = files->next;
	}
	return -2;
}

int close_file(File *files, int fd){ //0 si se pudo cerrar, -1 si ya estaba cerrado, -2 si no existe
	while(files){
		if((files->fd) == fd){
			if((files->open) == -1)
				return -1;
			else{
				files->open = -1;
				files->cursor = 0;
				return 0;
			}
		}
		files = files->next;
	}
	return -2;
}

//fd si se pudo abrir, -1 si ya estaba abierto, -2 si no existe
int open_file(int client_id, File *files, char *nombre){ //Ok

	while(files){
	   if(!strcmp(files->name, nombre)) {      	// para comparar nombres siempre con strcmp!
			if((files->open) != -1){ 			//Todo valor distinto de 0 es true
				return -1;
			} else {
				files->open = client_id;
				return (files->fd);
			}
		}	
		files = files->next;
	}
	return -2;	
}

void close_all_files(File *files){ //OK
	
	while(files){
		if(files->open){
			files->open = -1;
			files->cursor = 0;
		}	
		files = files->next;
	}
}

int delete_file(File *files, File *fst_file, char *nombre){ //Todavía tengo problemas cuando 
	
	File *prev = NULL;
	
	if(files){
		if(!strcmp(files->name, nombre)){
			if((files->open) == -1){
				prev = files;
				files = files->next;
                free(prev->name);
                free(prev->content);
				free(prev);
				fst_file = files;
				return 0; //ok, cerrado
			} else { 
				return -1;	//Abierto
			}
		}
		prev = files;
		files = files->next;
		while(prev){
			if(!strcmp(files->name, nombre)){
				if((files->open) == -1){
					prev->next = files->next;
                    free(files->name);
                    free(files->content);
					free(files);
					return 0;	//OK, borrado
				} else 
					return -1;	//Abierto
			}
			prev = files;
			files = files->next;
		}
	}
	return -2; //No existe
}

char *list_files(File *files){ //OK
	
	if(!files) return "";
    
    char *lista = calloc((F_NAME_SIZE+1)*MAX_FILES, sizeof(char));
    
    if(files){
		strcpy(lista, files->name);
		files=files->next;
	}
    while(files){
		strcat(lista, " ");
        strcat(lista, files->name);
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
    int wid = ((Worker_Info *)w_info)->id;
    mqd_t *wqueue = ((Worker_Info *)w_info)->queue;
    free(w_info);	   
    
    // Loop infinito hasta que resolvamos las operaciones, vamos metiendolas adentro 
    // a medida que funcionan
    while(1){
        if((readed = mq_receive(wqueue[wid], buffer_in, MSG_SIZE+1, NULL)) >= 0){
            request = (Request *) buffer_in;
            print_request(wid, request);
    
		files = files_init;
    
		switch(request->op){
			
			case LSD:
				if(request->external){ //hacer free en handler creo
					char *tmp = (char *)calloc((F_NAME_SIZE+2)*MAX_FILES*N_WORKERS, sizeof(char));
					char *tmp2 = list_files(files);
                    if(strlen(tmp2)>0)
                        strcpy(tmp, tmp2);
                        
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
					strcpy (intern_request->arg0, request->arg0);
					if(strlen(tmp)>0){
						strcat(intern_request->arg0, " ");
						strcat(intern_request->arg0, tmp);
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
	
							pthread_mutex_lock(&fd_mutex);
								int tmp_fd = file_descriptor;
								file_descriptor++;
							pthread_mutex_unlock(&fd_mutex);
							
							File *new = create_file();
							if(new){
								memcpy(new -> name, request->arg0, F_NAME_SIZE-1);
								new -> fd = tmp_fd;
								new -> open = -1;
								new -> cursor = 0;
								new -> size = 0; //?
								new -> content = NULL;
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
					
					int tmp = open_file(request->client_id, files, request->arg0);
					
					//BORRAR
					printf("Comprobación OPN: %d.\n", tmp);
					//
					
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
							else{
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
							if(tmp == -1)
								intern_request->arg1 = "-1";
							else{
								char *aux;
								asprintf(&aux, "%d", tmp);
								intern_request->arg1 = aux;
							}
							SEND_REQ_MAIN(intern_request);
						}
					}
				}
				break; 
                
			case WRT:
				fill_reply(ans, NOT_IMP, NULL);
				SEND_ANS();
				break;
                
			case REA:
				fill_reply(ans, NOT_IMP, NULL);
				SEND_ANS();
				break;
                
			case CLO:
				if(!(request->external) && ((request->main_worker) == wid)){
					if(strcmp(request->arg1, "-1") == 0)
						fill_reply(ans, F_CLOSED, NULL);
					else if (strcmp(request->arg1, "-2") == 0)
						fill_reply(ans, F_NOTEXIST, NULL);
					else
						fill_reply(ans, NONE, NULL);
					
					SEND_ANS();	
				} else {
					
					int status = close_file(files, atoi(request->arg0));
					
					//BORRAR
					printf("Comprobación CLO: %d.\n", status);
					//
					
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
						close_all_files(files);
						fill_reply(ans, NONE, NULL);
						SEND_ANS();
					} else {
						intern_request = request;
						intern_request->external = 0;
						send_next_worker(wid, wqueue, intern_request);
					}
				} else {
					close_all_files(files);
					send_next_worker(wid, wqueue, request);
				}
				break;
		}
    
	}
    
    /*while(0){

        files = files_init;

			switch(request->op){
                    
				case DEL:{	//DONE
					if(request->external){
						
						int status = delete_file(files, request->arg0);
						
						if(status == 0){ //borrado
							
							ans->err = NONE;
							ans->answer = NULL;
							SEND_ANS();
							
						}
						else if(status == -1){ //error, abierto
							 
							 ans->err = F_OPEN;
							 ans->answer = NULL;
							 SEND_ANS();
							 
						}
						else{ //no existe, comprobar otros
							
							if(N_WORKERS == 1){
								
								ans->err = NONE;
								ans->answer = NULL;
								SEND_ANS();
								
							}
							else{
								
								intern_request -> op = DEL;
								intern_request -> external = 0;
								intern_request -> main_worker = request->main_worker;
								intern_request -> arg0 = request -> arg0;
								intern_request -> arg1 = "-2";
								intern_request -> arg2 = NULL;
								intern_request -> client_id = request -> client_id;
								intern_request -> client_queue = request -> client_queue;
 					
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
								else
									mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);

							}
						}
					}
					else{
							
						if(request->main_worker == wid){ //volví
							
							if(strcmp("-1", intern_request->arg1)){ //abierto, error
								
								ans->err = F_OPEN;
								ans->answer = NULL;
								SEND_ANS();
								
							}
							else{	//Borrado o no existe, OK
								
								ans->err = NONE;
								ans->answer = NULL;
								SEND_ANS();
								
							}
						}
						else{
							
							int status = delete_file(files, request->arg0);
							
							if(status == -2){
									
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &request, sizeof(request), 0);
								else
									mq_send(wqueue[wid+1], (char *) &request, sizeof(request), 0);	
								
							}
							else{
								
									intern_request -> op = DEL;
									intern_request -> external = 0;
									intern_request -> main_worker = request -> main_worker;
									intern_request -> arg0 = request -> arg0;
									
									if(status == -1)
										intern_request -> arg1 = "-1";
									else
										intern_request -> arg1 = "0";
										
									intern_request -> arg2 = NULL;
									intern_request -> client_id = request -> client_id;
									intern_request -> client_queue = request -> client_queue;
									
									mq_send(wqueue[request->main_worker], (char *) &intern_request, sizeof(intern_request), 0);
							}
						}
					}			
				}
                break;
        
				case WRT:{ //Falta analizar un caso
					//
					int found = 0;
					
					if((request->external) || (!(request->external)&&(request->main_worker != wid))){ //Pedido externo o interno distinto del ppal
					
						while(files != NULL){
						
							if(files->fd == atoi(request->arg0)){
								
								found = 1;
								
								if(files->open != wid){	
									if(request->external){
										ans->err = F_CLOSED;
										ans->answer = "El archivo no está abierto para éste worker.";
										SEND_ANS();
										break;
									}
									else{
										intern_request -> op = request->op;
										intern_request -> external = 0;
										intern_request -> main_worker = request -> main_worker;
										intern_request -> arg0 = NULL;
										intern_request -> arg1 = NULL;
										intern_request -> arg2 = "-1";
										intern_request -> client_id = request -> client_id;
										intern_request -> client_queue = request -> client_queue;
										
										mq_send(wqueue[request->main_worker], (char *) &intern_request, sizeof(intern_request), 0);
										break;
									}
								}
								else{
								
									if((request->arg1) <= 0){
											ans->err = NONE;
											ans->answer = NULL;
									}
									else{
										
										strcat(files->content, request->arg2),
										
										ans->err = NONE; //cambiar por casos - ARREGLAR!!
										ans->answer = NULL; //ver en donde hacer el free
									
									}
									SEND_ANS();
									break;
								}
							
							}
							else
								files = files->next;
							
						}
						
						if((N_WORKERS > 1) && (found == 0)){
							
							intern_request -> op = request->op;
							intern_request -> external = 0;
							intern_request -> main_worker = request -> main_worker;
							intern_request -> arg0 = request -> arg0;
							intern_request -> arg1 = request -> arg1;
							intern_request -> arg2 = request->arg2;
							intern_request -> client_id = request -> client_id;
							intern_request -> client_queue = request -> client_queue;
							
							if(wid == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);
						
						}
						
					}
					else{	//volví
							
							if(strcmp(request->arg2, "0") == 0){ //Lectura satisfactoria
								ans->err = NONE;
								ans->answer = request->arg1;
							}
							else if(strcmp(request->arg2, "-1") == 0){ //Archivo cerrado 
								ans->err = F_CLOSED;
								ans->answer = "El archivo no está abierto para éste worker.";
							}
							else{
								ans->err = BAD_FD;	//Les parece o eso era mas para el handler?
								ans->answer = NULL;
							}
							SEND_ANS();
					}	
					//
				}
                break;    
                    
				case REA:{
					
					int found = 0;
					
					if((request->external) || (!(request->external)&&(request->main_worker != wid))){ //Pedido externo o interno distinto del ppal
					
						while(files != NULL){
						
							if(files->fd == atoi(request->arg0)){
								
								found = 1;
								
								if(files->open != wid){	
									if(request->external){
										ans->err = F_CLOSED;
										ans->answer = "El archivo no está abierto para éste worker.";
										SEND_ANS();
										break;
									}
									else{
										intern_request -> op = request->op;
										intern_request -> external = 0;
										intern_request -> main_worker = request -> main_worker;
										intern_request -> arg0 = NULL;
										intern_request -> arg1 = NULL;
										intern_request -> arg2 = "-1";
										intern_request -> client_id = request -> client_id;
										intern_request -> client_queue = request -> client_queue;
										
										mq_send(wqueue[request->main_worker], (char *) &intern_request, sizeof(intern_request), 0);
										break;
									}
								}
								else{
								
									if((request->arg1) <= 0){
											ans->err = NONE;
											ans->answer = NULL;
									}
									else{
										
										char *aux = calloc(atoi(request->arg1) + 1, 1);
										int n = 0;
										
										while((files->cursor < files->size) && (n < atoi(request->arg1))){
											aux[n] = (files->content)[files->cursor];
											n++;
											(files->cursor)++;
										}
										
										ans->err = NONE;
										ans->answer = aux; //ver en donde hacer el free
										
											 
									//el handler deber responder además haciendo un sizeof de answer
									//mover cursor a donde corresponda
									
									}
									SEND_ANS();
									break;
								}
							
							}
							else
								files = files->next;
							
						}
						
						if((N_WORKERS > 1) && (found == 0)){
							
							intern_request -> op = request->op;
							intern_request -> external = 0;
							intern_request -> main_worker = request -> main_worker;
							intern_request -> arg0 = request -> arg0;
							intern_request -> arg1 = request -> arg1;
							intern_request -> arg2 = "0";
							intern_request -> client_id = request -> client_id;
							intern_request -> client_queue = request -> client_queue;
							
							if(wid == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);
						
						}
						
					}
					else{	//volví
							
							if(strcmp(request->arg2, "0") == 0){ //Lectura satisfactoria
								ans->err = NONE;
								ans->answer = request->arg1;
							}
							else if(strcmp(request->arg2, "-1") == 0){ //Archivo cerrado 
								ans->err = F_CLOSED;
								ans->answer = "El archivo no está abierto para éste worker.";
							}
							else{
								ans->err = BAD_FD;	//Les parece o eso era mas para el handler?
								ans->answer = NULL;
							}
							SEND_ANS();
					}	
				}
                
                break;    
                        
				
			}*/
			
	}
						
    mq_close(wqueue[wid]);
    return 0;

}


int init_workers(){
    
    for(int i = 0; i<N_WORKERS; i++){
        
        // Instance the worker message queue
        char *worker_name;
        asprintf(&worker_name, "/w%d", i);
        
        attr.mq_flags = 0;  
        attr.mq_maxmsg = MAX_MESSAGES;  
        attr.mq_msgsize = MSG_SIZE;  
        attr.mq_curmsgs = 0;
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0644, &attr)) == (mqd_t) -1)
            ERROR("\nDFS_SERVER: Error opening message queue for workers\n");
        
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues;
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

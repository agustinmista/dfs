#include "Common.h"
#include "Worker.h"
#define SEND_ANS() mq_send(*(request->client_queue), (char *) ans, sizeof(*ans), 1)
#define SEND_REQ_MAIN(wrequest); mq_send(wqueue[request->main_worker], (char *) wrequest, sizeof(Request), 0);


struct mq_attr attr;

int file_descriptor = INIT_FD;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
int tmp_fd;

//Ver comprobaciones de malloc y otras..

void print_request(int wid, Request *r){    
    printf("DFS_SERVER_REQ: [worker: %d] [id: %d] [op: %d] [external: %d] [arg0:'%s'] [arg1:'%s\'] [arg2:'%s']\n",
           wid, r->client_id, r->op, r->external, r->arg0, r->arg1, r->arg2);
}

void fill_reply(Reply *ans, Error werror, char *wreply){
	
	ans->err = werror;
	ans->answer = wreply;
	
}

void fill_request (Request *intern_request,Operation op,int ext, int mw, char *arg0,char *arg1,char *arg2,int cID,mqd_t *cl_queue){
    intern_request->op = op;
   	intern_request->external = ext;
    intern_request->main_worker = mw;
    intern_request->arg0 = arg0;
    intern_request->arg1 = arg1;
    intern_request->arg2 = arg2;
    intern_request->client_id = cID;
    intern_request->client_queue = cl_queue;            
}


File *create_file(){

	File *aux = malloc(sizeof(File));
	if(aux == NULL)
		return NULL;
		
	aux->name = malloc(32);
	if(aux->name == NULL)
		free(aux);
		
	aux->content = malloc(2000);
	if(aux->content == NULL){
		free(aux->name);
		free(aux);
	}
	return aux;
}

int send_next_worker(int id, mqd_t *queue, Request *message){
	
	if(id == N_WORKERS - 1)
		return mq_send(queue[0], (char *)message, sizeof(*message), 0);
	else
		return mq_send(queue[id+1], (char *)message, sizeof(*message), 0);

}

int is_open(File *files, char *nombre){ //-2 no existe, -1 existe cerrado, id existe abierto por id

	while(files != NULL){
		if(strcmp(files->name, nombre) == 0){
			if((files->open) < 0)
				return -1;
			else
				return (files->open);
		}
		files = files->next;
	}
	return -2;
}

int close_file(File *files, int descriptor){ //0 si se pudo cerrar, -1 si ya estaba cerrado, -2 si no existe
	
	while(files != NULL){
		if(files->fd == descriptor){
			if((files->open) < 0)
				return -1;
			else{
				files->open = -1;
				files->cursor = 0;
				return 0;
			}
			
		files = files->next;
		}
	}
	return -2;
	
}

int open_file(int cID, File *files, char *nombre, int *obs){	//0 si se pudo abrir ->obs = fd, -1 si ya estaba abierto ->obs=cID, -2 si no existe
	
	while(files != NULL){
		if(files->name == nombre){
			if((files->open) < 0){
				files->open = cID;
				obs = &(files->fd);
				return 0;
			}
			else{
				obs = &(files->open);
				return -1;
			}
		}	
		files = files->next;
	}
	obs = NULL; //?
	return -2;	
}

int close_all_files(File *files){
	
	while(files != NULL){
		if((files->open) > 0){
			files->open = -1;
			files->cursor = 0;
		}	
		files = files->next;
	}
	
	return 0;
}

int delete_file(File *files, char *nombre){
	
	File *prev = NULL;
	
	while(files != NULL){
		if(strcmp(files->name, nombre) == 0){
			if((files->open) < 0){
				prev->next = files->next;
				free(files);
				return 0;	//OK, borrado
			}
			else
				return -1;	//Abierto
		}
		else
			files = files->next;
	}
	return -2; //No existe
	
}

char *list_files(File *files){
	
	if(files == NULL)
		return " ";
	else{
		char *lista = "";
		while(files != NULL){
			strcat(lista, files->name);
			strcat(lista, " ");
			files = files->next;
		}
		return lista;
	}
}

void *worker(void *w_info){
	
	File *files = NULL;
	File *files_init = NULL;
    
    char buffer_in[MSG_SIZE+1];
    int readed;
    
    Request *request;
    Request *intern_request = malloc(sizeof(*intern_request));
	Reply *ans = (Reply *)malloc(sizeof(Reply));
	
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
    
		switch(request->op){
			
			case LSD:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case DEL:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case CRE:
				if((request->main_worker) == wid){
						
					if((!(request->external) && (strcmp(request->arg1, "0") == 0)) || (N_WORKERS == 1)){
							
						if(is_open(files, request->arg0) == -2){
							
							pthread_mutex_lock(&fd_mutex);
								tmp_fd = file_descriptor;
								file_descriptor++;
							pthread_mutex_unlock(&fd_mutex);
								
							File *new = create_file();
							new -> name = request->arg0;
							new -> fd = tmp_fd;
							new -> open = -1;
							new -> cursor = 0;
							new -> size = 0; //?
							new -> content = NULL;
							new -> next = files_init; 
								
							if(files_init == NULL)
								files_init = new;
								
							fill_reply(ans, NONE, NULL);	
							
						}
						else
							fill_reply(ans, F_EXIST, NULL);
							
						SEND_ANS();
					}
					else if(!(request->external) && (strcmp(request->arg1, "-1") == 0)){	
						fill_reply(ans, F_EXIST, NULL);
						SEND_ANS();
					}
					else{
							
						if(is_open(files, request->arg0) == -2){
							intern_request = request;
							intern_request->external = 0;
							intern_request->arg1 = "0";
							send_next_worker(wid, wqueue, intern_request);
						}
						else{
							fill_reply(ans, F_EXIST, NULL);
							SEND_ANS();
						}
					}
				}
				else{
					
					if(is_open(files, request->arg0) == -2){
						intern_request=request;
						intern_request->external = 0;
						intern_request->arg1 = "0";
						send_next_worker(wid, wqueue, intern_request);
					}
					else{
						intern_request=request;
						intern_request->external = 0;
						intern_request->arg1 = "-1";
						SEND_REQ_MAIN(intern_request);
					}	
				}			
                break;
			case OPN:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case WRT:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case REA:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case CLO:
				fill_reply(ans, NONE, NULL);
				SEND_ANS();
				break;
			case BYE:
				if(request->main_worker == wid){
					if(!(request->external) || N_WORKERS == 1){
						close_all_files(files);
						fill_reply(ans, NONE, NULL);
						SEND_ANS();
					}
					else{
						intern_request = request;
						intern_request->external = 0;
						send_next_worker(wid, wqueue, intern_request);
					}
				}
				else{
					close_all_files(files);
					send_next_worker(wid, wqueue, request);
				}
				break;
		}
    
	}
    
    /*while(0){

        files = files_init;

			switch(request->op){
				
				case LSD:	//DONE
					
					if(request->external){
						if(N_WORKERS > 1){
							intern_request->op = LSD;
							intern_request->external = 0;
							intern_request->main_worker = request -> main_worker;
							intern_request->arg0 = NULL;
							intern_request->arg1 = NULL;
							intern_request->arg2 = NULL;
							intern_request->client_id = request->client_id;
							intern_request->client_queue = request->client_queue;
						
							if(wid  == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);
						} else {
							ans->err=NONE;
							ans->answer = list_files(files);
							SEND_ANS();
						}
                        
					} else {
						
						if(wid == request->main_worker){ //volvi
							ans->err = NONE;
							ans->answer = strcat(request->arg0, list_files(files));
							SEND_ANS();
						} else {
							intern_request->op = LSD;
							intern_request->external = 0;
							intern_request->main_worker = request->main_worker;
							intern_request->arg0 = strcat(request->arg0, list_files(files));
							intern_request->arg1 = NULL;
							intern_request->arg2 = NULL;
							intern_request->client_id = request->client_id;
							intern_request->client_queue = request->client_queue;
						
							if(wid  == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);
						}
					}
					break;
                    
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
                    
				
				case OPN:{	//Falta revisar bien pero creo que estaría
				
				int *code = malloc(sizeof(int));
				code = NULL;
				
				if(request->external){
						
						int status = open_file(request->client_id, files, request->arg0, code);
					
						if(status == -2){
							
							intern_request -> op = request -> op;
							intern_request -> external = 0;
							intern_request -> main_worker = request -> main_worker;
							intern_request -> arg0 = request -> arg0;
							intern_request -> arg1 = "-2";
							intern_request -> arg2 = NULL;
							intern_request -> client_id = request -> client_id;
							intern_request -> client_queue = request -> client_queue;
							
							if(N_WORKERS == 1){
								
								ans->err = F_NOTEXIST;
								ans->answer = NULL;
								SEND_ANS();
								
							}
							else{	
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), 0);
								else
									mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), 0);						
							}
						}
						else if(status == -1){
							
							ans->err = F_OPEN;
							asprintf(&(ans->answer), "%d", *code); //cID que lo abrió
							SEND_ANS();

						}
						else{
							ans->err = NONE;
							asprintf(&(ans->answer), "%d", *code);	//file descriptor
							SEND_ANS();
						}
					}
					else{
						
						if(request -> main_worker == wid){
							
							if(strcmp(request -> arg1, "-2") == 0){
							
								ans -> err = F_NOTEXIST;
								ans -> answer = NULL;
								SEND_ANS();			
													
							}
							else if(strcmp(request -> arg1, "-1")){
							
								ans->err = F_OPEN;
								asprintf(&(ans->answer), "%d", *(int *)(request->arg2)); //cID que lo abrió
								SEND_ANS();

							}
							else{
								ans->err = NONE;
								asprintf(&(ans->answer), "%d", *(int *)(request->arg2));	//file descriptor
								SEND_ANS();
							}
						}
						else{
							
							int status = open_file(request->client_id, files, request->arg0, code);
							
							if(status == -2){
								
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &request, sizeof(request), 0);
								else
									mq_send(wqueue[wid+1], (char *) &request, sizeof(request), 0);						

							}
							else{
								
								intern_request -> op = request -> op;
								intern_request -> external = 0;
								intern_request -> main_worker = request -> main_worker;
								intern_request -> arg0 = request -> arg0;
								if(status == -1)
									intern_request -> arg1 = "-1";
								else
									intern_request -> arg1 = "0";
									
								intern_request -> arg2 = (char *)code;
								intern_request -> client_id = request -> client_id;
								intern_request -> client_queue = request -> client_queue;

								mq_send(wqueue[request->main_worker], (char *) &intern_request, sizeof(intern_request), 0);
							}
						}
					}
					
					free(code);
					
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
                    
				case CLO:{	//DONE
				
					if(request->external){
						
						int status = close_file(files, atoi(request -> arg0));
					
						if(status == -2){
							
							intern_request -> op = request -> op;
							intern_request -> external = 0;
							intern_request -> main_worker = request -> main_worker;
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
						else{
							
							ans->err = NONE;
							ans->answer = NULL;
							SEND_ANS();

						}
					}
					else{
						
						if(request -> main_worker == wid){
							
							if(strcmp(request -> arg1, "-2") == 0){
							
								ans -> err = F_NOTEXIST;
								ans -> answer = NULL;
								
								SEND_ANS();								
							}
							else{
								
								ans -> err = NONE;
								ans -> answer = NULL;
					
								SEND_ANS();
								
							}
						}
						else{
							
							int status = close_file(files, atoi(request->arg0));
							
							if(status == -2){
								
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &request, sizeof(request), 0);
								else
									mq_send(wqueue[wid+1], (char *) &request, sizeof(request), 0);						

							}
							else{
								
								intern_request -> op = request -> op;
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
                    
				case BYE:{
//                    			int s;
//                    			if(request->external){
//                        			if(N_WORKERS > 1){
//                		    			intern_request->op = BYE;
//                					intern_request->external = 0;
//                					intern_request->main_worker = request -> main_worker;
//                		    			intern_request->arg0 = NULL;
//                		    			intern_request->arg1 = NULL;
//                					intern_request->arg2 = NULL;
//                					intern_request->client_id = request->client_id;
//                					intern_request->client_queue = request->client_queue;			
//            
//			                            	if (wid == N_WORKERS - 1)
//                        			        	mq_send(wqueue[0],(char *) &intern_request,sizeof(intern_request), MAX_PRIORITY);
//                            			    	else 
//                                				mq_send(wqueue[N_WORKERS + 1],(char *) &intern_request,sizeof(intern_request), MAX_PRIORITY);
//                        			}
//                        			else{
//                            				while (files != NULL){
//                                				s = close_file(files,atoi(files->name));
//                                				files = files -> next;
//                            				}
//                            				ans -> err = NONE;
//                            				ans ->answer = NULL;
//                            				SEND_ANS();
//                        			}
//                			}
//                			else{
//						if (wid == request->main_worker){
//    
//                        				while (files != NULL){
//                            					s = close_file(files,atoi(files->name));
//                            					files = files -> next;
//                        				}
//                        				ans -> err = NONE;
//                        				ans ->answer = NULL;
//                        				SEND_ANS();
//                    				}
//	
//      				        else{
//          						intern_request->op = BYE;
//          						intern_request->external = 0;
//            						intern_request->main_worker = request->main_worker;
//            						intern_request->arg0 = NULL;
//            						intern_request->arg1 = NULL;
//                					intern_request->arg2 = NULL;
//            						intern_request->client_id = request->client_id;
//            						intern_request->client_queue = request->client_queue;            
//			                        
//                        				if (wid == N_WORKERS - 1) 
//                            					mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
//            						else
//            							mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
//               
//                   				}	
//                			}					
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

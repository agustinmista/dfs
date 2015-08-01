#include "Common.h"
#include "Worker.h"

//HACER MACRO CON RESPUESTA SATISFACTORIA AL HANDLER

struct mq_attr attr;

int file_descriptor = INIT_FD;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
int tmp_fd;

int archivo_abierto(File *files, char *nombre){ //-2 no existe, -1 existe cerrado, id existe abierto por id

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

int borrar(File *files, char *nombre){
	
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

char * listar_archivos(File *files){
	
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
    
    char message[MSG_SIZE+1];
    Request *request;
    Request *intern_request = malloc(sizeof(Request));
	Reply *ans = malloc(sizeof(Reply));
	
    // Parse worker args
    int wid      = ((Worker_Info *)w_info)->id;
    mqd_t *wqueue = ((Worker_Info *)w_info)->queue;
    free(w_info);	
    
    int readed;
    
    while(1){
        
        memset(message, 0, MSG_SIZE+1);
        ans->err = NONE;
        ans->answer = "";
        files = files_init;
        
        if((readed = mq_receive(wqueue[wid], message, sizeof(message), NULL)) >= 0){
		      
			request = (Request *) message;
			printf("Recibí una request de %d\n", request->client_id);
            
			switch(request->op){
				
				case LSD:{
					
					if(request->origin){
						
						if(N_WORKERS > 1){
							intern_request->op = LSD;
							intern_request->origin = 0;
							asprintf(&(intern_request -> arg0), "%d", wid);
							intern_request->arg1 = NULL;
							intern_request->arg2 = NULL;
							intern_request->client_id = request->client_id;
							intern_request->client_queue = request->client_queue;
						
							if(wid  == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
						}
						else{
							ans->err=NONE;
							ans->answer = listar_archivos(files);
							mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
						}
					}
					else{
						
						if(wid == atoi(request->arg0)){ //volvi
							ans->err = NONE;
							ans->answer = strcat(request->arg1, listar_archivos(files));
							mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
						}
						else{
							intern_request->op = LSD;
							intern_request->origin = 0;
							intern_request->arg0 = request->arg0;
							intern_request->arg1 = strcat(request->arg1, listar_archivos(files));
							intern_request->arg2 = NULL;
							intern_request->client_id = request->client_id;
							intern_request->client_queue = request->client_queue;
						
							if(wid  == N_WORKERS - 1)
								mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
							else
								mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);

						}
					}
					
				}
				case DEL:{ //Me falta terminar

					//Crear una función que analice si el archivo
					//se encuentra en este worker
					//Si no está enviar mensaje indicando el comando y el nro
					//de worker
				
					//Borrar el archivo si está en alguno
					
					if(request->origin){
						
						int status = borrar(files, request->arg0);
						
						if(status == 0){ //borrado
							
							ans->err = NONE;
							ans->answer = NULL;
							mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
							
						}
						else if(status == -1){ //error, abierto
							 
							 ans->err = F_OPEN;
							 ans->answer = NULL;
							 mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
							 
						}
						else{ //no existe, comprobar otros
							
							if(N_WORKERS == 1){
								
								ans->err = NONE;
								ans->answer = NULL;
								mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
								
							}
							else{
								
								intern_request -> op = DEL;
								intern_request -> origin = 0;
								intern_request -> arg0 = request -> arg0;
								asprintf(&(intern_request -> arg1), "%d", wid);
								intern_request -> arg2 = "-2";
								intern_request -> client_id = request -> client_id;
								intern_request -> client_queue = request -> client_queue;
 					
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
								else
									mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);

							}
						}
					}
					else{
							
						if(atoi(request-> arg1) == wid){ //volví
							
							if(strcmp("-1", intern_request->arg2)){ //abierto, error
								
								ans->err = F_OPEN;
								ans->answer = NULL;
								mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
								
							}
							else{	//Borrado o no existe, OK
								
								ans->err = NONE;
								ans->answer = NULL;
								mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
								
							}
						}
						else{
							
							int status = borrar(files, request->arg0);
							
							if(status == -2){
									
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &request, sizeof(request), MAX_PRIORITY);
								else
									mq_send(wqueue[wid+1], (char *) &request, sizeof(request), MAX_PRIORITY);	
								
							}
							else{
								
									intern_request -> op = DEL;
									intern_request -> origin = 0;
									intern_request -> arg0 = request -> arg0;
									intern_request -> arg1 = request -> arg1;
									
									if(status == -1)
										intern_request -> arg2 = "-1";
									else
										intern_request -> arg2 = "0";
										
									intern_request -> client_id = request -> client_id;
									intern_request -> client_queue = request -> client_queue;
									
									mq_send(wqueue[atoi(request->arg1)], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
							}
						}
					}			
				}
				
				case CRE:{	//NE < -1
					
					if((request->origin) && (archivo_abierto(files,request->arg0) > -2)){ //existe en worker ppal
		
						ans->answer = NULL;
						ans->err = F_EXIST;
		
						mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
					
					}
					else if((request->origin) && (archivo_abierto(files,request->arg0) < -1)) { //No existe en ppal
						
						//inicio anillo
						intern_request -> op = CRE;
						intern_request -> origin = 0;
						intern_request -> arg0 = request -> arg0;
						asprintf(&(intern_request -> arg1), "%d", wid);
						intern_request -> arg2 = "0";
						intern_request -> client_id = request -> client_id;
						intern_request -> client_queue = request -> client_queue;
 					
						if(wid == N_WORKERS - 1)
							mq_send(wqueue[0], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
						else
							mq_send(wqueue[wid+1], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY);
							
					}
					else if(!(request->origin)) { //Interno
						
						if(atoi(request -> arg1) == wid){ //volví
							if(strcmp(request->arg2, "-1") == 0){ //existe
								
								ans->answer = NULL;
								ans->err = F_EXIST;
								
							}
							else{
								
								pthread_mutex_lock(&fd_mutex);
									tmp_fd = file_descriptor;
									file_descriptor++;
								pthread_mutex_unlock(&fd_mutex);
					
								File *new = malloc(sizeof(File));
								new -> name = request->arg0;
								new -> fd = tmp_fd;
								new -> open = -1;
								new -> cursor = 0;
								new -> size = 0; //?
								new -> content = NULL;
								new -> next = files; 
					
								if(files_init->content == NULL)
									files = new;
								else{
									while(files->next != NULL){
										files = files -> next;
									}
									files->next = new;
								}
								
								asprintf(&(ans->answer), "%d", tmp_fd); //VER
								ans->err = NONE;
								
							}
							
							mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), MAX_PRIORITY);
							
						}
						else{	//todavía no volví
							
							if(archivo_abierto(files,request->arg0) > -2){ //existe
								
								intern_request -> op = CRE;
								intern_request -> origin = 0;
								intern_request -> arg0 = request -> arg0;
								intern_request -> arg1 = request -> arg1; //id worker ppal
								intern_request -> arg2 = "-1";
								intern_request -> client_id = request -> client_id;
								intern_request -> client_queue = request -> client_queue;
							
								mq_send(wqueue[atoi(request->arg1)], (char *) &intern_request, sizeof(intern_request), MAX_PRIORITY); 
							
							}
							else{
								
								if(wid == N_WORKERS - 1)
									mq_send(wqueue[0], (char *) &request, sizeof(request), MAX_PRIORITY);
								else
									mq_send(wqueue[wid+1], (char *) &request, sizeof(request), MAX_PRIORITY);
							
							}
							
						}	
					
					}
					
				}
				case OPN:{
					
				}
				case WRT:{ 
					
				}
				case REA:{ 
					
				}
				case CLO:{
				
				}
				case BYE:{
					
				}
			}
			
		}
		
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
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0666, &attr)) == (mqd_t) -1)
            ERROR("\nDFS_SERVER: Error opening message queue for workers\n");
        
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues;
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

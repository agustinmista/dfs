#include "Common.h"
#include "Worker.h"

/*struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = 10000;
attr.mq_flags = 0;*/

//seguir viendo como manejar bien queues y ver como unificar respuestas

int file_descriptor = INIT_FD;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

int existe_archivo(int wID, File *files, char *nombre){

    int i = 0;

    while (files != NULL)
    {
        if (strcmp(files->name, nombre) == 0)
            return i;
        else
            i++;
        files = files->next;
    }
    return -1;
}

void *worker(void *w_info){
	
	File *files = NULL;
	File *files_init = NULL;
    
    int n_files = 0;
    int i;
    
    char message[MSG_SIZE+1];
    Request *request;
    Request *intern_request = malloc(sizeof(Request));
	Reply *ans = malloc(sizeof(Reply));
	
    // Parse worker args
    int wid      = ((Worker_Info *)w_info)->id;
    mqd_t *wqueue = ((Worker_Info *)w_info)->queue;
    free(w_info);	
    
    
    while(1){
        
        memset(message, 0, MSG_SIZE+1);
        ans->err = NONE;
        ans->answer = "";
        files = files_init;
              
        if(mq_receive(wqueue[wid], message, sizeof(message), NULL) >= 0){
		
			request = (Request *) message;
			
			switch(request->op){
				
				case LSD:{
					
					if(wid == 0){
							
							for(i=0; i < n_files; i++){
								strcat(ans->answer, files->name);
								strcat(ans->answer, " ");
								files = files->next;
							}
							
							if(N_WORKERS > 1){
								intern_request->op = LSD;
								intern_request->err = NONE;
								intern_request->arg0 = "0";
								intern_request->arg1 = NULL;
								intern_request->arg2 = NULL;
								intern_request->client_id = request->client_id;
								intern_request->client_queue = request->client_queue;
								
								for(i=1; i < N_WORKERS; i++)				
									mq_send(wqueue[i], (char *) &intern_request, sizeof(intern_request), 32768);
							}
							
							mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), 32768); 
								
					}
					else if(strcmp(request->arg0, "0") == 0){		//mensaje interno
						
						for(i=0; i < n_files; i++){
								strcat(ans->answer, files->name);
								strcat(ans->answer, " ");
								files = files->next;
						}
						
						mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), 32768);
					}
					else {
						
						mq_send(wqueue[0], (char *) &request, sizeof(request), 32768);
						
					}
				}
				case DEL:{ 

					//Crear una función que analice si el archivo
					//se encuentra en este worker
					//Si no está enviar mensaje indicando el comando y el nro
					//de worker
				
					//Borrar el archivo si está en alguno
				}
				case CRE:{
					
					i = 0;
                    int res = -1;
                    int tmp_fd;
                    
                    pthread_mutex_lock(&fd_mutex);
						tmp_fd = file_descriptor;
						file_descriptor++;
					pthread_mutex_unlock(&fd_mutex);
                   
					
					// Esto no se si esta bien hacerlo asi o convendria hacerlo en paralelo: <- mensajes creo, ver como unificar respuesta
                    /* while ((i < N_WORKERS){  <- cada uno debería tener acceso directo sólo a su lista de archivos
                        res = existe_archivo_2(i, files,name);
                        
                        }*/
                        
                    //for(i=1; i < N_WORKERS; i++){		//modificarlo para distinguir msjs internos		
						//if(i != wid)
							//mq_send(WorkerN, (char *) &intern_request, sizeof(intern_request), MQ_PRIO_MAX);
                    //}
                    
                    
                    //receives unificados
                    //res = existe_archivo_local, rtas de receive
                    
                    if (res != -1){
						ans->answer = NULL;
						ans->err = F_EXIST;
					}    
                    else if (res == -1){                 
							
							File *new = malloc(sizeof(File));
							new -> name = request->arg0;
							new -> fd = tmp_fd;
							new -> open = -1;
							new -> cursor = 0;
							new -> size = 0; //?
							new -> content = NULL;
							new -> next = NULL; 
                        
							if(files_init->content == NULL)
								files_init = new;
							else{
								while(files->next != NULL)
									files = files -> next;
								
								files->next = new;
							}

                        ans -> answer = NULL;                   				
                        ans -> err = NONE;
                	}
					mq_send(*(request->client_queue), (char *) &ans, sizeof(ans), 32768);
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
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0666, NULL)) == (mqd_t) -1)
            ERROR("\nDFS_SERVER: Error opening message queue for workers\n");
        
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues;
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

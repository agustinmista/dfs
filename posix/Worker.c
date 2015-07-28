#include "Common.h"
#include "Worker.h"

/*struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = 10000;
attr.mq_flags = 0;*/

void *worker(void *w_info){
	File *files_init, *files;
    
    int n_files = 0;
    int i;
    
    char message[MSG_SIZE+1];
    Request *request;
    Request *intern_request = malloc(sizeof(Request));
	Reply *ans = malloc(sizeof(Reply));
	
    // Parse worker args
    int wid      = ((Worker_Info *)w_info)->id;
    mqd_t wqueue = ((Worker_Info *)w_info)->queue;
    free(w_info);	
    
    
    while(1){
        
        memset(message, 0, MSG_SIZE+1);
        ans->err = NONE;
        ans->answer = "";
        files = files_init;
              
        if(mq_receive(wqueue, message, sizeof(message), NULL) >= 0){
		
			request = (Request *) message;
			
			switch(request->op){
				
				case 0:{ //LSD
					
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
								
								//for(i=1; i < N_WORKERS; i++)				
									//mq_send(WorkerN, (char *) &intern_request, sizeof(intern_request), MQ_PRIO_MAX);
							}
							
							//mq_send(request->client_queue, (char *) &ans, sizeof(ans), MQ_PRIO_MAX - 1); 
								
					}
					else if(strcmp(request->arg0, "0") == 0){		//mensaje interno -- externos con -1?
						
						for(i=0; i < n_files; i++){
								strcat(ans->answer, files->name);
								strcat(ans->answer, " ");
								files = files->next;
						}
						
						//mq_send(request->client_queue, (char *) &ans, sizeof(ans), MQ_PRIO_MAX - 1);
					}
					else {
						
						//mq_send(Worker0, (char *) &request, sizeof(request), MQ_PRIO_MAX);
						
					}
				}
				case 1:{ //DEL

				//Crear una función que analice si el archivo
				//se encuentra en este worker
				//Si no está enviar mensaje indicando el comando y el nro
				//de worker
				
				//Borrar el archivo si está en alguno
				}
				case 2:{ //CRE
					
				}
				case 3:{ //OPN
					
				}
				case 4:{ //WRT
					
				}
				case 5:{ //REA
					
				}
				case 6:{ //CLO
				
				}
				case 7:{ //BYE
					
				}
			}
			
		}
		
	}
			
	free(intern_request);
	free(ans);			
    mq_close(wqueue);
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
        newWorker->queue = worker_queues[i];
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

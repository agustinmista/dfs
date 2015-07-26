#include "Common.h"
#include "Worker.h"

/*struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = 10000;
attr.mq_flags = 0;*/


void *worker(void *w_info){
	
	Worker_Info *info = (Worker_Info *)w_info;
    int wid = info->id;
    mqd_t wqueue = info->queue;
    File *wfiles = info->files;
    
    //File *files = 
    //files->content = NULL;
    //files->next = NULL;
    char message[100];
    char *parsing, *saveptr;
	
			
    while(1){
        
        memset(message, 0, 100);
              
        if(mq_receive(wqueue, message, 100, NULL) >= 0){
			
			printf("... \n");
			
			parsing = strtok_r(message, " ", &saveptr);
			if (strcmp (parsing,"LSD") == 0)
			{
				parsing = strtok_r(NULL, " ", &saveptr);
				if (parsing == NULL){
					printf("LSD\n");
				}else
					printf("error de comando \n");
			}
			else if (strcmp (parsing,"DEL") == 0)
				printf ("DEL\n");
			else if (strcmp (parsing,"CRE") == 0)
				printf ("CRE\n");
			else if (strcmp (parsing,"OPN") == 0)
				printf ("OPN\n");
			else if (strcmp (parsing,"WRT") == 0)
				printf ("WRT\n");
			else if (strcmp (parsing,"REA") == 0)
				printf ("REA\n");
			else if (strcmp (parsing,"CLO") == 0)
				printf ("CLO\n");
			else if (strcmp (parsing,"BYE") == 0)
				printf ("BYE\n");
			else
				printf("error de comando");
        //---
        //--- Where the magic happens!
        //---
		}
	}
    
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
        
        //worker_files[i] = malloc(sizeof(File)*n_Files_Worker);
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues[i];
        newWorker->files = NULL;
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}

#include "Common.h"
#include "Worker.h"

/*
struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = MSG_SIZE;
attr.mq_flags = 0;		//VER
*/

void *worker(void *id){
	
	int wid = *((int *) id);
    
    File *files = NULL;
	
    while(1){
        //---
        //--- Where the magic happens!
        //---
    }    
    
    
    mq_close(worker_queues[wid]);
    return 0;

}

int init_workers(){
	
    for(int i = 0; i<N_WORKERS; i++){
        
        // Instance the worker message queue
        char *worker_name;
        asprintf(&worker_name, "/w%d", i);
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0666, NULL)) != (mqd_t) -1)
            ERROR("DFS_SERVER: Error opening message queue for workers\n");
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, &i);
        
    }
    return 0;
}


File *newFile(File *files, char *name){
    File *newFile = (File *) malloc(sizeof(File));
    
    strcpy(new_file->name, name);
    
    pthread_mutex_lock(&mutex_fd);
        newFile->fd = global_fd++;
    pthread_mutex_unlock(&mutex_fd);
    
    newFile->open = -1;
    newFile->cursor = 0;
    newFile->size = 0;
    newFile->content = NULL;
    
    if (files)  newFile->next = files;
    else        newFile->next = NULL;
    
    return newFile;    
}



#include "Common.h"
#include "Worker.h"

#define fstID 100;

int workerids = fstID;

pthread_mutex_t mutexID = PTHREAD_MUTEX_INITIALIZER;

/*
struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = MSG_SIZE;
attr.mq_flags = 0;		//VER
*/

void *worker(){
	
	int wid; //ID worker
    pthread_mutex_lock(&mutexID);
		wid = workerids;
		workerids++;
    pthread_mutex_unlock(&mutexID);
    
    char name[8];
    
    sprintf(name, "Inbox%d", wid);
    
    mqd_t wrkqueue = mq_open(name, O_RDWR || O_CREAT); //Ver atributos
     
	//...
    
    mq_close(wrkqueue);

    
    return 0;

}

int init_workers(int nw){
    
	int i;
	
	pthread_t workers[nw];
	
	for(i = 0; i<nw; i++)
		pthread_create(&workers[i], NULL, worker, NULL);
    
    return 0;
}

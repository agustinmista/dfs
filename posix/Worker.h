#ifndef __WORKER_H__
#define __WORKER_H__
#endif

typedef struct{
	int id;
	File *files;
	//mqd_t msg;
}Worker;

int init_workers(int nw);

#ifndef __WORKER_H__
#define __WORKER_H__
#endif

#define n_Files_Worker 5

pthread_t workers[N_WORKERS];
mqd_t worker_queues[N_WORKERS];

int init_workers();

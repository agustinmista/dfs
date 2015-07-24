#ifndef __WORKER_H__
#define __WORKER_H__
#endif

pthread_t workers[N_WORKERS];
mqd_t worker_queues[N_WORKERS];

pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

long int global_fd = 0;

int init_workers();

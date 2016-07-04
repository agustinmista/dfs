#ifndef __WORKER_H__
#define __WORKER_H__

// Initialize workers
int init_workers();

// Main worker function
pthread_t workers[N_WORKERS];

// Global worker message queues
mqd_t worker_queues[N_WORKERS];

#endif

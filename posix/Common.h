#ifndef __COMMON_H__
#define __COMMON_H__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>
//#include <mqueue.h>

#define ERROR(s) (exit((perror(s), -1)))

#define N_WORKERS 5

typedef enum {
    LSD,
    DEL,
    CRE,
    OPN,
    WRT,
    REA,
    CLO,
    BYE         
} Operation;

typedef enum {
    BAD_FD,
    BAD_ARG,
    BAD_CMD,
    F_OPEN,
    F_EXIST,
    F_NOTEXIST,        
} Error;

typedef struct _Session {
	int socket;
	int client_id;
	int worker_id;
//	mqd_t worker_queue;
//	mqd_t client_queue;
} Session;

typedef struct _File {
	char name[64];     
	char *content;     
	int fd;            
	int open;      // -1 of closed, client_id otherwise
	int cursor;
	int size;
	struct _File *next;	
} File;

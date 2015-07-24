#ifndef __COMMON_H__
#define __COMMON_H__
#endif
#define _GNU_SOURCE  //elimina el warning al usar asprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>
#include <mqueue.h>

#define ERROR(s) (exit((perror(s), -1)))

#define N_WORKERS 5
#define MSG_SIZE 4096 //VER size

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
	int client_id;
	int worker_id;
	mqd_t worker_queue;
	mqd_t client_queue;
} Session;

typedef struct _File {
	char name[64];     
	char *content;     
	int fd;            
	int open;      // -1 if closed, client_id otherwise
	int cursor;
	int size;
	struct _File *next;	
} File;

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

#define ERROR(s)    exit((perror(s), -1))

#define N_WORKERS 5
#define MAX_MESSAGES 10
#define MSG_SIZE 1024 //VER size
#define MAX_PRIORITY 32768


typedef enum _Operation {
    LSD,
    DEL,
    CRE,
    OPN,
    WRT,
    REA,
    CLO,
    BYE         
} Operation;

typedef enum _Error {
    NONE,
    BAD_FD,
    BAD_ARG,
    F_OPEN,
    F_EXIST,
    F_CLOSED,
    F_NOTEXIST      
} Error;

typedef struct _Session {
	int client_id;
	int worker_id;
	mqd_t worker_queue;
	mqd_t client_queue;
} Session;

typedef struct _File {
	char *name; //32
	int fd;            
	int open;      // -1 if closed, client_id otherwise
	int cursor;
	int size;
    char *content;
	struct _File *next;	
} File;

typedef struct _Worker_Info {
	int id;
	mqd_t *queue;
	File *files;
} Worker_Info;

typedef struct _Request {
	Operation op;
	int origin; //0 internas, >0 (1 preferentemente) externas
	char *arg0;
	char *arg1;
	char *arg2;
	int client_id;
	mqd_t *client_queue;
} Request;

typedef struct _Reply {
	Error err;
	char *answer;
} Reply;

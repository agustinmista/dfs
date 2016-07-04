#ifndef __COMMON_H__
#define __COMMON_H__

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>
#include <mqueue.h>

#include "FDPool.h"

#define ERROR(s)    exit((perror(s), -1))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define N_WORKERS       5
#define MAX_MESSAGES    50
#define MAX_FILES       50
#define INIT_FD         100
#define MAX_OPEN_FILES  10
#define MSG_SIZE        1024
#define F_NAME_SIZE     64
#define F_CONTENT_SIZE  4096
#define n_Files_Worker  5


/*
*   Supported operations
*/
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

/*
*   Operation errors
*/
typedef enum _Error {
    NONE,
    BAD_FD,
    BAD_ARG,
    F_OPEN,
    F_EXIST,
    F_CLOSED,
    F_NOTEXIST,
    F_NOTSPACE,
    F_TOOMANY,
    NOT_IMP
} Error;

/*
*   User sessions
*/
typedef struct _Session {
	int client_id;
	int worker_id;
	mqd_t worker_queue;
	mqd_t *client_queue;
} Session;

/*
*   Files
*/
typedef struct _File {
	char *name;
	int fd;
	int open;      // -1 if closed, client_id otherwise
	int cursor;
	int size;
    char *content;
	struct _File *next;
} File;

/*
*   Workers information
*/
typedef struct _Worker_Info {
	int id;
	mqd_t *queue;
	FDPool *fd_pool;
	File *files;
} Worker_Info;

/*
*   Requests between clients and workers
*/
typedef struct _Request {
	Operation op;
	int external; 		// != 0 if external request
	int main_worker; 	//worker who received the external request
	char *arg0;
	char *arg1;
	char *arg2;
	int client_id;
	mqd_t *client_queue;
} Request;

/*
*  Answers from workers to clients 
*/
typedef struct _Reply {
	Error err;
	char *answer;
} Reply;

#endif

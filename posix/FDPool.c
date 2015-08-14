#include "Common.h"
#include "FDPool.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

FDPool *createFDPool(int size){
    FDPool *newPool = malloc(sizeof(FDPool));
    newPool->arr = malloc((size/8)+1);
    memset(newPool->arr, 0, size/8);
    newPool->size = size;
    
    return newPool;
}

void printFDPool(FDPool *pool){    
    for(int i=0; i<pool->size; i++){
        if(IS_SET(pool->arr, i))       
            printf("|" COLOR_RED "%d" COLOR_RESET, i);
        else 
            printf("|" COLOR_GREEN "%d" COLOR_RESET, i);
    }
    printf("|\n");
}

int newFD(FDPool *pool){
    for(int i=0; i<pool->size; i++){
        if(!IS_SET(pool->arr, i)){
            // Protect the array asignation with the mutex
            pthread_mutex_lock(&mutex);
            SET(pool->arr, i);
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    return -1;
}

int freeFD(FDPool *pool, int fd){
    CLEAR(pool->arr, fd);
    return -1;
}


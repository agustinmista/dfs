#include "Common.h"

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
    int i=0;
    
    // Protect the array while searching for first free fd
    pthread_mutex_lock(&mutex);
    while(i<pool->size && IS_SET(pool->arr, i)) i++;
    pthread_mutex_unlock(&mutex);
    
    if(i<pool->size){
        SET(pool->arr, i);
        return i;
    } else return -1;
}

int freeFD(FDPool *pool, int fd){
    CLEAR(pool->arr, fd);
    return -1;
}


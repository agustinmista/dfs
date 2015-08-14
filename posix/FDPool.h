#ifndef __FDPOOL_H__
#define __FDPOOL_H__
#endif

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"

#define IS_SET(arr, k)  (arr[k/8] &   (1<<(k%8)))
#define SET(arr, k)     (arr[k/8] |=   1<<(k%8) )
#define CLEAR(arr, k)   (arr[k/8] &= ~(1<<(k%8)))

typedef struct _FDPool{
    char *arr;
    int size;
} FDPool;

FDPool *createFDPool(int size);

void printFDPool(FDPool *pool);

int newFD(FDPool *pool);

void freeFD(FDPool *pool, int fd);
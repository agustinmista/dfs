#ifndef __FDPOOL_H__
#define __FDPOOL_H__
#endif

// ANSI Colors 
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"

// Bitwise manipulation macros
#define IS_SET(arr, k)  (arr[k/8] &   (1<<(k%8)))
#define SET(arr, k)     (arr[k/8] |=   1<<(k%8) )
#define CLEAR(arr, k)   (arr[k/8] &= ~(1<<(k%8)))

/*
 * File Descriptor Pool
 */
typedef struct _FDPool{
    char *arr;
    int size;
} FDPool;

// Create a FDPool with size elements
FDPool *createFDPool(int size);

// Print an FDPool status (with colours!)
void printFDPool(FDPool *pool);

// Find a new FD from pool, return -1 if full
int newFD(FDPool *pool);

// Remove an FD from pool
int freeFD(FDPool *pool, int fd);

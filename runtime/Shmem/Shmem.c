//
// Shared memory
//

#include "Shmem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// the log file name
static const char *g_LogFileName = "newcomair_123456789";

// the file descriptor of the shared memory, need to be closed at the end
static int fd = -1;

// the buffer size of the shared memory
#define BUFFERSIZE (1UL << 33)

// the ptr to the buffer of the shared memory
static unsigned long *pcBuffer = NULL;

// the current offset of the shared memory
static unsigned  long iBufferIndex = 0;

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 */
void InitMemHooks() {
    fd = shm_open(g_LogFileName, O_RDWR | O_CREAT, 0777);
    if (fd == -1) {
        fprintf(stderr, "shm_open failed:%s\n", strerror(errno));
        exit(-1);
    }
    if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "fstruncate failed:%s\n", strerror(errno));
        exit(-1);
    }
    pcBuffer = (unsigned long *)mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL) {
        fprintf(stderr, "mmap failed:%s\n", strerror(errno));
        exit(-1);
    }
}

/**
 * Truncate the shared memory buffer to the actual data size, then close.
 */
void FinalizeMemHooks() {
    if (ftruncate(fd, iBufferIndex) == -1) {
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));
        exit(-1);
    }
    close(fd);
}

/**
 * Write the begin address and length of the accessed memory to shared memory buffer.
 * @param beginAddress the begin address of the accessed memory
 * @param length the length of the accessed memory
 */
void RecordMemHooks(void *beginAddress, unsigned long length) {
    unsigned long beginNum = (unsigned long)beginAddress;

//    static unsigned long lastBeginNum = 0;
//    static unsigned long lastLength = 0;
//
//    if (lastLength == length) {
//        return;
//    }
//
//    pcBuffer[iBufferIndex++] = lastBeginNum = beginNum;
//    pcBuffer[iBufferIndex++] = lastLength = length;
//
    pcBuffer[iBufferIndex++] = beginNum;
    pcBuffer[iBufferIndex++] = length;
}

/**
 * Write the exec times of the cloned Loop to shared memory buffer.
 * @param cost the exec times of the cloned Loop
 *
 * The first element is set to zero to distinguish from the RecordMemHook output.
 */
void RecordCostHooks(unsigned long cost) {
    pcBuffer[iBufferIndex++] = 0UL;
    pcBuffer[iBufferIndex++] = cost;
}
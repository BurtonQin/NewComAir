#ifndef NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H
#define NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H

#include <stdio.h>

#define BUFFERSIZE (1UL << 33)

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer);

int closeSharedMem(const char *sharedMemName, int fd, bool clearData = false);

int openDebugLogFile(const char *logFileName, FILE *pFile);

int closeDebugLogFile(FILE *pFile);

int readStride(const char *indvarFileName, int &stride);


#endif //NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H

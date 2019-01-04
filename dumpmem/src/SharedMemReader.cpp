#include "SharedMemReader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ParseRecord.h"

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer) {

    fd = shm_open(sharedMemName, O_RDWR, 07777);
    if (fd == -1) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return errno;
    }
    if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "fstruncate failed: %s\n", strerror(errno));
        return errno;
    }
    pcBuffer = (char *) mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return errno;
    }

    return 0;
}

int closeSharedMem(const char *sharedMemName, int fd, bool clearData) {
    if (clearData) {
        if (ftruncate(fd, 0) == -1) {
            fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
            return errno;
        }
        if (shm_unlink(sharedMemName) == -1) {
            fprintf(stderr, "shm_unlink failed: %s\n", strerror(errno));
            return errno;
        }
    }
    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return errno;
    }
    return 0;
}

int openDebugLogFile(const char *logFileName, FILE *pFile) {
    pFile = fopen(logFileName, "w");
    if (pFile == nullptr) {
        fprintf(stderr, "file open failed: %s\n", strerror(errno));
        return errno;
    }
    return 0;
}

int closeDebugLogFile(FILE *pFile) {
    return fclose(pFile);
}

int readStride(const char *indvarFileName, int &stride) {

    FILE *pFile = fopen(indvarFileName, "r");
    if (pFile == nullptr) {
        fprintf(stderr, "file open failed: %s\n", strerror(errno));
        return errno;
    }
    const
    unsigned LINE_BUFFER_SIZE = 256;
    char line[LINE_BUFFER_SIZE];
    if (fgets(line, sizeof(line), pFile)) {  // the first line is indvar name
        if (fgets(line, sizeof(line), pFile)) {  // the second line is stride
            stride = abs(atoi(line));
            return 0;
        }
    }
    fprintf(stderr, "file format error\n");
    return -1;
}

int main(int argc, char *argv[]) {

    static char g_LogFileName[] = "newcomair_123456789";
    static char g_IndvarInfo[] = "/home/boqin/Projects/NewComAir/stubs/apache34464/build/indvar.info";

    char *sharedMemName;
    char *indvarInfoPath;

    if (argc >= 2) {
        sharedMemName = argv[1];
        indvarInfoPath = argv[2];
    } else {
        sharedMemName = g_LogFileName;
        indvarInfoPath = g_IndvarInfo;
    }
    int fd = 0;
    char *pcBuffer = NULL;
    auto err = openSharedMem(sharedMemName, fd, pcBuffer);
    if (err != 0) {
        return err;
    }
    int stride;
    err = readStride(indvarInfoPath, stride);
    if (err != 0) {
        return err;
    }
//#ifdef DEBUG
//    FILE *pFile;
//    openDebugLogFile(sharedMemName, pFile);
//#endif
//
//#ifdef DEBUG
//    parseRecord(pcBuffer, stride, pFile);
//#else
    parseRecord(pcBuffer, stride, NULL);
//#endif
//
//#ifdef DEBUG
//    closeDebugLogFile(pFile);
//#endif
//
    closeSharedMem(sharedMemName, fd);
}
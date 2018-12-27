#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <set>
#include <algorithm>

struct struct_stMemRecord {
    unsigned long address;
    unsigned length;
    unsigned flag;
} record;

#define BUFFERSIZE (1UL << 33)

static const char *g_LogFileName = "newcomair_123456789";

int DumpSharedMemory(bool enableDebug) {

    int fd = shm_open(g_LogFileName, O_RDWR, 07777);
    if (fd == -1) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return errno;
    }
    if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "fstruncate failed: %s\n", strerror(errno));
        return errno;
    }
    char *pcBuffer = (char *) mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return errno;
    }

    FILE *pFile;
    if (enableDebug) {
        pFile = fopen(g_LogFileName, "w");
        if (pFile == NULL) {
            fprintf(stderr, "file open failed: %s\n", strerror(errno));
            return errno;
        }
    }

    // init to non-0
    record.flag = 1;
    unsigned loopNum = 0;

    bool endFlag = false;

    std::map<unsigned long, int> one_loop_record;  // <address, flag>
    std::set<unsigned long> one_loop_distinct_addr;  // Ci: ith Distinct First Load Address
    std::set<unsigned long> all_distinct_addr;  // Mi: i-1 Distinct First Load Addresses

    unsigned long sumOfMiCi = 0;
    unsigned long sumOfRi = 0;
    unsigned cost = 0;

    for (unsigned long i = 0; !endFlag; i += 16UL) {
        memcpy(&record, &pcBuffer[i], 16);
        if (enableDebug) {
            fprintf(pFile, "%lu, %u, %u\n", record.address, record.length, record.flag);
        }
        switch (record.flag) {
            case 0: {  // dump cost or end
                endFlag = true;
                // if dump cost, not end
                if (record.address == 0UL && record.length != 0U) {
                    cost = record.length;
                    if (enableDebug) {
                        printf("cost: %u\n", cost);
                    }

                    if (loopNum > 0) {
                        if (enableDebug) {
                            printf("Loop: %u\n", loopNum);
                        }
                        for (auto &kv : one_loop_record) {
                            if (kv.second == 2) {
                                one_loop_distinct_addr.insert(kv.first);
                            }
                        }
                        // calc
                        sumOfMiCi += all_distinct_addr.size() * one_loop_distinct_addr.size();

                        if (enableDebug) {
                            printf("sumOfMiCi: %lu, Mi: %lu, Ci: %lu\n", sumOfMiCi, all_distinct_addr.size(),
                                   one_loop_distinct_addr.size());
                        }
                        std::set<unsigned long> intersect;
                        std::set_intersection(all_distinct_addr.begin(), all_distinct_addr.end(),
                                              one_loop_distinct_addr.begin(), one_loop_distinct_addr.end(),
                                              std::inserter(intersect, intersect.begin()));
                        sumOfRi += intersect.size();

                        if (enableDebug) {
                            printf("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size());
                        }
                        // merge
                        all_distinct_addr.insert(one_loop_distinct_addr.begin(), one_loop_distinct_addr.end());

                        // clear
                        one_loop_record.clear();
                        one_loop_distinct_addr.clear();
                    }
                    if (enableDebug) {
                        printf("end\n");
                    }
                }
                break;
            }
            case 1: { // delimit
                // begin a new loop record
                if (loopNum > 0) {
                    if (enableDebug) {
                        printf("Loop: %u\n", loopNum);
                    }
                    for (auto &kv : one_loop_record) {
                        if (kv.second == 2) {
                            one_loop_distinct_addr.insert(kv.first);
                        }
                    }
                    // calc
                    sumOfMiCi += all_distinct_addr.size() * one_loop_distinct_addr.size();

                    if (enableDebug) {
                        printf("sumOfMiCi: %lu, Mi: %lu, Ci: %lu\n", sumOfMiCi, all_distinct_addr.size(),
                               one_loop_distinct_addr.size());
                    }
                    std::set<unsigned long> intersect;
                    std::set_intersection(all_distinct_addr.begin(), all_distinct_addr.end(),
                                          one_loop_distinct_addr.begin(), one_loop_distinct_addr.end(),
                                          std::inserter(intersect, intersect.begin()));
                    sumOfRi += intersect.size();

                    if (enableDebug) {
                        printf("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size());
                    }
                    // merge
                    all_distinct_addr.insert(one_loop_distinct_addr.begin(), one_loop_distinct_addr.end());

                    // clear
                    one_loop_record.clear();
                    one_loop_distinct_addr.clear();
                }
                loopNum++;
                break;
            }
            case 2: {  // load
                // new then set to 2
                for (unsigned j = 0; j < record.length; j += 8) {
                    if (one_loop_record[record.address + j] == 0) {
                        one_loop_record[record.address + j] = 2;
                    }
                }
                break;
            }

            case 3:  // store
                // new then set to 3
                for (unsigned j = 0; j < record.length; j += 8) {
                    if (one_loop_record[record.address + j] == 0) {
                        one_loop_record[record.address + j] = 3;
                    }
                }
                break;

            default:  // others
                fprintf(stderr, "shared memory buffer abnormal data");
                endFlag = true;
                if (enableDebug) {
                    printf("end\n");
                }
                break;
        }
    }

    if (sumOfRi == 0) {
        if (enableDebug) {
            printf("sumOfMiCi=%lu, sumOfRi=%lu, cost=%u\n", sumOfMiCi, sumOfRi, cost);
        } else {
            printf("%u,%u\n", 0, cost);
        }
    } else {
        if (enableDebug) {
            printf("sumOfMiCi=%lu, sumOfRi=%lu, N=%lu, cost=%u\n", sumOfMiCi, sumOfRi, sumOfMiCi / sumOfRi, cost);
        } else {
            printf("%lu,%u\n", sumOfMiCi / sumOfRi, cost);
        }
    }

    if (enableDebug) {
        fclose(pFile);
    }
    if (!enableDebug) {
        if (ftruncate(fd, 0) == -1) {
            fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
            return errno;
        }
        if (shm_unlink(g_LogFileName) == -1) {
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

int main(int argc, char *argv[]) {
    if (argc > 1 && strncmp(argv[1], "-d", 3) == 0) {
        return DumpSharedMemory(true);
    } else {
        return DumpSharedMemory(false);
    }
}

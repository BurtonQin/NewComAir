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

int DumpSharedMemory() {

	int fd = shm_open(g_LogFileName, O_RDWR, 07777);
	if (fd == -1) {
		fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        exit(-1);
	}
	if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "fstruncate failed: %s\n", strerror(errno));
        exit(-1);
    }
	char *pcBuffer = (char *)mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(-1);
    }

    FILE *pFile = fopen(g_LogFileName, "w");
    if (pFile == NULL) {
		fprintf(stderr, "file open failed: %s\n", strerror(errno));
		exit(-1);
    }

	// init to non-0
	record.flag = 1;
    unsigned loopNum = 0;

    bool endFlag = false;

	std::map<unsigned long, int> one_loop_record;  // <address, flag>
	std::set<unsigned long> one_loop_distinct_addr;  // Ci: ith Distinct First Load Address
	std::set<unsigned long> all_distinct_addr;  // Mi: i-1 Distinct First Load Addresses

	unsigned sumOfMiCi = 0;
	unsigned sumOfRi = 0;

	for (unsigned long i = 0; !endFlag; i+=16UL) {
		memcpy(&record, &pcBuffer[i], 16);
		fprintf(pFile, "%lu, %u, %u\n", record.address, record.length, record.flag);
		switch (record.flag) {
			case 1: { // delimit
				// begin a new loop record
				if (loopNum > 0) {
					printf("Loop: %u\n", loopNum);
					for (auto &kv : one_loop_record) {
						if (kv.second == 2) {
							one_loop_distinct_addr.insert(kv.first);
						}
					}
					// calc
					sumOfMiCi += all_distinct_addr.size() * one_loop_distinct_addr.size();

					printf("sumOfMiCi: %lu, Mi: %lu, Ci: %lu\n", sumOfMiCi, all_distinct_addr.size(), one_loop_distinct_addr.size());

					std::set<unsigned long> intersect;
					std::set_intersection(all_distinct_addr.begin(), all_distinct_addr.end(), one_loop_distinct_addr.begin(), one_loop_distinct_addr.end(), std::inserter(intersect, intersect.begin()));
					sumOfRi += intersect.size();

					printf("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size());

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
				endFlag = true;
				if (loopNum > 0) {
					printf("Loop: %u\n", loopNum);
					for (auto &kv : one_loop_record) {
						if (kv.second == 2) {
							one_loop_distinct_addr.insert(kv.first);
						}
					}
					// calc
					sumOfMiCi += all_distinct_addr.size() * one_loop_distinct_addr.size();

					printf("sumOfMiCi: %lu, Mi: %lu, Ci: %lu\n", sumOfMiCi, all_distinct_addr.size(), one_loop_distinct_addr.size());

					std::set<unsigned long> intersect;
					std::set_intersection(all_distinct_addr.begin(), all_distinct_addr.end(), one_loop_distinct_addr.begin(), one_loop_distinct_addr.end(), std::inserter(intersect, intersect.begin()));
					sumOfRi += intersect.size();

					printf("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size());

					// merge
					all_distinct_addr.insert(one_loop_distinct_addr.begin(), one_loop_distinct_addr.end());

					// clear
					one_loop_record.clear();
					one_loop_distinct_addr.clear();
				}
				printf("end\n");
				break;
		}
	}

	if (sumOfRi == 0) {
		printf("sumOfMiCi=%u, sumOfRi=%u\n", sumOfMiCi, sumOfRi);
	} else {
		printf("sumOfMiCi=%u, sumOfRi=%u, N=%u\n", sumOfMiCi, sumOfRi, sumOfMiCi/sumOfRi);
	}

	fclose(pFile);
	close(fd);

	return 0;
}

int main() {
	return DumpSharedMemory();
}

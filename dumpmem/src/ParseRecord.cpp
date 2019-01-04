#include "ParseRecord.h"

#include <string.h>

#include <set>
#include <algorithm>

#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

static int
parseOneLoop(unsigned long beginAddr, unsigned long endAddr, OneLoopRecordTy &oneLoopRecord, unsigned byteNum,
             int stride) {

    unsigned byteCounter = 0;
    for (auto addr = beginAddr; addr <= endAddr; addr += 1) {
        if (oneLoopRecord[addr] == OneLoopRecordFlag::Uninitialized) {
            oneLoopRecord[addr] = OneLoopRecordFlag::FirstLoad;
        }
        ++byteCounter;
        if (byteCounter == byteNum) {
            byteCounter = 0;
            addr += byteNum * 8 * (stride - 1);
        }
    }
    return 0;
}

static void calcMiCi(OneLoopRecordTy &oneLoopRecord, std::set<unsigned long> &allDistinctAddr, unsigned long &sumOfMiCi,
                     unsigned long &sumOfRi) {

    std::set<unsigned long> oneLoopDistinctAddr;
    for (auto &kv : oneLoopRecord) {
        if (kv.second == OneLoopRecordFlag::FirstLoad) {
            oneLoopDistinctAddr.insert(kv.first);
        }
    }

    sumOfMiCi += allDistinctAddr.size() * oneLoopDistinctAddr.size();

    DEBUG_PRINT(("sumOfMiCi: %lu, Mi: %lu, Ci: %lu\n", sumOfMiCi, allDistinctAddr.size(), oneLoopDistinctAddr.size()));

    std::set<unsigned long> intersect;
    std::set_intersection(allDistinctAddr.begin(), allDistinctAddr.end(), oneLoopDistinctAddr.begin(),
                          oneLoopDistinctAddr.end(), std::inserter(intersect, intersect.begin()));
    sumOfRi += intersect.size();

    DEBUG_PRINT(("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size()));

    allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());

    oneLoopRecord.clear();
}

void parseRecord(char *pcBuffer, int stride, FILE *pFile) {

    if (pcBuffer == NULL) {
        printf("NULL buffer\n");
    }

    bool endFlag = false;
    struct_stMemRecord record;

    unsigned long beginAddr;
    unsigned long endAddr;
    unsigned byteNum = 0;

    OneLoopRecordTy oneLoopRecord;  // <address, OneLoopRecordFlag>
    std::set<unsigned long> oneLoopDistinctAddr;  // Ci: ith Distinct First Load Address
    std::set<unsigned long> allDistinctAddr;  // Mi: i-1 Distinct First Load Addresses

    unsigned long sumOfMiCi = 0;
    unsigned long sumOfRi = 0;

    for (unsigned long i = 0; !endFlag; i += 16UL) {
        memcpy(&record, &pcBuffer[i], sizeof(record));
        if (pFile) {
            fprintf(pFile, "%lu, %u, %u\n", record.address, record.length, record.flag);
        }

        switch (record.flag) {
            case RecordFlag::Terminator: {
                endFlag = true;
                if (i > 0) {  // skip the first loop (loop begins with delimiter)
                    if (record.address == 0UL && record.length != 0U) {
                        auto cost = record.length;
                        DEBUG_PRINT(("cost: %u\n", cost));
                        parseOneLoop(beginAddr, endAddr, oneLoopRecord, byteNum, stride);
                        calcMiCi(oneLoopRecord, allDistinctAddr, sumOfMiCi, sumOfRi);
                        if (sumOfRi == 0) {
                            DEBUG_PRINT(("sumOfMiCi=%lu, sumOfRi=%lu, cost=%u\n", sumOfMiCi, sumOfRi, cost));
                            printf("%u,%u\n", 0, cost);
                        } else {
                            DEBUG_PRINT(("sumOfMiCi=%lu, sumOfRi=%lu, N=%lu, cost=%u\n", sumOfMiCi, sumOfRi, sumOfMiCi /
                                                                                                             sumOfRi, cost));
                            printf("%lu,%u\n", sumOfMiCi / sumOfRi, cost);
                        }
                    } else {
                        fprintf(stderr, "Wrong cost format\n");
                    }
                }
                break;
            }
            case RecordFlag::Delimiter: {
                if (i > 0) {  // skip the first loop (loop begins with delimiter)
                    parseOneLoop(beginAddr, endAddr, oneLoopRecord, byteNum, stride);
                    calcMiCi(oneLoopRecord, allDistinctAddr, sumOfMiCi, sumOfRi);
                }
                break;
            }
            case RecordFlag::LoadFlag: {
                for (unsigned j = 0; j < record.length; j += 8) {
                    if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                        oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstLoad;
                    }
                }
                break;
            }
            case RecordFlag::StoreFlag: {
                for (unsigned j = 0; j < record.length; j += 8) {
                    if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                        oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstStore;
                    }
                }
                break;
            }
            case RecordFlag::LoopBeginFlag: {
                beginAddr = record.address;
                if (byteNum == 0) {
                    byteNum = record.length / 8;
                }
                break;
            }
            case RecordFlag::LoopEndFlag: {
                endAddr = record.address;
                break;
            }
            default: {
                break;
            }
        }
    }
}
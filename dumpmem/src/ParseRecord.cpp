#include "ParseRecord.h"

#include <string.h>

#include <set>
//#include <deque>
#include <algorithm>
#include <limits.h>
#include <assert.h>

#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

constexpr unsigned INVALID_ID = 0;
constexpr unsigned MIN_ID = 1;
constexpr unsigned DELIMIT = INT_MAX;
constexpr unsigned LOOP_BEGIN = INT_MAX - 1;
constexpr unsigned LOOP_END = INT_MAX - 2;
constexpr unsigned MAX_ID = INT_MAX - 3;

struct IndvarInfo {
    struct_stMemRecord beginRecord;
    struct_stMemRecord endRecord;
    int stride;
};

typedef std::vector<IndvarInfo> VecIndvarInfoTy;

static bool isIndvar(unsigned uID, const std::map<unsigned, int> &mapIndvarStride) {
    return mapIndvarStride.find(uID) != mapIndvarStride.end();
}

static int getStride(unsigned uID, const std::map<unsigned, int> &mapIndvarStride) {
    const auto it = mapIndvarStride.find(uID);
    if (it != mapIndvarStride.end()) {
        return it->second;
    } else {
        return 0;
    }
}

struct ArrayBeginEnd {
    unsigned long begin;
    unsigned long end;
    unsigned length;
    int stride;

    bool check() const {
        if (length == 0) {
            fprintf(stderr, "length == 0\n");
            return false;
        }
        if (stride == 0) {
            fprintf(stderr, "stride == 0\n");
            return false;
        }
        if ((end - begin) / stride <= 0) {
            fprintf(stderr, "(end - begin) / stride <= 0\n");
            return false;
        }
        return true;
    }

    bool add(OneLoopRecordTy &oneLoopRecord) const {
        if (!check()) {
            return false;
        }
        // make sure begin < end and stride > 0
        unsigned long tmpBegin = begin;
        unsigned long tmpEnd = end;
        int tmpStride = stride;
        if (tmpStride < 0) {
            tmpStride = -tmpStride;
            unsigned long temp = tmpBegin;
            tmpBegin = tmpEnd;
            tmpEnd = temp;
        }

        unsigned byteNum = length / 8;
        unsigned byteCounter = 0;

        // Assume that stride == 2
        //        Addr |0|1|2|3|x|x|x|x|8|9|10|11|...
        // byteCounter |1|2|3|4|------>|1|2|3 |4 |...

        for (unsigned long addr = tmpBegin; addr <= tmpEnd; addr += 1) {
            if (oneLoopRecord[addr] == OneLoopRecordFlag::Uninitialized) {
                oneLoopRecord[addr] = OneLoopRecordFlag::FirstLoad;
            }

            ++byteCounter;
            // Move byte num of (stride-1)
            if (byteCounter == byteNum) {
                byteCounter = 0;
                addr += byteNum * (tmpStride - 1);
            }
        }

        return true;
    }
};

static bool collectIndvar(const struct_stMemRecord &record, const std::map<unsigned, int> &mapIndvarStride, std::map<unsigned, ArrayBeginEnd> &mapArrayInfo) {
    int id = record.id;
    if (id < 0) {
        id = -id;
    }
    unsigned uID = (unsigned)id;
    if (uID == INVALID_ID || MIN_ID > uID || uID > MAX_ID) {
        return false;
    }
    if (!isIndvar(uID, mapIndvarStride)) {
        return false;
    }

    auto it = mapArrayInfo.find(uID);
    if (it == mapArrayInfo.end()) {  // Begin
        ArrayBeginEnd arrayBeginEnd;
        arrayBeginEnd.begin = record.address;
        arrayBeginEnd.length = record.length;
        arrayBeginEnd.stride = getStride(uID, mapIndvarStride);
        mapArrayInfo[uID] = arrayBeginEnd;
    } else {  // end
        if (it->second.length != record.length) {
            fprintf(stderr, "Inst %u : Begin length != End length", uID);
            return false;
        }
        it->second.end = record.address;
    }

    return true;
}

static void calcIndvar(const std::map<unsigned, ArrayBeginEnd> &mapArrayInfo, OneLoopRecordTy &oneLoopRecord) {
    for (auto &kv : mapArrayInfo) {
        kv.second.add(oneLoopRecord);
    }
}

// parseOneLoop gets indvar address access
// It is called after non-indvar address access calculated
// Here it is assumed that there is no address access conflicts between "Load indvar" and "Store non-indvar"
// If it is not satisfied (which is rare), then the logic should be re-checked.
//static void parseOneLoop(const VecIndvarInfoTy &vecIndvarInfo, OneLoopRecordTy &oneLoopRecord) {
//
//    for (const auto &indvarInfo : vecIndvarInfo) {
//
//        auto stride = indvarInfo.stride;
//        if (stride == 0) {
//            fprintf(stderr, "Stride == 0\n");
//            continue;
//        }
//
//        if (stride < 0) {
//            stride = -stride;
//        }
//
//        auto beginLength = indvarInfo.beginRecord.length;
//        auto endLength = indvarInfo.endRecord.length;
//
//        if (beginLength != endLength) {
//            fprintf(stderr, "Indvar Begin Length != Indvar End Length\n");
//            continue;
//        }
//
//        auto &length = beginLength;
//
//        auto byteNum = length / 8;
//
//        auto beginAddr = indvarInfo.beginRecord.address;
//        auto endAddr = indvarInfo.endRecord.address;
//
//        if (beginAddr > endAddr) {
//            auto tmp = beginAddr;
//            beginAddr = endAddr;
//            endAddr = tmp;
//        }
//
//        unsigned byteCounter = 0;
//
//        // Assume that stride == 2
//        //        Addr |0|1|2|3|x|x|x|x|8|9|10|11|...
//        // byteCounter |1|2|3|4|------>|1|2|3 |4 |...
//
//        for (auto addr = beginAddr; addr <= endAddr; addr += 1) {
//            if (oneLoopRecord[addr] == OneLoopRecordFlag::Uninitialized) {
//                oneLoopRecord[addr] = OneLoopRecordFlag::FirstLoad;
//            }
//
//            ++byteCounter;
//            // Move byte num of (stride-1)
//            if (byteCounter == byteNum) {
//                byteCounter = 0;
//                addr += byteNum * (stride - 1);
//            }
//        }
//    }
//}

static void calcMiCi(OneLoopRecordTy &oneLoopRecord, std::set<unsigned long> &allDistinctAddr, unsigned long &sumOfMiCi,
                     unsigned long &sumOfRi, unsigned long &oneLoopIOFuncSize, unsigned long &allIOFuncSize) {

    std::set<unsigned long> oneLoopDistinctAddr;
    for (auto &kv : oneLoopRecord) {
        if (kv.second == OneLoopRecordFlag::FirstLoad) {
            oneLoopDistinctAddr.insert(kv.first);
        }
    }

    if (allDistinctAddr.empty()) {
        allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());
        allIOFuncSize = oneLoopIOFuncSize;
    }

    sumOfMiCi += (allDistinctAddr.size() + allIOFuncSize) * (oneLoopDistinctAddr.size() + oneLoopIOFuncSize);

    DEBUG_PRINT(
            ("sumOfMiCi: %lu, Mi: %lu+%lu, Ci: %lu+%lu\n", sumOfMiCi, allDistinctAddr.size(), allIOFuncSize, oneLoopDistinctAddr.size(), oneLoopIOFuncSize));

    std::set<unsigned long> intersect;
    std::set_intersection(allDistinctAddr.begin(), allDistinctAddr.end(), oneLoopDistinctAddr.begin(),
                          oneLoopDistinctAddr.end(), std::inserter(intersect, intersect.begin()));
    sumOfRi += intersect.size();

    DEBUG_PRINT(("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size()));

    allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());

    oneLoopRecord.clear();
    allIOFuncSize += oneLoopIOFuncSize;
    oneLoopIOFuncSize = 0;
}

void parseRecord(char *pcBuffer, const std::map<unsigned, int> &mapIndvarStride, FILE *pDebugFile) {

    if (pcBuffer == nullptr) {
        printf("NULL buffer\n");
    }

    bool endFlag = false;

//    VecIndvarInfoTy vecIndvarInfo;
//
//    std::deque<int> dqInitStride(vecStride.begin(), vecStride.end());
//    std::deque<int> dqStride = dqInitStride;

    struct_stMemRecord record{0UL, 0U, RecordFlag::Terminator};
    // First indvarBegin, first indvarEnd
//    std::deque<struct_stMemRecord> dqIndvarBeginRecord;
    std::map<unsigned, ArrayBeginEnd> mapArrayInfo;

    OneLoopRecordTy oneLoopRecord;  // <address, OneLoopRecordFlag>
    std::set<unsigned long> oneLoopDistinctAddr;  // Ci: ith Distinct First Load Address
    unsigned long oneLoopIOFuncSize = 0;  // when used, clear
    unsigned long allIOFuncSize = 0;

    std::set<unsigned long> allDistinctAddr;  // Mi: i-1 Distinct First Load Addresses

    unsigned long sumOfMiCi = 0;
    unsigned long sumOfRi = 0;

    for (unsigned long i = 0; !endFlag; i += 16UL) {
        memcpy(&record, &pcBuffer[i], sizeof(record));
        if (pDebugFile) {
            fprintf(pDebugFile, "%lu, %u, %d\n", record.address, record.length, record.id);
        }

        RecordFlag flag;
        if (record.id == INVALID_ID) {
            flag = RecordFlag::Terminator;
        } else if (record.id == DELIMIT) {
            flag = RecordFlag::Delimiter;
        } else if (collectIndvar(record, mapIndvarStride, mapArrayInfo)) {
            flag = RecordFlag::LoopFlag;
        } else if (record.id > 0) {
            flag = RecordFlag::LoadFlag;
        } else if (record.id < 0) {
            flag = RecordFlag::StoreFlag;
        }

        switch (flag) {
            case RecordFlag::Terminator: {
                endFlag = true;
                if (i > 0) {  // skip the first loop (loop begins with delimiter)
                    if (record.address != 0UL && record.length == 0U) {
                        auto cost = record.address;
                        DEBUG_PRINT(("cost: %u\n", cost));
//                        parseOneLoop(vecIndvarInfo, oneLoopRecord);
//                        vecIndvarInfo.clear();
                        calcIndvar(mapArrayInfo, oneLoopRecord);
                        mapArrayInfo.clear();
                        calcMiCi(oneLoopRecord, allDistinctAddr, sumOfMiCi, sumOfRi, oneLoopIOFuncSize, allIOFuncSize);
//                        dqStride = dqInitStride;
                        if (sumOfRi == 0) {
                            DEBUG_PRINT(("sumOfMiCi=%lu, sumOfRi=%lu, cost=%lu\n", sumOfMiCi, sumOfRi, cost));
                            printf("%u,%lu\n", 0, cost);
                        } else {
                            DEBUG_PRINT(
                                    ("sumOfMiCi=%lu, sumOfRi=%lu, N=%lu, cost=%lu\n", sumOfMiCi, sumOfRi, sumOfMiCi /
                                                                                                          sumOfRi, cost));
                            printf("%lu,%lu\n", sumOfMiCi / sumOfRi, cost);
                        }
                    } else {
                        fprintf(stderr, "cost=%lu, record=%u\n", record.address, record.length);
                        fprintf(stderr, "Wrong cost format\n");
                    }
                }
                break;
            }
            case RecordFlag::Delimiter: {
                if (i > 0) {  // skip the first loop (loop begins with delimiter)
//                    parseOneLoop(vecIndvarInfo, oneLoopRecord);
//                    vecIndvarInfo.clear();
                    calcIndvar(mapArrayInfo, oneLoopRecord);
                    mapArrayInfo.clear();
                    calcMiCi(oneLoopRecord, allDistinctAddr, sumOfMiCi, sumOfRi, oneLoopIOFuncSize, allIOFuncSize);
//                    dqStride = dqInitStride;
                }
                break;
            }
            case RecordFlag::LoadFlag: {

                if (record.address == 0UL) {
                    // IO Function update RMS
                    oneLoopIOFuncSize += 1;
                } else {
                    for (unsigned j = 0; j < record.length; j += 8) {
                        if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                            oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstLoad;
                        }
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
//            case RecordFlag::LoopBeginFlag: {
//                dqIndvarBeginRecord.push_back(record);
//                break;
//            }
//            case RecordFlag::LoopEndFlag: {
//
//                // get stride
//                auto stride = dqStride.front();
//                dqStride.pop_front();
//                if (stride == 0) {
//                    fprintf(stderr, "Stride == 0\n");
//                    continue;
//                }
//
//                // get indvar begin
//                auto indvarBeginRecord = dqIndvarBeginRecord.front();
//                dqIndvarBeginRecord.pop_front();
//
//                // check indvar begin and end length
//                auto &indvarEndRecord = record;
//                if (indvarBeginRecord.length != indvarEndRecord.length) {
//                    fprintf(stderr, "Indvar Begin Length != Indvar End Length\n");
//                    continue;
//                }
//
//                IndvarInfo indvarInfo{indvarBeginRecord, record, stride};
//                vecIndvarInfo.push_back(indvarInfo);
//                break;
//            }
            default: {
                break;
            }
        }
    }
}
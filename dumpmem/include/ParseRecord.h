#ifndef NEWCOMAIR_DUMPMEM_PARSERECORD_H
#define NEWCOMAIR_DUMPMEM_PARSERECORD_H

#include <map>
#include <vector>

enum RecordFlag : unsigned {
    Terminator = 0,
    Delimiter = 2147483647,
    LoadFlag,
    StoreFlag,
    LoopBeginFlag,
    LoopEndFlag
};

enum OneLoopRecordFlag : unsigned {
    Uninitialized = 0,
    FirstLoad,
    FirstStore
};

struct struct_stMemRecord {
    unsigned long address;
    unsigned length;
//    RecordFlag flag;
    int id;
};

typedef std::map<unsigned long, OneLoopRecordFlag> OneLoopRecordTy;

void parseRecord(char *pcBuffer, const std::vector<int> &stride, FILE *pFile = nullptr);

#endif //NEWCOMAIR_DUMPMEM_PARSERECORD_H

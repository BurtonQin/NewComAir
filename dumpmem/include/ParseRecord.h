#ifndef NEWCOMAIR_DUMPMEM_PARSERECORD_H
#define NEWCOMAIR_DUMPMEM_PARSERECORD_H

#include <map>

enum RecordFlag : unsigned {
    Terminator = 0,
    Delimiter,
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
    RecordFlag flag;
};

typedef std::map<unsigned long, OneLoopRecordFlag> OneLoopRecordTy;

void parseRecord(char *pcBuffer, int stride, FILE *pFile = NULL);

#endif //NEWCOMAIR_DUMPMEM_PARSERECORD_H

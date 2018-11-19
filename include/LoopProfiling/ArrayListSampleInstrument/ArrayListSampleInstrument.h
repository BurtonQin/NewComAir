//
// Empty Pass demo.
//

#ifndef NEWCOMAIR_ARRAYLISTSAMPLEINSTRUMENT_H
#define NEWCOMAIR_ARRAYLISTSAMPLEINSTRUMENT_H

#include "llvm/Pass.h"

using namespace llvm;

struct ArrayListSampleInstrument : public ModulePass {

    static char ID;

    ArrayListSampleInstrument();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);
};

#endif //NEWCOMAIR_ARRAYLISTSAMPLEINSTRUMENT_H

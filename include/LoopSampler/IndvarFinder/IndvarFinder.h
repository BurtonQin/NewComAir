#ifndef NEWCOMAIR_LOOPSAMPLER_INDVARFINDER_INDVARFINDER_H
#define NEWCOMAIR_LOOPSAMPLER_INDVARFINDER_INDVARFINDER_H

#include <map>
#include <vector>
#include <llvm/Pass.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/Instruction.h>

struct IndvarFinder : public llvm::ModulePass {

    static char ID;

    IndvarFinder();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    virtual bool runOnModule(llvm::Module &M);

private:

    std::map<const llvm::Instruction *, const llvm::SCEV *>
    searchInstSCEV(llvm::Loop *pLoop, llvm::ScalarEvolution &SE);

    const llvm::SCEV *searchStride(const llvm::SCEV *scev, llvm::ScalarEvolution &SE);

};

#endif //NEWCOMAIR_LOOPSAMPLER_INDVARFINDER_INDVARFINDER_H

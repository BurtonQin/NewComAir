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

    void SetupInit(Module &M);

    void SetupTypes();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    // void InstrumentInnerLoop(Loop *pInnerLoop, PostDominatorTree *PDT);
    void InstrumentInnerLoop(Loop *pInnerLoop);

    void CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap);

    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    /* Module */
    Module *pModule;
    /* ********** */

    /* Type */
    Type *VoidType;
    IntegerType *LongType;
    IntegerType *IntType;
    PointerType *VoidPointerType;
    /* ********** */


    /* Global Variable */
//    GlobalVariable *numCost;
//    AllocaInst *itNum;
    GlobalVariable *Switcher;
    GlobalVariable *GeoRate;
    /* ***** */

    /* Function */
    // int aprof_init()
//    Function *aprof_init;
//    // aprof_dump(void *memory_addr, int length)
//    Function *aprof_dump;
//    // void aprof_return(unsigned long numCost,  unsigned long itNum)
//    Function *aprof_return;

    Function *aprof_geo;
    /* ********** */

    /* Constant */
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantInt0;
    ConstantInt *ConstantInt1;
    ConstantInt *ConstantSamplingRate;
    /* ********** */

};

#endif //NEWCOMAIR_ARRAYLISTSAMPLEINSTRUMENT_H

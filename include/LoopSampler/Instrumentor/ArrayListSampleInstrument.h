//
// CloneSample Pass demo.
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

private:

    void SetupInit(Module &M);

    void SetupTypes();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    void InstrumentInnerLoop(Loop *pInnerLoop, PostDominatorTree *PDT);

    void CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap, std::vector<BasicBlock *> &vecCloned);

    // copy operands and incoming values from old Inst to new Inst
    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    void InstrumentRecordMemHooks(std::vector<BasicBlock *> &vecAdd);

    void InstrumentMain();

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
    GlobalVariable *numGlobalCounter;
    GlobalVariable *GeoRate;
    GlobalVariable *numGlobalCost;
    /* ***** */

    /* Function */
    // int aprof_init()
//    Function *aprof_init;
//    // aprof_dump(void *memory_addr, int length)
//    Function *aprof_dump;
//    // void aprof_return(unsigned long numCost,  unsigned long itNum)
//    Function *aprof_return;

    Function *geo;

    // Init shared memory at the entry of main function.
    Function *InitMemHooks;

    // Finalize shared memory at the return/exit of main function.
    Function *FinalizeMemHooks;

    // Record mem addr and len after each Instruction in the cloned Loop.
    Function *RecordMemHooks;

    // Record Cost at the return of the Function containing the cloned Loop.
    Function *RecordCostHooks;

    /* ********** */

    /* Constant */
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantInt0;
    ConstantInt *ConstantIntN1;
//    ConstantInt *ConstantInt1;
    ConstantInt *ConstantSamplingRate;
    /* ********** */

};

#endif //NEWCOMAIR_ARRAYLISTSAMPLEINSTRUMENT_H

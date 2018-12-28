//
// CloneSample Pass demo.
//

#ifndef NEWCOMAIR_LOOPINSTRUMENT_H
#define NEWCOMAIR_LOOPINSTRUMENT_H

#include "llvm/Pass.h"
//#include "llvm/Transforms/Utils/ValueMapper.h"
//#include "llvm/Analysis/AliasAnalysis.h"
//#include "llvm/Analysis/AliasSetTracker.h"
#include <vector>
#include <map>
#include <set>

using namespace llvm;
using namespace std;

struct LoopInstrumentor : public ModulePass {

    static char ID;

    LoopInstrumentor();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

private:

    void SetupInit(Module &M);

    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    void InstrumentInnerLoop(Loop *pInnerLoop);

    void CreateIfElseBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap, std::vector<BasicBlock *> &vecCloned);

    // copy operands and incoming values from old Inst to new Inst
    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    void InstrumentRecordMemHooks(std::vector<BasicBlock *> &vecCloned, std::vector<Instruction *> &notInstrumented);

    void CloneFunctionCalled(std::set<BasicBlock *> &setBlocksInLoop, ValueToValueMapTy &VCalleeMap, std::map<Function *, std::set<Instruction *> > &FuncCallSiteMapping);

    void InstrumentMain();

    void InlineNumLocalCost(Loop *pLoop, vector<Instruction *>& notInstrumentedOriginal);

    void InlineSetRecord(Value *address, Value *length, Value *flag, Instruction *InsertBefore);
    void InlineHookDelimit(Instruction *InsertBefore);
    void InlineHookStore(Value *addr, Type *type1, Instruction *InsertBefore);
    void InlineHookLoad(Value *addr, Type *type1, Instruction *InsertBefore);
    void InlineOutputCost(Instruction *InsertBefore);

    /* Module */
    Module *pModule;
    std::set<int> setInstID;
    vector<std::pair<Function *, int> > vecParaID;
    /* ********** */

    /* Type */
    Type *VoidType;
    IntegerType *LongType;
    PointerType *LongStarType;
    IntegerType *IntType;
    PointerType *VoidPointerType;
    IntegerType *CharType;
    PointerType *CharStarType;
    IntegerType *BoolType;
    /* ********** */

    /* Struct */
    StructType *struct_stMemRecord;

    /* ********* */

    /* Global Variable */
    GlobalVariable *SAMPLE_RATE;
    GlobalVariable *numGlobalCounter;
    GlobalVariable *numGlobalCost;
    GlobalVariable *Record_CPI;
    GlobalVariable *pcBuffer_CPI;
    GlobalVariable *iBufferIndex_CPI;

    /* ***** */

    /* ***** */
    /* Function */
    // int aprof_init()
//    Function *aprof_init;
//    // aprof_dump(void *memory_addr, int length)
//    Function *aprof_dump;
//    // void aprof_return(unsigned long numCost,  unsigned long itNum)
//    Function *aprof_return;

    // sample_rate_str = getenv("SAMPLE_RATE");
    Function *getenv;
    // sample_rate = atoi(sample_rate_str)
    Function *function_atoi;
    Function *func_llvm_memcpy;

    Function *geo;

    // Init shared memory at the entry of main function.
    Function *InitMemHooks;

    // Finalize shared memory at the return/exit of main function.
    Function *FinalizeMemHooks;

    // Record mem addr and len after each Instruction in the cloned Loop.
    // Function *RecordMemHooks;

    // Record Cost at the return of the Function containing the cloned Loop.
    // Function *RecordCostHooks;

//    Function *HookDelimiter;


//    Function *func_llvm_memcpy;
    /* ********** */

    /* Constant */
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantInt0;  // end
    ConstantInt *ConstantIntN1;
    ConstantInt *ConstantInt1;  // delimit
    ConstantInt *ConstantInt2;  // load
    ConstantInt *ConstantInt3;  // store
    ConstantInt *ConstantInt4;  // memcpy
    ConstantInt *ConstantInt5;  // memmove
    ConstantInt *ConstantLong10;
    ConstantInt *ConstantLong16;
    ConstantInt *ConstantIntFalse;
    ConstantPointerNull *ConstantNULL;
    Constant *SAMPLE_RATE_ptr;

    ConstantAggregateZero *ConstantStMemRecord;
    //ConstantAggregateZero *ConstantArrayRecord;

    Constant *ConstantPtrRecord;
    Constant *ConstantRecordAddress;
    Constant *ConstantRecordLength;
    Constant *ConstantRecordFlag;
    /* ********** */
};

#endif //NEWCOMAIR_LOOPINSTRUMENT_H

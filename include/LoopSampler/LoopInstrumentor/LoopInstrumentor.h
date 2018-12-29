#ifndef NEWCOMAIR_LOOPSAMPLER_LOOPINSTRUMENTOR_LOOPINSTRUMENTOR_H
#define NEWCOMAIR_LOOPSAMPLER_LOOPINSTRUMENTOR_LOOPINSTRUMENTOR_H

#include <vector>
#include <set>

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

using namespace llvm;

struct LoopInstrumentor : public ModulePass {

    static char ID;

    LoopInstrumentor();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

private:

    void SetupInit(Module &M);

    // Setup
    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    // Instrument
    void InstrumentMain();

    void InstrumentInnerLoop(Loop *pInnerLoop, std::map<Instruction *, bool> &mapToInstrument);

    // Helper
    bool SearchToBeInstrumented(Loop *pLoop, AliasAnalysis &AA, DominatorTree &DT,
                                std::map<Instruction *, bool> &mapToInstrument);

    void CloneFunctionCalled(std::set<BasicBlock *> &setBlocksInLoop, ValueToValueMapTy &VCalleeMap,
                             std::map<Function *, std::set<Instruction *> > &FuncCallSiteMapping);

    void CreateIfElseBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap,
                        std::vector<BasicBlock *> &vecCloned);

    // copy operands and incoming values from old Inst to new Inst
    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    // Instrument InlineHookLoad and InlineHookStore
    void InstrumentRecordMemHooks(std::map<Instruction *, bool> &mapToInstrument, Instruction *pFirstOfPreheader);

    // Inline instrument
    void InlineNumLocalCost(Loop *pLoop);

    void InlineSetRecord(Value *address, Value *length, Value *flag, Instruction *InsertBefore);

    void InlineHookDelimit(Instruction *InsertBefore);

    void InlineHookStore(Value *addr, Type *type1, Instruction *InsertBefore);

    void InlineHookLoad(Value *addr, Type *type1, Instruction *InsertBefore);

    void InlineOutputCost(Instruction *InsertBefore);

    // Module
    Module *pModule;
    std::set<int> setInstID;
    // std::vector<std::pair<Function *, int> > vecParaID;

    // Type
    Type *VoidType;
    IntegerType *LongType;
    IntegerType *IntType;
    IntegerType *CharType;
    PointerType *CharStarType;
    StructType *struct_stMemRecord;

    // Global Variable
    GlobalVariable *SAMPLE_RATE;
    GlobalVariable *numGlobalCounter;
    GlobalVariable *numGlobalCost;
    GlobalVariable *Record_CPI;
    GlobalVariable *pcBuffer_CPI;
    GlobalVariable *iBufferIndex_CPI;

    // Function
    // sample_rate_str = getenv("SAMPLE_RATE");
    Function *getenv;
    // sample_rate = atoi(sample_rate_str)
    Function *function_atoi;
    // random_gen
    Function *geo;
    // Init shared memory at the entry of main function.
    Function *InitMemHooks;
    // Finalize shared memory at the return/exit of main function.
    Function *FinalizeMemHooks;

    // Constant
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantInt0;  // end
    ConstantInt *ConstantIntN1;
    ConstantInt *ConstantInt1;  // delimit
    ConstantInt *ConstantInt2;  // load
    ConstantInt *ConstantInt3;  // store
    ConstantInt *ConstantLong16;
    ConstantPointerNull *ConstantNULL;
    Constant *SAMPLE_RATE_ptr;
};

#endif //NEWCOMAIR_LOOPSAMPLER_LOOPINSTRUMENTOR_LOOPINSTRUMENTOR_H

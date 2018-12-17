//
// Clonesample Pass demo.
//

#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "LoopSampler/LoopInstrumentor/LoopInstrumentor.h"
#include "Common/ArrayLinkedIndentifier.h"
#include "Common/Constant.h"
#include "Common/Loop.h"

#include <stdlib.h>

using namespace llvm;

static RegisterPass<LoopInstrumentor> X("loop-instrument",
                                        "instrument a loop accessing an array element in each iteration",
                                        true, false);

static cl::opt<unsigned> uSrcLine("noLine",
                                  cl::desc("Source Code Line Number for the Loop"), cl::Optional,
                                  cl::value_desc("uSrcCodeLine"));

static cl::opt<std::string> strFileName("strFile",
                                        cl::desc("File Name for the Loop"), cl::Optional,
                                        cl::value_desc("strFileName"));

static cl::opt<std::string> strFuncName("strFunc",
                                        cl::desc("Function Name"), cl::Optional,
                                        cl::value_desc("strFuncName"));

static cl::opt<bool> bElseIf("elseIf", cl::desc("use if-elseif-else instead of if-else"), cl::Optional,
                             cl::value_desc("bElseIf"), cl::init(true));

char LoopInstrumentor::ID = 0;

void LoopInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<LoopInfoWrapperPass>();
}

LoopInstrumentor::LoopInstrumentor() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeLoopInfoWrapperPassPass(Registry);
}

void LoopInstrumentor::SetupTypes() {

    this->VoidType = Type::getVoidTy(pModule->getContext());
    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
    this->CharType = IntegerType::get(pModule->getContext(), 8);
    this->BoolType = IntegerType::get(pModule->getContext(), 1);

    this->VoidPointerType = PointerType::get(IntegerType::get(pModule->getContext(), 8), 0);
    this->CharStarType = PointerType::get(this->CharType, 0);
    this->LongStarType = PointerType::get(this->LongType, 0);
}

void LoopInstrumentor::SetupConstants() {

    // long: 0, 1
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong1 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));

    // int: 0, -1, 1, 0
    this->ConstantInt0 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
    this->ConstantIntN1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
    this->ConstantInt1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
    this->ConstantIntFalse = ConstantInt::get(pModule->getContext(), APInt(1, StringRef("0"), 10));

    // char*: NULL
    this->ConstantNULL = ConstantPointerNull::get(this->CharStarType);
}

void LoopInstrumentor::SetupGlobals() {

    // int numGlobalCounter = 0;
    assert(pModule->getGlobalVariable("numGlobalCounter") == NULL);
    this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, 0,
                                                "numGlobalCounter");
    this->numGlobalCounter->setAlignment(4);
    this->numGlobalCounter->setInitializer(this->ConstantInt0);

    // int SAMPLE_RATE = 0;
    assert(pModule->getGlobalVariable("SAMPLE_RATE") == NULL);
    this->SAMPLE_RATE = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::CommonLinkage, 0,
                                           "SAMPLE_RATE");
    this->SAMPLE_RATE->setAlignment(4);
    this->SAMPLE_RATE->setInitializer(this->ConstantInt0);

    // char *pcBuffer_CPI = NULL;
    assert(pModule->getGlobalVariable("pcBuffer_CPI") == NULL);
    this->pcBuffer_CPI = new GlobalVariable(*pModule, this->CharStarType, false, GlobalValue::ExternalLinkage, 0,
                                            "pcBuffer_CPI");
    this->pcBuffer_CPI->setAlignment(8);
    this->pcBuffer_CPI->setInitializer(this->ConstantNULL);

    //"SAMPLE_RATE" string
    ArrayType *ArrayTy12 = ArrayType::get(this->CharType, 12);
    GlobalVariable *pArrayStr = new GlobalVariable(*pModule, ArrayTy12, true, GlobalValue::PrivateLinkage, 0, "");
    pArrayStr->setAlignment(1);
    Constant *ConstArray = ConstantDataArray::getString(pModule->getContext(), "SAMPLE_RATE", true);
    vector<Constant *> vecIndex;
    vecIndex.push_back(this->ConstantInt0);
    vecIndex.push_back(this->ConstantInt0);
    this->SAMPLE_RATE_ptr = ConstantExpr::getGetElementPtr(ArrayTy12, pArrayStr, vecIndex);
    pArrayStr->setInitializer(ConstArray);
}

void LoopInstrumentor::SetupStructs() {
    vector<Type *> struct_fields;

    assert(pModule->getTypeByName("struct.stMemRecord") == NULL);
    this->struct_stMemRecord = StructType::create(pModule->getContext(), "struct.stMemRecord");
    struct_fields.clear();
    struct_fields.push_back(this->LongType);  // address
    struct_fields.push_back(this->LongType);   // flagAndLength
    if (this->struct_stMemRecord->isOpaque()) {
        this->struct_stMemRecord->setBody(struct_fields, false);
    }
}

void LoopInstrumentor::SetupFunctions() {

    std::vector<Type *> ArgTypes;

    // getenv
    this->getenv = pModule->getFunction("getenv");
    if (!this->getenv) {
        ArgTypes.push_back(this->CharStarType);
        FunctionType *getenv_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->getenv = Function::Create(getenv_FuncTy, GlobalValue::ExternalLinkage, "getenv", pModule);
        this->getenv->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    this->function_atoi = pModule->getFunction("atoi");
    if(!this->function_atoi)
    {
        ArgTypes.clear();
        ArgTypes.push_back(this->CharStarType);
        FunctionType * atoi_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->function_atoi = Function::Create(atoi_FuncTy, GlobalValue::ExternalLinkage, "atoi", pModule );
        this->function_atoi->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
//
//    // func_llvm_memcpy
//    this->func_llvm_memcpy = pModule->getFunction("llvm.memcpy.p0i8.p0i8.i64");
//    if (!this->func_llvm_memcpy) {
//        ArgTypes.clear();
//        ArgTypes.push_back(this->CharStarType);
//        ArgTypes.push_back(this->CharStarType);
//        ArgTypes.push_back(this->LongType);
//        ArgTypes.push_back(this->IntType);
//        ArgTypes.push_back(this->BoolType);
//        FunctionType *memcpy_funcTy = FunctionType::get(this->VoidType, ArgTypes, false);
//        this->func_llvm_memcpy = Function::Create(memcpy_funcTy, GlobalValue::ExternalLinkage,
//                                                  "llvm.memcpy.p0i8.p0i8.i64", pModule);
//        this->func_llvm_memcpy->setCallingConv(CallingConv::C);
//    }

    // geo
    this->geo = this->pModule->getFunction("geo");
    if (!this->geo) {
        ArgTypes.push_back(this->IntType);
        FunctionType *Geo_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->geo = Function::Create(Geo_FuncTy, GlobalValue::ExternalLinkage, "geo", this->pModule);
        this->geo->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // InitMemHooks
    this->InitMemHooks = this->pModule->getFunction("InitMemHooks");
    if (!this->InitMemHooks) {
        FunctionType *InitMemHooks_FuncTy = FunctionType::get(this->LongStarType, ArgTypes, false);
        this->InitMemHooks = Function::Create(InitMemHooks_FuncTy, GlobalValue::ExternalLinkage, "InitMemHooks",
                                              this->pModule);
        this->InitMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // FinalizeMemHooks
    this->FinalizeMemHooks = this->pModule->getFunction("FinalizeMemHooks");
    if (!this->FinalizeMemHooks) {
        FunctionType *FinalizeMemHooks_FuncTy = FunctionType::get(this->VoidType, ArgTypes, false);
        this->FinalizeMemHooks = Function::Create(FinalizeMemHooks_FuncTy, GlobalValue::ExternalLinkage,
                                                  "FinalizeMemHooks", this->pModule);
        this->FinalizeMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

//    // RecordMemHooks
//    this->RecordMemHooks = this->pModule->getFunction("RecordMemHooks");
//    if (!this->RecordMemHooks) {
//        ArgTypes.push_back(this->VoidPointerType);
//        ArgTypes.push_back(this->LongType);
//        FunctionType *RecordMemHooks_FuncTy = FunctionType::get(this->VoidType, ArgTypes, false);
//        this->RecordMemHooks = Function::Create(RecordMemHooks_FuncTy, GlobalValue::ExternalLinkage, "RecordMemHooks",
//                                                this->pModule);
//        this->RecordMemHooks->setCallingConv(CallingConv::C);
//        ArgTypes.clear();
//    }
}

void LoopInstrumentor::InstrumentMain() {
    AttributeList emptyList;
    CallInst *pCall;

    Function *pFunctionMain = this->pModule->getFunction("main");

    if (!pFunctionMain) {
        errs() << "Cannot find the main function\n";
        return;
    }

    // Instrument to entry block
    {
        CallInst *pCall;
        StoreInst *pStore;

        BasicBlock &entryBlock = pFunctionMain->getEntryBlock();

        // Instrument before FirstNonPHI of main function.
        Instruction *firstInst = pFunctionMain->getEntryBlock().getFirstNonPHI();

        // Instrument pcBuffer_CPI
//        pStore = new StoreInst(pCall, this)

        // Instrument InitMemHooks
        pCall = CallInst::Create(this->InitMemHooks, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);

        // Instrument getenv
        // TODO: change attributes
        vector<Value *> vecParam;
        vecParam.push_back(SAMPLE_RATE_ptr);
        pCall = CallInst::Create(this->getenv, vecParam, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);

        // Instrument atoi
        // TODO: change attributes
        pCall = CallInst::Create(this->function_atoi, pCall, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);

        // SAMPLE_RATE = atoi(getenv("SAMPLE_RATE"));
        pStore = new StoreInst(pCall, SAMPLE_RATE, false, firstInst);
        pStore->setAlignment(4);
    }

    for (Function::iterator BB = pFunctionMain->begin(); BB != pFunctionMain->end(); BB++) {
        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {

            // Instrument FinalizeMemHooks before return.
            if (ReturnInst *pRet = dyn_cast<ReturnInst>(II)) {
                pCall = CallInst::Create(this->FinalizeMemHooks, "", pRet);
                pCall->setCallingConv(CallingConv::C);
                pCall->setTailCall(false);
                pCall->setAttributes(emptyList);

            } else if (isa<CallInst>(II) || isa<InvokeInst>(II)) {
                CallSite cs(&*II);
                Function *pCalled = cs.getCalledFunction();
                if (!pCalled) {
                    continue;
                }
                // Instrument FinalizeMemHooks before calling exit or functions similar to exit.
                // TODO: any other functions similar to exit?
                if (pCalled->getName() == "exit" || pCalled->getName() == "_ZL9mysql_endi") {
                    pCall = CallInst::Create(this->FinalizeMemHooks, "", &*II);
                    pCall->setCallingConv(CallingConv::C);
                    pCall->setTailCall(false);
                    pCall->setAttributes(emptyList);
                }
            }
        }
    }
}

void LoopInstrumentor::SetupInit(Module &M) {
    // all set up operation
    this->pModule = &M;
    SetupTypes();
    SetupConstants();
    SetupGlobals();
    SetupStructs();
    SetupFunctions();
}


bool LoopInstrumentor::runOnModule(Module &M) {

    SetupInit(M);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction) {
        errs() << "Cannot find the input function\n";
        return false;
    }

    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);

    InstrumentMain();
    InstrumentInnerLoop(pLoop, NULL);

    return false;
}

void LoopInstrumentor::InstrumentInnerLoop(Loop *pInnerLoop, PostDominatorTree *PDT) {

    set<BasicBlock *> setBlocksInLoop;
    for (Loop::block_iterator BB = pInnerLoop->block_begin(); BB != pInnerLoop->block_end(); BB++) {
        setBlocksInLoop.insert(*BB);
    }

    ValueToValueMapTy VCalleeMap;
    map<Function *, set<Instruction *> > FuncCallSiteMapping;

    // add hooks to function called inside the loop
    CloneFunctionCalled(setBlocksInLoop, VCalleeMap, FuncCallSiteMapping);

//    InstrumentCostUpdater(pInnerLoop);

//    vector<LoadInst *> vecLoad;
//    vector<Instruction *> vecIn;
//    vector<Instruction *> vecOut;

    // created auxiliary basic block
    vector<BasicBlock *> vecAdd;
    if (bElseIf == false) {
        CreateIfElseBlock(pInnerLoop, vecAdd);
    } else {
        CreateIfElseIfBlock(pInnerLoop, vecAdd);
    }
    // clone loop
    ValueToValueMapTy VMap;
    vector<BasicBlock *> vecCloned;

    CloneInnerLoop(pInnerLoop, vecAdd, VMap, vecCloned);

    // instrument RecordMemHooks to clone loop
    InstrumentRecordMemHooks(vecCloned);

//    Function *pFunc = pInnerLoop->getHeader()->getParent();


//    for (Function::iterator BI = pFunc->begin(); BI != pFunc->end(); ++BI) {
//
//        BasicBlock *BB = &*BI;
//
//        if (BB->getName().str().find(".CPI") == std::string::npos) {
//            continue;
//        }
//
//        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
//
//            Instruction *Inst = &*II;
//
//            if (hasMarkFlag(Inst)) {
//
//                InstrumentHooks(pFunc, Inst);
//            }
//        }
//    }
//
//    InstrumentReturn(pFunc);
}

void LoopInstrumentor::CreateIfElseBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded) {
    /*
     * If (counter == 0) {              // condition1
     *      counter = gen_random();     // ifBody
     *      // while                    //      cloneBody (to be instrumented)
     * } else {
     *      counter--;                  // elseBody
     *      // while                    //      header (original code, no instrumented)
     * }
     */

    BasicBlock *pCondition1 = NULL;

    BasicBlock *pIfBody = NULL;
    BasicBlock *pElseBody = NULL;
    BasicBlock *pClonedBody = NULL;

    LoadInst *pLoad1 = NULL;
    LoadInst *pLoad2 = NULL;

    ICmpInst *pCmp = NULL;

    BinaryOperator *pBinary = NULL;
    TerminatorInst *pTerminator = NULL;
    BranchInst *pBranch = NULL;
    StoreInst *pStore = NULL;
    CallInst *pCall = NULL;
    AttributeList emptySet;

    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
    Module *pModule = pInnerFunction->getParent();

    pCondition1 = pInnerLoop->getLoopPreheader();
    BasicBlock *pHeader = pInnerLoop->getHeader();

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, 0);
    // Contains original code, thus no CPI
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, 0);
    // Cloned code, added to ifBody and elseIfBody
    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, 0);

    pTerminator = pCondition1->getTerminator();

    /*
    * Append to condition1:
    *  if (counter == 0) {
    *    goto ifBody;
    *  } else {
    *    goto elseBody;
    *  }
    */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt0, "cmp0");
        pBranch = BranchInst::Create(pIfBody, pElseBody, pCmp);
        ReplaceInstWithInst(pTerminator, pBranch);
    }

    /*
     * Append to ifBody:
     *  counter = gen_random();
     *  // instrumentDelimiter
     *  goto clonedBody;
     */
    {
        pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pIfBody);
        pLoad2->setAlignment(4);
        pCall = CallInst::Create(this->geo, pLoad2, "", pIfBody);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptySet);
        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pIfBody);
        pStore->setAlignment(4);


        BranchInst::Create(pClonedBody, pIfBody);
    }

    /*
     * Append to elseBody:
     *  counter--;
     *  goto header;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pElseBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1", pElseBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pElseBody);
        pStore->setAlignment(4);
        BranchInst::Create(pHeader, pElseBody);
    }

    // condition1: num 0
    vecAdded.push_back(pCondition1);
    vecAdded.push_back(pIfBody);
    // cloneBody: num 2
    vecAdded.push_back(pClonedBody);
    vecAdded.push_back(pElseBody);
}

void LoopInstrumentor::CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded) {
    /*
     * if (counter == 0) {              // condition1
     *      counter--;                  // ifBody
     *       // while                   //      cloneBody (to be instrumented)
     * } else if (counter == -1) {      // condition2
     *      counter = gen_random();     // elseIfBody
     *      // while                    //      cloneBody (to be instrumented)
     * } else {
     *      counter--;                  // elseBody
     *      // while                    //      header (original code, no instrumented)
     * }
     */

    BasicBlock *pCondition1 = NULL;
    BasicBlock *pCondition2 = NULL;

    BasicBlock *pIfBody = NULL;
    BasicBlock *pElseIfBody = NULL;
    BasicBlock *pElseBody = NULL;
    BasicBlock *pClonedBody = NULL;

    LoadInst *pLoad1 = NULL;
    LoadInst *pLoad2 = NULL;
    //   LoadInst *pLoad3 = NULL;
    ICmpInst *pCmp = NULL;

    BinaryOperator *pBinary = NULL;
    TerminatorInst *pTerminator = NULL;
    BranchInst *pBranch = NULL;
    StoreInst *pStore = NULL;
    CallInst *pCall = NULL;
    AttributeList emptySet;

    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
    Module *pModule = pInnerFunction->getParent();

    pCondition1 = pInnerLoop->getLoopPreheader();
    BasicBlock *pHeader = pInnerLoop->getHeader();

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, 0);

    pCondition2 = BasicBlock::Create(pModule->getContext(), ".if2.CPI", pInnerFunction, 0);

    pElseIfBody = BasicBlock::Create(pModule->getContext(), ".else.if.body.CPI", pInnerFunction, 0);
    // Contains original code, thus no CPI
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, 0);
    // Cloned code, added to ifBody and elseIfBody
    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, 0);

    pTerminator = pCondition1->getTerminator();

    /*
     * Append to condition1:
     *  if (counter == 0) {
     *    goto ifBody;
     *  } else {
     *    goto condition2;
     *  }
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt0, "cmp0");
        pBranch = BranchInst::Create(pIfBody, pCondition2, pCmp);
        ReplaceInstWithInst(pTerminator, pBranch);
    }

    /*
     * Append to ifBody:
     *  counter--;  // counter-- -> counter += -1
     *  goto clonedBody;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pIfBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_0", pIfBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pIfBody);
        pStore->setAlignment(4);
        BranchInst::Create(pClonedBody, pIfBody);
    }

    /*
     * Append to condition2:
     *  if (counter == -1) {
     *    goto elseIfBody;
     *  } else {
     *    goto elseBody;
     *  }
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pCondition2);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(*pCondition2, ICmpInst::ICMP_EQ, pLoad1, this->ConstantIntN1, "cmpN1");
        BranchInst::Create(pElseIfBody, pElseBody, pCmp, pCondition2);
    }

    /*
     * Append to elseIfBody:
     *  counter = gen_random();
     *  goto clonedBody;
     */
    {
        pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pElseIfBody);
        pLoad2->setAlignment(4);
        pCall = CallInst::Create(this->geo, pLoad2, "", pElseIfBody);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptySet);
        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pElseIfBody);
        pStore->setAlignment(4);
        BranchInst::Create(pClonedBody, pElseIfBody);
    }

    /*
     * Append to elseBody:
     *  counter--;
     *  goto header;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pElseBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_1", pElseBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pElseBody);
        pStore->setAlignment(4);
        BranchInst::Create(pHeader, pElseBody);
    }

    /*
     * Insert at the beginning of clonedBody:
     * cost++;
     */
//    {
//        pLoad3 = new LoadInst(this->numGlobalCost, "", false, pClonedBody);
//        pLoad3->setAlignment(8);
//        pBinary = BinaryOperator::Create(Instruction::Add, pLoad3, this->ConstantLong1, "inc1", pClonedBody);
//        pStore = new StoreInst(pBinary, this->numGlobalCost, false, pClonedBody);
//        pStore->setAlignment(8);
//    }

    // condition1: num 0
    vecAdded.push_back(pCondition1);
    vecAdded.push_back(pIfBody);
    // cloneBody: num 2
    vecAdded.push_back(pClonedBody);
    vecAdded.push_back(pCondition2);
    vecAdded.push_back(pElseIfBody);
    vecAdded.push_back(pElseBody);
}

void LoopInstrumentor::CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap,
                                      std::vector<BasicBlock *> &vecCloned) {
    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pLoop->getExitBlocks(ExitBlocks);

//    set<BasicBlock *> setExitBlocks;
//
//    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
//        setExitBlocks.insert(ExitBlocks[i]);
//    }

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
        VMap[ExitBlocks[i]] = ExitBlocks[i];
    }

    vector<BasicBlock *> ToClone;
    vector<BasicBlock *> BeenCloned;

    set<BasicBlock *> setCloned;

    //clone loop
    ToClone.push_back(pLoop->getHeader());

    while (ToClone.size() > 0) {

        BasicBlock *pCurrent = ToClone.back();
        ToClone.pop_back();

        WeakTrackingVH &BBEntry = VMap[pCurrent];
        if (BBEntry) {
            continue;
        }

        BasicBlock *NewBB;
        BBEntry = NewBB = BasicBlock::Create(pCurrent->getContext(), "", pFunction);

        if (pCurrent->hasName()) {
            NewBB->setName(pCurrent->getName() + ".CPI");
            // add to vecCloned
            vecCloned.push_back(NewBB);
        }

        if (pCurrent->hasAddressTaken()) {
            errs() << "hasAddressTaken branch\n";
            exit(0);
        }

        for (BasicBlock::iterator II = pCurrent->begin(); II != pCurrent->end(); ++II) {
            Instruction *Inst = &*II;
            Instruction *NewInst = Inst->clone();
            if (II->hasName()) {
                NewInst->setName(II->getName() + ".CPI");
            }
            VMap[Inst] = NewInst;
            NewBB->getInstList().push_back(NewInst);
        }

        const TerminatorInst *TI = pCurrent->getTerminator();
        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i) {
            ToClone.push_back(TI->getSuccessor(i));
        }

        setCloned.insert(pCurrent);
        BeenCloned.push_back(NewBB);
    }

    //remap value used inside loop
    vector<BasicBlock *>::iterator itVecBegin = BeenCloned.begin();
    vector<BasicBlock *>::iterator itVecEnd = BeenCloned.end();


    for (; itVecBegin != itVecEnd; itVecBegin++) {
        for (BasicBlock::iterator II = (*itVecBegin)->begin(); II != (*itVecBegin)->end(); II++) {
            this->RemapInstruction(&*II, VMap);
        }
    }

    //add to ifBody and elseIfBody
    BasicBlock *pCondition1 = vecAdd[0];
    BasicBlock *pClonedBody = vecAdd[2];

    BasicBlock *pClonedHeader = cast<BasicBlock>(VMap[pLoop->getHeader()]);

    BranchInst::Create(pClonedHeader, pClonedBody);

    for (BasicBlock::iterator II = pClonedHeader->begin(); II != pClonedHeader->end(); II++) {
        if (PHINode *pPHI = dyn_cast<PHINode>(II)) {
            // vector<int> vecToRemoved;
            for (unsigned i = 0, e = pPHI->getNumIncomingValues(); i != e; ++i) {
                if (pPHI->getIncomingBlock(i) == pCondition1) {
                    pPHI->setIncomingBlock(i, pClonedBody);
                }
            }
        }
    }

    set<BasicBlock *> setProcessedBlock;

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {

        if (setProcessedBlock.find(ExitBlocks[i]) != setProcessedBlock.end()) {
            continue;

        } else {

            setProcessedBlock.insert(ExitBlocks[i]);
        }

        for (BasicBlock::iterator II = ExitBlocks[i]->begin(); II != ExitBlocks[i]->end(); II++) {

            if (PHINode *pPHI = dyn_cast<PHINode>(II)) {

                unsigned numIncomming = pPHI->getNumIncomingValues();

                for (unsigned i = 0; i < numIncomming; i++) {

                    BasicBlock *incommingBlock = pPHI->getIncomingBlock(i);

                    if (VMap.find(incommingBlock) != VMap.end()) {

                        Value *incommingValue = pPHI->getIncomingValue(i);

                        if (VMap.find(incommingValue) != VMap.end()) {
                            incommingValue = VMap[incommingValue];
                        }

                        pPHI->addIncoming(incommingValue, cast<BasicBlock>(VMap[incommingBlock]));

                    }
                }

            }
        }
    }
}

void LoopInstrumentor::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap) {
    for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op) {
        Value *Op = I->getOperand(op);
        ValueToValueMapTy::iterator It = VMap.find(Op);
        if (It != VMap.end()) {
            I->setOperand(op, It->second);
        }
    }

    if (PHINode *PN = dyn_cast<PHINode>(I)) {
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
            ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
            if (It != VMap.end())
                PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
        }
    }
}

void LoopInstrumentor::InstrumentRecordMemHooks(std::vector<BasicBlock *> &vecCloned) {

    for (std::vector<BasicBlock *>::iterator BB = vecCloned.begin(); BB != vecCloned.end(); BB++) {

        BasicBlock *pBB = *BB;

        for (BasicBlock::iterator II = pBB->begin(); II != pBB->end(); II++) {
            Instruction *pInst = &*II;

            if (pInst->getOpcode() == Instruction::Load) {

                errs() << "Instruction:" << *pInst << '\n';

                Value *var = pInst->getOperand(0);

                if (!var) {
                    errs() << "var: NULL" << '\n';
                    continue;
                }

                errs() << "var:" << *var << '\n';

                DataLayout *dl = new DataLayout(this->pModule);

                Type *type_1 = var->getType()->getContainedType(0);

                if (isa<FunctionType>(type_1)) {
                    errs() << "type_1: FunctionType" << '\n';
                    continue;
                }

                errs() << "type_1:" << *type_1 << '\n';

                if (type_1->isSized()) {
                    CastInst *voidPtr = new BitCastInst(var, this->VoidPointerType, "", pInst);
                    ConstantInt *varSize = ConstantInt::get(this->pModule->getContext(),
                                                            APInt(64,
                                                                  StringRef(
                                                                          std::to_string(dl->getTypeAllocSize(type_1))),
                                                                  10));

                    // External RecordMem
//                    std::vector<Value *> params;
//                    params.push_back(voidPtr);
//                    params.push_back(varSize);

//                    CallInst *pCall = CallInst::Create(this->RecordMemHooks, params, "", pInst);
//                    pCall->setCallingConv(CallingConv::C);
//                    pCall->setTailCall(false);
//                    AttributeList emptyList;
//                    pCall->setAttributes(emptyList);

                    // Inline RecordMem
                    //InlineHookMem(, pInst);

                } else {

                    pInst->dump();
                    type_1->dump();
                    assert(false);
                }
            }
        }
    }
}

void LoopInstrumentor::CloneFunctionCalled(set<BasicBlock *> &setBlocksInLoop, ValueToValueMapTy &VCalleeMap,
                                           map<Function *, set<Instruction *> > &FuncCallSiteMapping) {
    vector<Function *> vecWorkList;
    set<Function *> toDo;

    set<Instruction *> setMonitoredInstInCallee;

    set<BasicBlock *>::iterator itBlockSetBegin = setBlocksInLoop.begin();
    set<BasicBlock *>::iterator itBlockSetEnd = setBlocksInLoop.end();

    for (; itBlockSetBegin != itBlockSetEnd; itBlockSetBegin++) {
        BasicBlock *BB = *itBlockSetBegin;

        if (isa<UnreachableInst>(BB->getTerminator())) {
            continue;
        }

        for (BasicBlock::iterator II = (BB)->begin(); II != (BB)->end(); II++) {
            if (isa<DbgInfoIntrinsic>(II)) {
                continue;
            } else if (isa<InvokeInst>(II) || isa<CallInst>(II)) {
                CallSite cs(&*II);
                Function *pCalled = cs.getCalledFunction();

                if (pCalled == NULL) {
                    continue;
                }

                if (pCalled->isDeclaration()) {
                    continue;
                }

                FuncCallSiteMapping[pCalled].insert(&*II);

                if (toDo.find(pCalled) == toDo.end()) {
                    toDo.insert(pCalled);
                    vecWorkList.push_back(pCalled);
                }
            }
        }
    }

    while (vecWorkList.size() > 0) {
        Function *pCurrent = vecWorkList[vecWorkList.size() - 1];
        vecWorkList.pop_back();

        for (Function::iterator BB = pCurrent->begin(); BB != pCurrent->end(); BB++) {
            if (isa<UnreachableInst>(BB->getTerminator())) {
                continue;
            }

            for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
                if (isa<DbgInfoIntrinsic>(II)) {
                    continue;
                } else if (isa<InvokeInst>(II) || isa<CallInst>(II)) {
                    CallSite cs(&*II);
                    Function *pCalled = cs.getCalledFunction();

                    if (pCalled != NULL && !pCalled->isDeclaration()) {
                        FuncCallSiteMapping[pCalled].insert(&*II);

                        if (toDo.find(pCalled) == toDo.end()) {
                            toDo.insert(pCalled);
                            vecWorkList.push_back(pCalled);
                        }
                    }
                }

                MDNode *Node = II->getMetadata("ins_id");

                if (!Node) {
                    continue;
                }

                assert(Node->getNumOperands() == 1);
                if (auto *MDV = dyn_cast<ValueAsMetadata>(Node)) {
                    Value *V = MDV->getValue();
                    ConstantInt *CI = dyn_cast<ConstantInt>(V);
                    assert(CI);
                    if (this->setInstID.find(CI->getZExtValue()) != this->setInstID.end()) {
                        if (isa<LoadInst>(II) || isa<CallInst>(II) || isa<InvokeInst>(II) || isa<MemTransferInst>(II)) {
                            setMonitoredInstInCallee.insert(&*II);
                        } else {
                            assert(0);
                        }
                    }
                }

            }
        }
    }

    set<Function *>::iterator itSetFuncBegin = toDo.begin();
    set<Function *>::iterator itSetFuncEnd = toDo.end();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        Function *rawFunction = *itSetFuncBegin;
        Function *duplicateFunction = CloneFunction(rawFunction, VCalleeMap, NULL);
        duplicateFunction->setName(rawFunction->getName() + ".CPI");
        duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
        rawFunction->getParent()->getFunctionList().push_back(duplicateFunction);

        VCalleeMap[rawFunction] = duplicateFunction;
    }

    itSetFuncBegin = toDo.begin();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        set<Instruction *>::iterator itSetInstBegin = FuncCallSiteMapping[*itSetFuncBegin].begin();
        set<Instruction *>::iterator itSetInstEnd = FuncCallSiteMapping[*itSetFuncBegin].end();

        ValueToValueMapTy::iterator FuncIt = VCalleeMap.find(*itSetFuncBegin);
        assert(FuncIt != VCalleeMap.end());

        Function *clonedFunction = cast<Function>(FuncIt->second);

        for (; itSetInstBegin != itSetInstEnd; itSetInstBegin++) {
            ValueToValueMapTy::iterator It = VCalleeMap.find(*itSetInstBegin);

            if (It != VCalleeMap.end()) {
                if (CallInst *pCall = dyn_cast<CallInst>(It->second)) {
                    pCall->setCalledFunction(clonedFunction);
                } else if (InvokeInst *pInvoke = dyn_cast<InvokeInst>(It->second)) {
                    pInvoke->setCalledFunction(clonedFunction);
                }
            }
        }
    }
}

//// *pcBuffer++ = 0;
//// *pcBuffer++ = 0;
//
//void LoopInstrumentor::InlineHookDelimit(Instruction *II) {
//
//    // %0 = load i64** @pBuffer, align 8, !dbg !15
//    LoadInst *pLoadBufferPtr0 = new LoadInst(this->pcBuffer_CPI, "", false, II);
//    pLoadBufferPtr0->setAlignment(8);
//    // %incdec.ptr = getelementptr inbounds i64* %0, i32 1, !dbg !1
//    GetElementPtrInst *pIncrPtr0 = GetElementPtrInst::Create(this->LongType, pLoadBufferPtr0, this->ConstantInt1, "inc1_pcBuffer_CPI_0", II);
//    // store i64* %incdec.ptr, i64** @pBuffer, align 8, !dbg !15
//    StoreInst *pStorePtr0 = new StoreInst(pIncrPtr0, pLoadBufferPtr0, false, II);
//    pStorePtr0->setAlignment(8);
//    // store i64 0, i64* %0, align 8, !dbg !16
//    StoreInst *pStoreBuffer0 = new StoreInst(this->ConstantLong0, pLoadBufferPtr0, false, II);
//    pStoreBuffer0->setAlignment(8);
//    // %1 = load i64** @pBuffer, align 8, !dbg !17
//    LoadInst *pLoadBufferPtr1 = new LoadInst(this->pcBuffer_CPI, "", false, II);
//    pLoadBufferPtr1->setAlignment(8);
//    // %incdec.ptr1 = getelementptr inbounds i64* %1, i32 1, !dbg !17
//    GetElementPtrInst *pIncrPtr1 = GetElementPtrInst::Create(this->LongType, pLoadBufferPtr1, this->ConstantInt1, "inc1_pcBuffer_CPI_1", II);
//    // store i64* %incdec.ptr1, i64** @pBuffer, align 8, !dbg !17
//    StoreInst *pStorePtr1 = new StoreInst(pIncrPtr1, this->pcBuffer_CPI, false, II);
//    pStorePtr1->setAlignment(8);
//    // store i64 0, i64* %1, align 8, !dbg !18
//    StoreInst *pStoreBuffer1 = new StoreInst(this->ConstantLong0, pLoadBufferPtr1, false, II);
//    pStoreBuffer1->setAlignment(8);
//}


//void LoopInstrumentor::InlineHookMem(MemTransferInst * pMem, Instruction * II)
//{
//    MDNode *Node = pMem->getMetadata("ins_id");
//    assert(Node);
//    assert(Node->getNumOperands() == 1);
//    ConstantInt *CI = dyn_cast<ConstantInt>(Node->getOperand(0));
//
//    LoadInst * pLoadPointer = new LoadInst(this->pcBuffer_CPI, "", false, II);
//    pLoadPointer->setAlignment(8);
//    LoadInst * pLoadIndex   = new LoadInst(this->iBufferIndex_CPI, "", false, II);
//    pLoadIndex->setAlignment(8);
//
//    GetElementPtrInst* getElementPtr = GetElementPtrInst::Create(pLoadPointer, pLoadIndex, "", II);
//    CastInst * pStoreAddress = new BitCastInst(getElementPtr, this->PT_struct_stLogRecord, "", II);
//
//    vector<Value *> vecIndex;
//    vecIndex.push_back(this->ConstantInt0);
//    vecIndex.push_back(this->ConstantInt0);
//    Instruction * const_ptr = GetElementPtrInst::Create(pStoreAddress, vecIndex, "", II);
//    StoreInst * pStore = new StoreInst(this->ConstantInt5, const_ptr, false, II);
//    pStore->setAlignment(4);
//
//    vecIndex.clear();
//    vecIndex.push_back(this->ConstantInt0);
//    vecIndex.push_back(this->ConstantInt1);
//    Instruction * MemRecord_ptr = GetElementPtrInst::Create(pStoreAddress, vecIndex, "", II);
//    PointerType * stMemRecord_PT = PointerType::get( this->struct_stMemRecord, 0);
//    CastInst * pMemRecord = new BitCastInst(MemRecord_ptr, stMemRecord_PT, "", II);
//
//    vecIndex.clear();
//    vecIndex.push_back(this->ConstantInt0);
//    vecIndex.push_back(this->ConstantInt0);
//    const_ptr = GetElementPtrInst::Create(pMemRecord, vecIndex, "", II);
//    pStore = new StoreInst(CI, const_ptr, false, II);
//    pStore->setAlignment(4);
//
//    vecIndex.clear();
//    vecIndex.push_back(this->ConstantInt0);
//    vecIndex.push_back(this->ConstantInt1);
//    const_ptr = GetElementPtrInst::Create(pMemRecord, vecIndex, "", II);
//    Value * pValueLength = pMem->getLength();
//    pStore = new StoreInst(pValueLength, const_ptr, false, II);
//    pStore->setAlignment(8);
//
//    LoadInst * pLoadRecordSize = new LoadInst(this->iRecordSize_CPI, "", false, II);
//    pLoadRecordSize->setAlignment(8);
//
//    BinaryOperator * pAdd = BinaryOperator::Create(Instruction::Add, pLoadIndex, pLoadRecordSize, "", II);
//
//    getElementPtr = GetElementPtrInst::Create(pLoadPointer, pAdd, "", II);
//
//    vector<Value *> vecParam;
//    vecParam.push_back(getElementPtr);
//    vecParam.push_back(pMem->getRawSource());
//    vecParam.push_back(pValueLength);
//    vecParam.push_back(this->ConstantInt1);
//    vecParam.push_back(this->ConstantIntFalse);
//
//    CallInst * pCall = CallInst::Create(this->func_llvm_memcpy, vecParam, "", II);
//    pCall->setCallingConv(CallingConv::C);
//    pCall->setTailCall(false);
//    AttributeSet AS;
//    pCall->setAttributes(AS);
//
//    pAdd = BinaryOperator::Create(Instruction::Add, pAdd, pValueLength, "", II );
//    pStore = new StoreInst(pAdd, this->iBufferIndex_CPI, false, II);
//    pStore->setAlignment(8);
//
//}
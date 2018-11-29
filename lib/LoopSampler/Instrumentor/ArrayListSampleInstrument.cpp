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

#include "LoopSampler/Instrumentor/ArrayListSampleInstrument.h"
#include "Common/ArrayLinkedIndentifier.h"
#include "Common/Constant.h"
#include "Common/Loop.h"

using namespace llvm;

static RegisterPass<ArrayListSampleInstrument> X("array-list-sample-instrument",
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


static cl::opt<int> SamplingRate("sampleRate",
                                 cl::desc("The rate of sampling."),
                                 cl::init(100));

char ArrayListSampleInstrument::ID = 0;

void ArrayListSampleInstrument::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
//    AU.addRequired<PostDominatorTreeWrapperPass>();
//    AU.addRequired<DominatorTreeWrapperPass>();
//    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
}

ArrayListSampleInstrument::ArrayListSampleInstrument() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
//    initializeScalarEvolutionWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
//    initializePostDominatorTreeWrapperPassPass(Registry);
//    initializeDominatorTreeWrapperPassPass(Registry);
}

void ArrayListSampleInstrument::SetupTypes() {

//    this->VoidType = Type::getVoidTy(pModule->getContext());
//    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
//    this->VoidPointerType = PointerType::get(IntegerType::get(pModule->getContext(), 8), 0);

}

void ArrayListSampleInstrument::SetupConstants() {

//    // long
//    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
//    this->ConstantLong1 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));

    // int
    this->ConstantInt0 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
    this->ConstantIntN1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
//    this->ConstantInt1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
    this->ConstantSamplingRate = ConstantInt::get(pModule->getContext(),
                                                  APInt(32, StringRef(std::to_string(SamplingRate)), 10));
}

void ArrayListSampleInstrument::SetupGlobals() {
////    assert(pModule->getGlobalVariable("numCost") == NULL);
//    this->numCost = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, 0, "numCost");
//    this->numCost->setAlignment(8);
//    this->numCost->setInitializer(this->ConstantLong0);

    assert(pModule->getGlobalVariable("numGlobalCounter") == NULL);
    this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType,
                                        false, GlobalValue::ExternalLinkage,
                                        0, "numGlobalCounter");

    this->numGlobalCounter->setAlignment(4);
    this->numGlobalCounter->setInitializer(this->ConstantInt0);

    assert(pModule->getGlobalVariable("GeoRate") == NULL);
    this->GeoRate = new GlobalVariable(*pModule, this->IntType,
                                       false, GlobalValue::ExternalLinkage,
                                       0, "GeoRate");
    this->GeoRate->setAlignment(4);
    this->GeoRate->setInitializer(this->ConstantSamplingRate);

}

void ArrayListSampleInstrument::SetupFunctions() {

    std::vector<Type *> ArgTypes;
//
//    // aprof_init
//    this->aprof_init = this->pModule->getFunction("aprof_init");
////    assert(this->aprof_init != NULL);
//
//    if (!this->aprof_init) {
//        FunctionType *AprofInitType = FunctionType::get(this->VoidType, ArgTypes, false);
//        this->aprof_init = Function::Create(AprofInitType, GlobalValue::ExternalLinkage, "aprof_init", this->pModule);
//        ArgTypes.clear();
//    }
//
//    // aprof_read
//    this->aprof_dump = this->pModule->getFunction("aprof_dump");
////    assert(this->aprof_read != NULL);
//
//    if (!this->aprof_dump) {
//        ArgTypes.push_back(this->VoidPointerType);
//        ArgTypes.push_back(this->IntType);
//        FunctionType *AprofReadType = FunctionType::get(this->VoidType, ArgTypes, false);
//        this->aprof_dump = Function::Create(AprofReadType, GlobalValue::ExternalLinkage, "aprof_dump", this->pModule);
//        this->aprof_dump->setCallingConv(CallingConv::C);
//        ArgTypes.clear();
//    }
//
//    // aprof_return
//    this->aprof_return = this->pModule->getFunction("aprof_return");
////    assert(this->aprof_return != NULL);
//
//    if (!this->aprof_return) {
//        ArgTypes.push_back(this->LongType);
//        FunctionType *AprofReturnType = FunctionType::get(this->VoidType, ArgTypes, false);
//        this->aprof_return = Function::Create(AprofReturnType, GlobalValue::ExternalLinkage, "aprof_return",
//                                              this->pModule);
//        this->aprof_return->setCallingConv(CallingConv::C);
//        ArgTypes.clear();
//    }

    this->aprof_geo = this->pModule->getFunction("aprof_geo");
    if (!this->aprof_geo) {
        // aprof_geo
        ArgTypes.push_back(this->IntType);
        FunctionType *AprofGeoType = FunctionType::get(this->IntType, ArgTypes, false);
        this->aprof_geo = Function::Create
                (AprofGeoType, GlobalValue::ExternalLinkage, "aprof_geo", this->pModule);
        this->aprof_geo->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

}

void ArrayListSampleInstrument::SetupInit(Module &M) {
    // all set up operation
    this->pModule = &M;
    SetupTypes();
    SetupConstants();
    SetupGlobals();
    SetupFunctions();
}


bool ArrayListSampleInstrument::runOnModule(Module &M) {

    SetupInit(M);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);

    if (pFunction == NULL) {
        errs() << "Cannot find the input function\n";
        return false;
    }

//    PostDominatorTree *PDT = &(getAnalysis<PostDominatorTreeWrapperPass>(*pFunction).getPostDomTree());
//    DominatorTree *DT = &(getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree());
    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();

    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);

//    LoopSimplify(pLoop, DT);

//    InstrumentInnerLoop(pLoop, PDT);
    InstrumentInnerLoop(pLoop, NULL);

    return false;
}

void ArrayListSampleInstrument::InstrumentInnerLoop(Loop *pInnerLoop, PostDominatorTree *PDT) {
    set<BasicBlock *> setBlocksInLoop;

    for (Loop::block_iterator BB = pInnerLoop->block_begin(); BB != pInnerLoop->block_end(); BB++) {
        setBlocksInLoop.insert(*BB);
    }

//    InstrumentCostUpdater(pInnerLoop);

//    vector<LoadInst *> vecLoad;
//    vector<Instruction *> vecIn;
//    vector<Instruction *> vecOut;

    //created auxiliary basic block
    vector<BasicBlock *> vecAdd;
    CreateIfElseIfBlock(pInnerLoop, vecAdd);

    //clone loop
    ValueToValueMapTy VMap;
    CloneInnerLoop(pInnerLoop, vecAdd, VMap);

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

void ArrayListSampleInstrument::CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded) {
    /*
    if (counter == 0) {            // condition1
        counter--;                 // ifBody
        while (0) {;}              //   cloneBody (to be instrumented)
    } else if (counter == -1) {    // condition2
        counter = gen_random();    // elseIfBody
        while (0) {;}              //   cloneBody (to be instrumented)
    } else {
        counter--;                 // elseBody
        while (0) {;}              //   header (original code, no instrumented)
    }
     */

    BasicBlock *pCondition1 = NULL;
    BasicBlock *pCondition2 = NULL;

    BasicBlock *pIfBody = NULL;
    BasicBlock *pElseIfBody = NULL;
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
     * Append to ifBody
     *  counter--;  // counter-- -> counter += -1
     *  goto clonedBody;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pIfBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_0", pIfBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pLoad1);
        pStore->setAlignment(4);
        BranchInst::Create(pClonedBody, pIfBody);
    }

    /*
     * Append to condition2
     *  if (counter == -1) {
     *    goto elseIfBody;
     *  } else {
     *    goto elseBody;
     *  }
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pCondition2);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_EQ, pLoad1, this->ConstantIntN1, "cmpN1");
        BranchInst::Create(pIfBody, pElseBody, pCmp, pCondition2);
    }

    /*
     * Append to elseIfBody
     *  counter = gen_random();
     *  goto clonedBody;
     */
    {
        pLoad2 = new LoadInst(this->GeoRate, "", false, 4, pElseIfBody);
        pCall = CallInst::Create(this->aprof_geo, pLoad2, "", pElseIfBody);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptySet);
        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pElseIfBody);
        pStore->setAlignment(4);
        BranchInst::Create(pClonedBody, pElseIfBody);
    }

    /*
     * Append to elseBody
     *  counter--;
     *  goto header;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pElseBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_1", pElseBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pLoad1);
        pStore->setAlignment(4);
        BranchInst::Create(pHeader, pElseBody);
    }

    // condition1: num 0
    vecAdded.push_back(pCondition1);
    vecAdded.push_back(pIfBody);
    // cloneBody: num 2
    vecAdded.push_back(pClonedBody);
    vecAdded.push_back(pCondition2);
    vecAdded.push_back(pElseIfBody);
    vecAdded.push_back(pElseBody);
}

void ArrayListSampleInstrument::CloneInnerLoop(Loop *pLoop, std::vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap) {
    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pLoop->getExitBlocks(ExitBlocks);

    set<BasicBlock *> setExitBlocks;

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
        setExitBlocks.insert(ExitBlocks[i]);
    }

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

void ArrayListSampleInstrument::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap) {
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




#include "LoopSampler/LoopInstrumentor/LoopInstrumentor.h"

#include <fstream>

#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include "Common/ArrayLinkedIndentifier.h"

using namespace llvm;
using std::vector;
using std::map;
using std::set;

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

static cl::opt<bool> bElseIf("bElseIf", cl::desc("use if-elseif-else instead of if-else"), cl::Optional,
                             cl::value_desc("bElseIf"), cl::init(false));

static const char *indvarFilePath = "indvar.info";

char LoopInstrumentor::ID = 0;

LoopInstrumentor::LoopInstrumentor() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeAAResultsWrapperPassPass(Registry);
    initializeDominatorTreeWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
}

void LoopInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
}

bool LoopInstrumentor::runOnModule(Module &M) {

    SetupInit(M);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction) {
        errs() << "Cannot find the input function\n";
        return false;
    }

    VecIndvarNameStrideTy vecIndvarNameStride;
    if (!ReadIndvarStride(indvarFilePath, vecIndvarNameStride)) {
        errs() << "Cannot open indvar file\n";
        return false;
    }

    AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(*pFunction).getAAResults();
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree();
    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();

    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);

    MapLocFlagToInstrument mapToInstrument;
    SearchToBeInstrumented(pLoop, AA, DT, vecIndvarNameStride, mapToInstrument);
    for (auto &kv : mapToInstrument) {
        errs() << *kv.first << " : " << kv.second << '\n';
    }

    InstrumentMain();
    InstrumentInnerLoop(pLoop, DT, mapToInstrument);

    return false;
}

void LoopInstrumentor::SetupInit(Module &M) {
    // all set up operation
    this->pModule = &M;
    SetupTypes();
    SetupStructs();
    SetupConstants();
    SetupGlobals();
    SetupFunctions();
}

void LoopInstrumentor::SetupTypes() {

    this->VoidType = Type::getVoidTy(pModule->getContext());
    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
    this->CharType = IntegerType::get(pModule->getContext(), 8);
    this->CharStarType = PointerType::get(this->CharType, 0);
}

void LoopInstrumentor::SetupStructs() {
    vector<Type *> struct_fields;

    assert(pModule->getTypeByName("struct.stMemRecord") == nullptr);
    this->struct_stMemRecord = StructType::create(pModule->getContext(), "struct.stMemRecord");
    struct_fields.clear();
    struct_fields.push_back(this->LongType);  // address
    struct_fields.push_back(this->IntType);   // length
    // 0: end; 1: delimiter; 2: load; 3: store; 4: loop begin; 5: loop end
    struct_fields.push_back(this->IntType);   // flag
    if (this->struct_stMemRecord->isOpaque()) {
        this->struct_stMemRecord->setBody(struct_fields, false);
    }
}

void LoopInstrumentor::SetupConstants() {

    // long: 0, 16
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong16 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("16"), 10));

    // int: -1, 0, 1, 2, 3, 4, 5, 6
    this->ConstantIntN1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
    this->ConstantInt0 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
    this->ConstantInt1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
    this->ConstantInt2 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("2"), 10));
    this->ConstantInt3 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("3"), 10));
    this->ConstantInt4 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("4"), 10));
    this->ConstantInt5 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("5"), 10));

    // char*: NULL
    this->ConstantNULL = ConstantPointerNull::get(this->CharStarType);
}

void LoopInstrumentor::SetupGlobals() {

    // int numGlobalCounter = 0;
    // TODO: CommonLinkage or ExternalLinkage
    assert(pModule->getGlobalVariable("numGlobalCounter") == nullptr);
    this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, nullptr,
                                                "numGlobalCounter");
    this->numGlobalCounter->setAlignment(4);
    this->numGlobalCounter->setInitializer(this->ConstantInt0);

    // int SAMPLE_RATE = 0;
    assert(pModule->getGlobalVariable("SAMPLE_RATE") == nullptr);
    this->SAMPLE_RATE = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::CommonLinkage, nullptr,
                                           "SAMPLE_RATE");
    this->SAMPLE_RATE->setAlignment(4);
    this->SAMPLE_RATE->setInitializer(this->ConstantInt0);

    // char *pcBuffer_CPI = nullptr;
    assert(pModule->getGlobalVariable("pcBuffer_CPI") == nullptr);
    this->pcBuffer_CPI = new GlobalVariable(*pModule, this->CharStarType, false, GlobalValue::ExternalLinkage, nullptr,
                                            "pcBuffer_CPI");
    this->pcBuffer_CPI->setAlignment(8);
    this->pcBuffer_CPI->setInitializer(this->ConstantNULL);

    // long iBufferIndex_CPI = 0;
    assert(pModule->getGlobalVariable("iBufferIndex_CPI") == nullptr);
    this->iBufferIndex_CPI = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, nullptr,
                                                "iBufferIndex_CPI");
    this->iBufferIndex_CPI->setAlignment(8);
    this->iBufferIndex_CPI->setInitializer(this->ConstantLong0);

    // struct_stLogRecord Record_CPI
    assert(pModule->getGlobalVariable("Record_CPI") == nullptr);
    this->Record_CPI = new GlobalVariable(*pModule, this->struct_stMemRecord, false, GlobalValue::ExternalLinkage,
                                          nullptr,
                                          "Record_CPI");
    this->Record_CPI->setAlignment(16);
    ConstantAggregateZero *const_struct = ConstantAggregateZero::get(this->struct_stMemRecord);
    this->Record_CPI->setInitializer(const_struct);

    // const char *SAMPLE_RATE_ptr = "SAMPLE_RATE";
    ArrayType *ArrayTy12 = ArrayType::get(this->CharType, 12);
    GlobalVariable *pArrayStr = new GlobalVariable(*pModule, ArrayTy12, true, GlobalValue::PrivateLinkage, nullptr, "");
    pArrayStr->setAlignment(1);
    Constant *ConstArray = ConstantDataArray::getString(pModule->getContext(), "SAMPLE_RATE", true);
    vector<Constant *> vecIndex;
    vecIndex.push_back(this->ConstantInt0);
    vecIndex.push_back(this->ConstantInt0);
    this->SAMPLE_RATE_ptr = ConstantExpr::getGetElementPtr(ArrayTy12, pArrayStr, vecIndex);
    pArrayStr->setInitializer(ConstArray);

    // long numGlobalCost = 0;
    assert(pModule->getGlobalVariable("numGlobalCost") == nullptr);
    this->numGlobalCost = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, nullptr,
                                             "numGlobalCost");
    this->numGlobalCost->setAlignment(4);
    this->numGlobalCost->setInitializer(this->ConstantInt0);
}

void LoopInstrumentor::SetupFunctions() {

    vector<Type *> ArgTypes;

    // getenv
    this->getenv = pModule->getFunction("getenv");
    if (!this->getenv) {
        ArgTypes.push_back(this->CharStarType);
        FunctionType *getenv_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->getenv = Function::Create(getenv_FuncTy, GlobalValue::ExternalLinkage, "getenv", pModule);
        this->getenv->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // atoi
    this->function_atoi = pModule->getFunction("atoi");
    if (!this->function_atoi) {
        ArgTypes.clear();
        ArgTypes.push_back(this->CharStarType);
        FunctionType *atoi_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->function_atoi = Function::Create(atoi_FuncTy, GlobalValue::ExternalLinkage, "atoi", pModule);
        this->function_atoi->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

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
        FunctionType *InitHooks_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->InitMemHooks = Function::Create(InitHooks_FuncTy, GlobalValue::ExternalLinkage, "InitMemHooks",
                                              this->pModule);
        this->InitMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // FinalizeMemHooks
    this->FinalizeMemHooks = this->pModule->getFunction("FinalizeMemHooks");
    if (!this->FinalizeMemHooks) {
        ArgTypes.push_back(this->LongType);
        FunctionType *FinalizeMemHooks_FuncTy = FunctionType::get(this->VoidType, ArgTypes, false);
        this->FinalizeMemHooks = Function::Create(FinalizeMemHooks_FuncTy, GlobalValue::ExternalLinkage,
                                                  "FinalizeMemHooks", this->pModule);
        this->FinalizeMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
}

void LoopInstrumentor::InstrumentMain() {
    AttributeList emptyList;

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

        // Instrument InitMemHooks
        pCall = CallInst::Create(this->InitMemHooks, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);

        // Store to pcBuffer_CPI
        pStore = new StoreInst(pCall, this->pcBuffer_CPI, false, firstInst);
        pStore->setAlignment(8);

        // Instrument getenv
        vector<Value *> vecParam;
        vecParam.push_back(SAMPLE_RATE_ptr);
        pCall = CallInst::Create(this->getenv, vecParam, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList func_get_env_PAL;
        {
            SmallVector<AttributeList, 4> Attrs;
            AttributeList PAS;
            {
                AttrBuilder B;
                B.addAttribute(Attribute::NoUnwind);
                PAS = AttributeList::get(pModule->getContext(), ~0U, B);
            }
            Attrs.push_back(PAS);
            func_get_env_PAL = AttributeList::get(pModule->getContext(), Attrs);
        }
        pCall->setAttributes(func_get_env_PAL);

        // Instrument atoi
        pCall = CallInst::Create(this->function_atoi, pCall, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList func_atoi_PAL;
        {
            SmallVector<AttributeList, 4> Attrs;
            AttributeList PAS;
            {
                AttrBuilder B;
                B.addAttribute(Attribute::NoUnwind);
                B.addAttribute(Attribute::ReadOnly);
                PAS = AttributeList::get(pModule->getContext(), ~0U, B);
            }

            Attrs.push_back(PAS);
            func_atoi_PAL = AttributeList::get(pModule->getContext(), Attrs);
        }
        pCall->setAttributes(func_atoi_PAL);

        // SAMPLE_RATE = atoi(getenv("SAMPLE_RATE"));
        pStore = new StoreInst(pCall, SAMPLE_RATE, false, firstInst);
        pStore->setAlignment(4);
    }

    CallInst *pCall;

    for (auto &BB : *pFunctionMain) {
        for (auto &II : BB) {
            Instruction *pInst = &II;
            // Instrument FinalizeMemHooks before return.
            if (auto pRet = dyn_cast<ReturnInst>(pInst)) {
                auto pLoad = new LoadInst(this->iBufferIndex_CPI, "", false, pRet);
                pLoad->setAlignment(8);
                pCall = CallInst::Create(this->FinalizeMemHooks, pLoad, "", pRet);
                pCall->setCallingConv(CallingConv::C);
                pCall->setTailCall(false);
                pCall->setAttributes(emptyList);

                // Output numGlobalCost before return.
                InlineOutputCost(pRet);

            } else if (isa<CallInst>(pInst) || isa<InvokeInst>(pInst)) {
                CallSite cs(pInst);
                Function *pCalled = cs.getCalledFunction();
                if (!pCalled) {
                    continue;
                }
                // Instrument FinalizeMemHooks before calling exit or functions similar to exit.
                // TODO: any other functions similar to exit?
                if (pCalled->getName() == "exit" || pCalled->getName() == "_ZL9mysql_endi") {
                    auto pLoad = new LoadInst(this->iBufferIndex_CPI, "", false, pRet);
                    pLoad->setAlignment(8);
                    pCall = CallInst::Create(this->FinalizeMemHooks, "", pInst);
                    pCall->setCallingConv(CallingConv::C);
                    pCall->setTailCall(false);
                    pCall->setAttributes(emptyList);

                    // Output numGlobalCost before exit.
                    InlineOutputCost(pRet);
                }
            }
        }
    }
}

bool LoopInstrumentor::ReadIndvarStride(const char *filePath, VecIndvarNameStrideTy &vecIndvarNameStride) {

    std::ifstream infile(filePath);

    if (!infile.is_open()) {
        return false;
    }

    std::string indvarName;
    int stride;

    while (infile >> indvarName >> stride) {
        vecIndvarNameStride.push_back(IndvarNameStride{indvarName, stride});
    }

    return true;
}

static bool isMustAlias(Instruction *pInst, set<Instruction *> &setNotMustAlias, AliasAnalysis &AA) {

    bool isMustAlias = false;
    for (const auto &pSetInst : setNotMustAlias) {
        if (AA.isMustAlias(pInst, pSetInst)) {
            errs() << "Must Alias:" << *pInst << "\n\t" << *pSetInst << '\n';
            isMustAlias = true;
            break;
        }
    }
    return isMustAlias;
}

static bool isIndvar(Value *value, const VecIndvarNameStrideTy &vecIndvarNameStride) {

    if (value && !vecIndvarNameStride.empty()) {
        auto name = value->getName();
        if (!name.empty()) {
            for (const auto &indvarNameStride : vecIndvarNameStride) {
                if (indvarNameStride.indvarName == name) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool hasIndvar(Instruction *pInst, const VecIndvarNameStrideTy &vecIndvarNameStride) {

    std::string name;
    if (pInst && !vecIndvarNameStride.empty()) {
        if (isa<LoadInst>(pInst)) {
            if (isIndvar(pInst->getOperand(0), vecIndvarNameStride)) {
                return true;
            }
        } else if (isa<StoreInst>(pInst)) {
            if (isIndvar(pInst->getOperand(1), vecIndvarNameStride)) {
                return true;
            }
        }
    }
    return false;
}

enum NCInstType : unsigned {
    NCOthers,
    NCLoad,
    NCStore,
    NCMemIntrinsic
};

static unsigned parseInst(Instruction *pInst, NCAddrType &addrType) {
    if (isa<LoadInst>(pInst)) {
        if (getNCAddrType(pInst, 0, addrType)) {
            return NCInstType::NCLoad;
        }
    } else if (isa<StoreInst>(pInst)) {
        if (getNCAddrType(pInst, 1, addrType)) {
            return NCInstType::NCStore;
        }
    } else if (isa<MemIntrinsic>(pInst)) {
        return NCInstType::NCMemIntrinsic;
    }
    return NCInstType::NCOthers;
}

bool LoopInstrumentor::SearchToBeInstrumented(Loop *pLoop, AliasAnalysis &AA, DominatorTree &DT,
                                              const VecIndvarNameStrideTy &vecIndvarNameStride,
                                              MapLocFlagToInstrument &mapToInstrument) {

    if (!pLoop) {
        errs() << "pLoop is nullptr\n";
        return false;
    }

    set<Instruction *> setNotMustAlias;
    map<NCAddrType, vector<Instruction *>> mapLoadInst;
    map<NCAddrType, vector<Instruction *>> mapStoreInst;

    // Traverse Loop to record IndVars (Load/Store), record only one for vars that are alias
    // Note that we must handle all indvars before handling other Vars to guarantee indvars in setNotMustAlias
    if (!vecIndvarNameStride.empty()) {
        for (auto BI = pLoop->block_begin(); BI != pLoop->block_end(); BI++) {
            BasicBlock *BB = *BI;
            for (auto &II : *BB) {
                Instruction *pInst = &II;

                NCAddrType addrType;
                if (hasIndvar(pInst, vecIndvarNameStride)) {
                    if (!isMustAlias(pInst, setNotMustAlias, AA)) {
                        switch (parseInst(pInst, addrType)) {
                            case NCInstType::NCLoad: {
                                setNotMustAlias.insert(pInst);
                                auto &vecLoadInst = mapLoadInst[addrType];
                                vecLoadInst.push_back(pInst);
                                break;
                            }
                            case NCInstType::NCStore: {
                                setNotMustAlias.insert(pInst);
                                auto &vecStoreInst = mapStoreInst[addrType];
                                vecStoreInst.push_back(pInst);
                                break;
                            }
                            case NCInstType::NCMemIntrinsic: {
                                errs() << "MemIntrinsic met\n";
                                pInst->dump();
                                break;
                            }
                            default: {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Traverse Loop to record other Vars (Load/Store), record only one for vars that are alias
    for (auto BI = pLoop->block_begin(); BI != pLoop->block_end(); BI++) {
        BasicBlock *BB = *BI;
        for (auto &II : *BB) {
            Instruction *pInst = &II;

            NCAddrType addrType;
            if (!hasIndvar(pInst, vecIndvarNameStride)) {
                if (!isMustAlias(pInst, setNotMustAlias, AA)) {
                    setNotMustAlias.insert(pInst);
                    switch (parseInst(pInst, addrType)) {
                        case NCInstType::NCLoad: {
                            setNotMustAlias.insert(pInst);
                            auto &vecLoadInst = mapLoadInst[addrType];
                            vecLoadInst.push_back(pInst);
                            break;
                        }
                        case NCInstType::NCStore: {
                            setNotMustAlias.insert(pInst);
                            auto &vecStoreInst = mapStoreInst[addrType];
                            vecStoreInst.push_back(pInst);
                            break;
                        }
                        case NCInstType::NCMemIntrinsic: {
                            errs() << "MemIntrinsic met\n";
                            pInst->dump();
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    }

    // Use dominance to decide instrument locations
    for (auto &kv : mapLoadInst) {

        auto addrType = kv.first;
        auto vecLoadInst = kv.second;

        // init to empty
        vector<Instruction *> vecStoreInst;

        if (mapStoreInst.find(addrType) != mapStoreInst.end()) {
            vecStoreInst = mapStoreInst[addrType];
        }

        switch (existsDom(DT, vecLoadInst, vecStoreInst)) {
            case NCDomResult::ExistsLoadDomAllStores: {
                // if loop invar: record one Load, hoist
                if (pLoop->isLoopInvariant(addrType.pAddr)) {
                    mapToInstrument[vecLoadInst[0]] = NCInstrumentLocFlag::Hoist;
                } else if (isIndvar(addrType.pAddr, vecIndvarNameStride)) {
                    // TODO: hoist BB [firstInst, pLoad] to preheader; hoist only relevant insts to preheader in the future
                    mapToInstrument[vecLoadInst[0]] = NCInstrumentLocFlag::HoistSink;
                } else {
                    mapToInstrument[vecLoadInst[0]] = NCInstrumentLocFlag::Inplace;
                }

                break;
            }

            case NCDomResult::Uncertainty: {
                // record all, in-place
                for (auto pLoad : vecLoadInst) {
                    mapToInstrument[pLoad] = NCInstrumentLocFlag::Inplace;
                }
                for (auto pStore : vecStoreInst) {
                    mapToInstrument[pStore] = NCInstrumentLocFlag::Inplace;
                }
                break;
            }

            case NCDomResult::ExistsStoreDomAllLoads: {
                // do not record
                break;
            }

            default: {
                errs() << "Unreachable NCDomResult\n";
                assert(false);
                break;
            }
        }
    }

    return true;
}

void LoopInstrumentor::InstrumentInnerLoop(Loop *pInnerLoop, DominatorTree &DT, MapLocFlagToInstrument &mapToInstrument) {

    set<BasicBlock *> setBlocksInLoop;
    for (auto BB = pInnerLoop->block_begin(); BB != pInnerLoop->block_end(); BB++) {
        setBlocksInLoop.insert(*BB);
    }

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pInnerLoop->getExitBlocks(ExitBlocks);
    set<BasicBlock *> setExitBlocks;
    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
        setExitBlocks.insert(ExitBlocks[i]);
    }

    ValueToValueMapTy VCalleeMap;
    map<Function *, set<Instruction *> > FuncCallSiteMapping;
    // add hooks to function called inside the loop
    CloneFunctionCalled(setBlocksInLoop, VCalleeMap, FuncCallSiteMapping);

    // created auxiliary basic block
    vector<BasicBlock *> vecAdd;
    if (bElseIf == false) {
        CreateIfElseBlock(pInnerLoop, vecAdd);
    } else {
        CreateIfElseIfBlock(pInnerLoop, vecAdd);
    }

    // inline numLocalCost to function
    InlineNumGlobalCost(pInnerLoop);

    // clone loop
    ValueToValueMapTy VMap;
    vector<BasicBlock *> vecCloned;
    CloneInnerLoop(pInnerLoop, vecAdd, VMap, vecCloned);

    map<BasicBlock *, BasicBlock *> mapExit2Inter;
    InsertBBBeforeExit(pInnerLoop, DT, VMap, mapExit2Inter);

    MapLocFlagToInstrument mapToInstrumentCloned;
    for (auto &kv : mapToInstrument) {
        auto pClonedInst = cast<Instruction>(VMap[kv.first]);
        if (pClonedInst) {
            mapToInstrumentCloned[pClonedInst] = kv.second;
        } else {
            errs() << "Not cloned: " << kv.first << '\n';
        }
    }

    BasicBlock *pClonedBody = vecAdd[2];
    Instruction *pTermInst = pClonedBody->getTerminator();

    set<BasicBlock *> setClonedInterBlock;

    for (auto &kv : mapExit2Inter) {
        setClonedInterBlock.insert(kv.second);
    }


    // instrument RecordMemHooks to clone loop
    InstrumentRecordMemHooks(mapToInstrumentCloned, pTermInst, setClonedInterBlock);

    // inline delimit
    Instruction *pFirstInst = pClonedBody->getFirstNonPHI();
    InlineHookDelimit(pFirstInst);
}

void LoopInstrumentor::CreateIfElseBlock(Loop *pInnerLoop, vector<BasicBlock *> &vecAdded) {
    /*
     * If (counter == 0) {              // condition1
     *      counter = gen_random();     // ifBody
     *      // while                    //      cloneBody (to be instrumented)
     * } else {
     *      counter--;                  // elseBody
     *      // while                    //      header (original code, no instrumented)
     * }
     */

    BasicBlock *pCondition1 = nullptr;

    BasicBlock *pIfBody = nullptr;
    BasicBlock *pElseBody = nullptr;
    BasicBlock *pClonedBody = nullptr;

    LoadInst *pLoad1 = nullptr;
    LoadInst *pLoad2 = nullptr;

    ICmpInst *pCmp = nullptr;

    BinaryOperator *pBinary = nullptr;
    TerminatorInst *pTerminator = nullptr;
    BranchInst *pBranch = nullptr;
    StoreInst *pStore = nullptr;
    CallInst *pCall = nullptr;
    AttributeList emptySet;

    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
    Module *pModule = pInnerFunction->getParent();

    pCondition1 = pInnerLoop->getLoopPreheader();
    BasicBlock *pHeader = pInnerLoop->getHeader();

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, nullptr);
    // Contains original code, thus no CPI
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);
    // Cloned code, added to ifBody and elseIfBody
    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, nullptr);

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

void LoopInstrumentor::CreateIfElseIfBlock(Loop *pInnerLoop, vector<BasicBlock *> &vecAdded) {
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

    BasicBlock *pCondition1 = nullptr;
    BasicBlock *pCondition2 = nullptr;

    BasicBlock *pIfBody = nullptr;
    BasicBlock *pElseIfBody = nullptr;
    BasicBlock *pElseBody = nullptr;
    BasicBlock *pClonedBody = nullptr;

    LoadInst *pLoad1 = nullptr;
    LoadInst *pLoad2 = nullptr;
    ICmpInst *pCmp = nullptr;

    BinaryOperator *pBinary = nullptr;
    TerminatorInst *pTerminator = nullptr;
    BranchInst *pBranch = nullptr;
    StoreInst *pStore = nullptr;
    CallInst *pCall = nullptr;
    AttributeList emptySet;

    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
    Module *pModule = pInnerFunction->getParent();

    pCondition1 = pInnerLoop->getLoopPreheader();
    BasicBlock *pHeader = pInnerLoop->getHeader();

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, nullptr);

    pCondition2 = BasicBlock::Create(pModule->getContext(), ".if2.CPI", pInnerFunction, nullptr);

    pElseIfBody = BasicBlock::Create(pModule->getContext(), ".else.if.body.CPI", pInnerFunction, nullptr);
    // Contains original code, thus no CPI
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);
    // Cloned code, added to ifBody and elseIfBody
    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, nullptr);

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

    // condition1: num 0
    vecAdded.push_back(pCondition1);
    vecAdded.push_back(pIfBody);
    // cloneBody: num 2
    vecAdded.push_back(pClonedBody);
    vecAdded.push_back(pCondition2);
    vecAdded.push_back(pElseIfBody);
    vecAdded.push_back(pElseBody);
}

void LoopInstrumentor::InlineNumGlobalCost(Loop *pLoop) {

    BasicBlock *pHeader = pLoop->getHeader();
    Instruction *pHeaderTerm = pHeader->getTerminator();

    auto pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", pHeaderTerm);
    BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantInt1, "numGlobalCost++",
                                                     pHeaderTerm);
    auto pStoreAdd = new StoreInst(pBinary, this->numGlobalCost, false, pHeaderTerm);
    pStoreAdd->setAlignment(4);
}

void LoopInstrumentor::CloneInnerLoop(Loop *pLoop, vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap,
                                      vector<BasicBlock *> &vecCloned) {

    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pLoop->getExitBlocks(ExitBlocks);

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
        VMap[ExitBlocks[i]] = ExitBlocks[i];
    }

    vector<BasicBlock *> ToClone;
    vector<BasicBlock *> BeenCloned;

    set<BasicBlock *> setCloned;

    //clone loop
    ToClone.push_back(pLoop->getHeader());

    while (!ToClone.empty()) {

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

        for (auto &II : *pCurrent) {
            Instruction *Inst = &II;
            Instruction *NewInst = Inst->clone();
            if (Inst->hasName()) {
                NewInst->setName(Inst->getName() + ".CPI");
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
    for (auto &BB : BeenCloned) {
        for (auto &II : *BB) {
            this->RemapInstruction(&II, VMap);
        }
    }

    //add to ifBody and elseIfBody
    BasicBlock *pCondition1 = vecAdd[0];
    BasicBlock *pClonedBody = vecAdd[2];

    auto pClonedHeader = cast<BasicBlock>(VMap[pLoop->getHeader()]);

    BranchInst::Create(pClonedHeader, pClonedBody);

    for (BasicBlock::iterator II = pClonedHeader->begin(); II != pClonedHeader->end(); II++) {
        if (auto pPHI = dyn_cast<PHINode>(II)) {
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

            if (auto pPHI = dyn_cast<PHINode>(II)) {

                unsigned numIncomming = pPHI->getNumIncomingValues();

                for (unsigned j = 0; j < numIncomming; j++) {

                    BasicBlock *incommingBlock = pPHI->getIncomingBlock(j);

                    if (VMap.find(incommingBlock) != VMap.end()) {

                        Value *incommingValue = pPHI->getIncomingValue(j);

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

void LoopInstrumentor::InsertBBBeforeExit(Loop *pLoop, DominatorTree &DT, ValueToValueMapTy &VMap,
                                          map<BasicBlock *, BasicBlock *> &mapExit2Inter) {

    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock*, 4> vecExitBlock;
    pLoop->getExitBlocks(vecExitBlock);

    SmallDenseSet<BasicBlock *> setNonDomExitBlock;
    for (const auto &dominatee : vecExitBlock) {
        bool isDominated = false;
        for (const auto &dominator : vecExitBlock) {
            if (dominatee != dominator && DT.dominates(dominator, dominatee)) {
                isDominated = true;
                break;
            }
        }
        if (!isDominated) {
            setNonDomExitBlock.insert(dominatee);
        }
    }

    SmallVector<Loop::Edge, 4> vecExitEdge;
    pLoop->getExitEdges(vecExitEdge);

    SmallDenseSet<Loop::Edge> setNonDomExitEdge;
    for (auto &exitEdge : vecExitEdge) {
        const BasicBlock *outsideBlock = exitEdge.second;
        if (outsideBlock && std::find(setNonDomExitBlock.begin(), setNonDomExitBlock.end(), outsideBlock) != setNonDomExitBlock.end()) {
            setNonDomExitEdge.insert(exitEdge);
        }
    }

    for (auto &exitEdge : setNonDomExitEdge) {
        const BasicBlock *insideBlock = exitEdge.first;
        const BasicBlock *outsideBlock = exitEdge.second;

        // convert
        BasicBlock *clonedInsideBlock = nullptr;
        BasicBlock *clonedOutsideBlock = nullptr;
        ValueToValueMapTy::iterator It = VMap.find(insideBlock);
        if (It != VMap.end()) {
            clonedInsideBlock = cast<BasicBlock>(It->second);
        }
        It = VMap.find(outsideBlock);
        if (It != VMap.end()) {
            clonedOutsideBlock = cast<BasicBlock>(It->second);
        }

        BasicBlock *interBlock = BasicBlock::Create(clonedInsideBlock->getContext(), ".inter.block.CPI", pFunction);

        TerminatorInst *TI = clonedInsideBlock->getTerminator();

        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i) {
            if (TI->getSuccessor(i) == clonedOutsideBlock) {
                TI->setSuccessor(i, interBlock);
                BranchInst::Create(clonedOutsideBlock, interBlock);
                interBlock->replaceSuccessorsPhiUsesWith(interBlock);

                mapExit2Inter[clonedOutsideBlock] = interBlock;
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

    if (auto PN = dyn_cast<PHINode>(I)) {
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
            ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
            if (It != VMap.end())
                PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
        }
    }
}

void LoopInstrumentor::InstrumentRecordMemHooks(MapLocFlagToInstrument &mapToInstrument,
                                                Instruction *pTermOfPreheader, set<BasicBlock *> setInterBlock) {

    vector<Instruction *> vecIndvar;

    for (auto &kv : mapToInstrument) {
        Instruction *pInst = kv.first;
        NCInstrumentLocFlag flag = kv.second;

        switch (pInst->getOpcode()) {
            case Instruction::Load: {
                NCAddrType addrType;
                if (getNCAddrType(pInst, 0, addrType)) {
                    switch (flag) {
                        case NCInstrumentLocFlag::Inplace: {
                            InlineHookLoad(addrType.pAddr, addrType.pType, kv.first);
                            break;
                        }
                        case NCInstrumentLocFlag::Hoist: {
                            InlineHookLoad(addrType.pAddr, addrType.pType, pTermOfPreheader);
                            break;
                        }
                        case NCInstrumentLocFlag::HoistSink: {
                            vecIndvar.push_back(pInst);
                            break;
                        }
                        default: {
                            errs() << "Unknown NCInstrumentLocFlag: " << kv.first << '\n';
                            break;
                        }
                    }
                } else {
                    errs() << "Load fails to get AddrType: " << kv.first << '\n';
                }
                break;
            }
            case Instruction::Store: {
                NCAddrType addrType;
                if (getNCAddrType(pInst, 1, addrType)) {
                    switch (flag) {
                        case NCInstrumentLocFlag::Inplace: {
                            InlineHookStore(addrType.pAddr, addrType.pType, kv.first);
                            break;
                        }
                        case NCInstrumentLocFlag::Hoist: {
                            InlineHookStore(addrType.pAddr, addrType.pType, pTermOfPreheader);
                            break;
                        }
                        case NCInstrumentLocFlag::HoistSink: {
                            break;
                        }
                        default: {
                            errs() << "Unknown NCInstrumentLocFlag: " << kv.first << '\n';
                            break;
                        }
                    }
                } else {
                    errs() << "Store fails to get AddrType: " << kv.first << '\n';
                }
                break;
            }
            default: {
                errs() << "Instrument not Load or Store: " << kv.first << '\n';
                break;
            }
        }
    }

    // Instrument preheader
    {
        // clone & remap
        ValueToValueMapTy VMap;
        vector<Instruction *> vecClonedInst;
        // BB1:
        //  ...         // clone from [BB1.begin, indvar1)
        //  indvar1     // instrument and clone "Load indvar1"
        //  ...         // clone from (indvar1, indvar2)
        //  indvar2     // instrument and clone "Load indvar2"
        // BB2:
        //  ...         // clone from [BB2.begin, indvar3)
        //  indvar3     // instrument and clone "Load indvar3"

        // Record BB and its current indvar
        map<BasicBlock *, Instruction *> mapBBInst;

        for (auto &pInst : vecIndvar) {
            // Clone pre insts of indvar
            BasicBlock *BB = pInst->getParent();
            // Find clone begin inst
            Instruction *pBeginInst = nullptr;
            if (mapBBInst.find(BB) == mapBBInst.end()) {
                pBeginInst = &*BB->begin();  // phi inst should also be cloned
            } else {
                pBeginInst = mapBBInst[BB]->getNextNode();
            }
            for (auto iter = pBeginInst; iter != pInst; iter = iter->getNextNode()) {
                ClonePreIndvar(iter, pTermOfPreheader, VMap, vecClonedInst);
            }
            // Clone indvar
            ClonePreIndvar(pInst, pTermOfPreheader, VMap, vecClonedInst);
            // Update map
            mapBBInst[BB] = pInst;
        }

        // Remap all cloned insts
        for (auto &inst : vecClonedInst) {
            RemapInstruction(inst, VMap);
        }

        // Instrument cloned Indvar recorder in preheader
        Instruction *pClonedIndvar = vecClonedInst[vecClonedInst.size() - 1];  // the last is the cloned indvar
        switch (pClonedIndvar->getOpcode()) {
            case Instruction::Load: {
                NCAddrType addrType;
                if (getNCAddrType(pClonedIndvar, 0, addrType)) {
                    InlineHookLoopBegin(addrType.pAddr, addrType.pType, pClonedIndvar);
                }
                break;
            }
            case Instruction::Store: {
//                NCAddrType addrType;
//                if (getNCAddrType(pClonedIndvar, 1, addrType)) {
//                    InlineHookStore(addrType.pAddr, addrType.pType, pClonedIndvar);
//                }
                break;
            }
            default: {
                errs() << "Cloned Indvar not Load or Store\n";
                break;
            }
        }
    }

    // Instrument exit blocks
    for (auto &exitBlock : setInterBlock) {
        auto pTermOfExitBlock = exitBlock->getTerminator();

        // clone & remap
        ValueToValueMapTy VMap;
        vector<Instruction *> vecClonedInst;

        for (auto &pInst : vecIndvar) {
            // Clone pre inst of indvar
            BasicBlock *BB = pInst->getParent();
            for (auto II = BB->begin(); &*II != pInst; ++II) {
                ClonePreIndvar(&*II, pTermOfExitBlock, VMap, vecClonedInst);
            }
            // Clone indvar
            ClonePreIndvar(pInst, pTermOfExitBlock, VMap, vecClonedInst);
        }

        // Remap all cloned insts
        for (auto &inst : vecClonedInst) {
            RemapInstruction(inst, VMap);
        }

        // Instrument cloned Indvar recorder in exit blocks
        Instruction *pClonedIndvar = vecClonedInst[vecClonedInst.size() - 1];  // the last is the cloned indvar
        switch (pClonedIndvar->getOpcode()) {
            case Instruction::Load: {
                NCAddrType addrType;
                if (getNCAddrType(pClonedIndvar, 0, addrType)) {
                    InlineHookLoopEnd(addrType.pAddr, addrType.pType, pClonedIndvar);
                }
                break;
            }
            case Instruction::Store: {
//                NCAddrType addrType;
//                if (getNCAddrType(pClonedIndvar, 1, addrType)) {
//                    InlineHookStore(addrType.pAddr, addrType.pType, pClonedIndvar);
//                }
                break;
            }
            default: {
                errs() << "Cloned Indvar not Load or Store\n";
                break;
            }
        }
    }


}

void LoopInstrumentor::CloneFunctionCalled(set<BasicBlock *> &setBlocksInLoop, ValueToValueMapTy &VCalleeMap,
                                           map<Function *, set<Instruction *> > &FuncCallSiteMapping) {
    vector<Function *> vecWorkList;
    set<Function *> toDo;

    set<Instruction *> setMonitoredInstInCallee;

    auto itBlockSetBegin = setBlocksInLoop.begin();
    auto itBlockSetEnd = setBlocksInLoop.end();

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

                if (pCalled == nullptr) {
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

    while (!vecWorkList.empty()) {
        Function *pCurrent = vecWorkList.back();
        vecWorkList.pop_back();

        for (auto &BB : *pCurrent) {
            auto pBB = &BB;
            if (isa<UnreachableInst>(pBB->getTerminator())) {
                continue;
            }

            for (BasicBlock::iterator II = pBB->begin(); II != pBB->end(); II++) {
                if (isa<DbgInfoIntrinsic>(II)) {
                    continue;
                } else if (isa<InvokeInst>(II) || isa<CallInst>(II)) {
                    CallSite cs(&*II);
                    Function *pCalled = cs.getCalledFunction();

                    if (pCalled != nullptr && !pCalled->isDeclaration()) {
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
                    auto CI = dyn_cast<ConstantInt>(V);
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

    auto itSetFuncBegin = toDo.begin();
    auto itSetFuncEnd = toDo.end();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        Function *rawFunction = *itSetFuncBegin;
        Function *duplicateFunction = CloneFunction(rawFunction, VCalleeMap, nullptr);
        duplicateFunction->setName(rawFunction->getName() + ".CPI");
        duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
        rawFunction->getParent()->getFunctionList().push_back(duplicateFunction);

        VCalleeMap[rawFunction] = duplicateFunction;
    }

    itSetFuncBegin = toDo.begin();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        auto itSetInstBegin = FuncCallSiteMapping[*itSetFuncBegin].begin();
        auto itSetInstEnd = FuncCallSiteMapping[*itSetFuncBegin].end();

        ValueToValueMapTy::iterator FuncIt = VCalleeMap.find(*itSetFuncBegin);
        assert(FuncIt != VCalleeMap.end());

        auto clonedFunction = cast<Function>(FuncIt->second);

        for (; itSetInstBegin != itSetInstEnd; itSetInstBegin++) {
            ValueToValueMapTy::iterator It = VCalleeMap.find(*itSetInstBegin);

            if (It != VCalleeMap.end()) {
                if (auto pCall = dyn_cast<CallInst>(It->second)) {
                    pCall->setCalledFunction(clonedFunction);
                } else if (auto pInvoke = dyn_cast<InvokeInst>(It->second)) {
                    pInvoke->setCalledFunction(clonedFunction);
                }
            }
        }
    }

    auto itMonInstBegin = setMonitoredInstInCallee.begin();
    auto itMonInstEnd = setMonitoredInstInCallee.end();

    for (; itMonInstBegin != itMonInstEnd; itMonInstBegin++) {
        ValueToValueMapTy::iterator It = VCalleeMap.find(*itMonInstBegin);
        assert(It != VCalleeMap.end());

        auto pInst = cast<Instruction>(It->second);
        if (isa<LoadInst>(pInst)) {
            if (dyn_cast<LoadInst>(pInst)) {
                unsigned oprandidx = 0;  // first operand
                NCAddrType addrType;
                if (getNCAddrType(pInst, oprandidx, addrType)) {
                    InlineHookLoad(addrType.pAddr, addrType.pType, pInst);
                }
            }

        } else if (isa<StoreInst>(pInst)) {
            if (dyn_cast<StoreInst>(pInst)) {
                unsigned oprandidx = 1;  // second operand
                NCAddrType addrType;
                if (getNCAddrType(pInst, oprandidx, addrType)) {
                    InlineHookStore(addrType.pAddr, addrType.pType, pInst);
                }
            }

        } else if (isa<MemIntrinsic>(pInst)) {
            errs() << "MemInstrinsic met\n";
            pInst->dump();
        }
    }
}

void LoopInstrumentor::InlineHookDelimit(Instruction *InsertBefore) {

    InlineSetRecord(this->ConstantLong0, this->ConstantInt0, this->ConstantInt1, InsertBefore);
}

void LoopInstrumentor::InlineHookLoad(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);
    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt2, InsertBefore);
}

void LoopInstrumentor::InlineHookStore(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt3, InsertBefore);
}

// Cost Record Format: (0, numGlobalCost, 0)
void LoopInstrumentor::InlineOutputCost(Instruction *InsertBefore) {

    auto pLoad = new LoadInst(this->numGlobalCost, "", false, InsertBefore);
    InlineSetRecord(this->ConstantLong0, pLoad, this->ConstantInt0, InsertBefore);
}

// Array or Linked list begin: (addr, length, 4)
void LoopInstrumentor::InlineHookLoopBegin(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);
    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt4, InsertBefore);
}

// Array or Linked list end: (addr, length, 5)
void LoopInstrumentor::InlineHookLoopEnd(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);
    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt5, InsertBefore);
}

void LoopInstrumentor::InlineSetRecord(Value *address, Value *length, Value *flag, Instruction *InsertBefore) {

    LoadInst *pLoadPointer;
    LoadInst *pLoadIndex;
    BinaryOperator *pBinary;
    CastInst *pCastInst;
    PointerType *pPointerType;

    // char *pc = (char *)pcBuffer_CPI[iBufferIndex_CPI];
    pLoadPointer = new LoadInst(this->pcBuffer_CPI, "", false, InsertBefore);
    pLoadPointer->setAlignment(8);
    pLoadIndex = new LoadInst(this->iBufferIndex_CPI, "", false, InsertBefore);
    pLoadIndex->setAlignment(8);
    GetElementPtrInst *getElementPtr = GetElementPtrInst::Create(this->CharType, pLoadPointer, pLoadIndex, "",
                                                                 InsertBefore);
    // struct_stMemRecord *ps = (struct_stMemRecord *)pc;
    pPointerType = PointerType::get(this->struct_stMemRecord, 0);
    pCastInst = new BitCastInst(getElementPtr, pPointerType, "", InsertBefore);

    // ps->address = address;
    vector<Value *> ptr_address_indices;
    ptr_address_indices.push_back(this->ConstantInt0);
    ptr_address_indices.push_back(this->ConstantInt0);
    GetElementPtrInst *pAddress = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                                            ptr_address_indices, "", InsertBefore);
    auto pStoreAddress = new StoreInst(address, pAddress, false, InsertBefore);
    pStoreAddress->setAlignment(8);

    // ps->length = length;
    vector<Value *> ptr_length_indices;
    ptr_length_indices.push_back(this->ConstantInt0);
    ptr_length_indices.push_back(this->ConstantInt1);
    GetElementPtrInst *pLength = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                                           ptr_length_indices, "", InsertBefore);
    auto pStoreLength = new StoreInst(length, pLength, false, InsertBefore);
    pStoreLength->setAlignment(8);

    // ps->flag = flag;
    vector<Value *> ptr_flag_indices;
    ptr_flag_indices.push_back(this->ConstantInt0);
    ptr_flag_indices.push_back(this->ConstantInt2);
    GetElementPtrInst *pFlag = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst, ptr_flag_indices,
                                                         "", InsertBefore);
    auto pStoreFlag = new StoreInst(flag, pFlag, false, InsertBefore);
    pStoreFlag->setAlignment(4);

    // iBufferIndex_CPI += 16
    pBinary = BinaryOperator::Create(Instruction::Add, pLoadIndex, this->ConstantLong16, "iBufferIndex += 16",
                                     InsertBefore);
    auto pStoreIndex = new StoreInst(pBinary, this->iBufferIndex_CPI, false, InsertBefore);
    pStoreIndex->setAlignment(8);
}

void LoopInstrumentor::ClonePreIndvar(Instruction *preIndvar, Instruction *InsertBefore, ValueToValueMapTy &VMap,
                                      vector<Instruction *> &vecClonedInst) {
    Instruction *pClonedInst = preIndvar->clone();
    if (preIndvar->hasName()) {
        pClonedInst->setName(preIndvar->getName() + ".N");
    }
    VMap[preIndvar] = pClonedInst;
    vecClonedInst.push_back(pClonedInst);
    pClonedInst->insertBefore(InsertBefore);
}
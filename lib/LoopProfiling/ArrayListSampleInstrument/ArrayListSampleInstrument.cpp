//
// Empty Pass demo.
//

#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/IR/BasicBlock.h"
//#include "llvm/IR/Constant.h"
//#include "llvm/IR/Constants.h"
//#include "llvm/IR/DebugInfo.h"
//#include "llvm/IR/MDBuilder.h"
//#include "llvm/IR/Module.h"
//#include "llvm/IR/IntrinsicInst.h"
//#include "llvm/Transforms/Utils/BasicBlockUtils.h"
//#include "llvm/Transforms/Utils/ValueMapper.h"
//#include "llvm/Transforms/Utils/Cloning.h"
//#include "llvm/IR/Instructions.h"
//#include "llvm/Support/CommandLine.h"
//#include "llvm/Support/raw_ostream.h"

#include "LoopProfiling/ArrayListSampleInstrument/ArrayListSampleInstrument.h"
//#include "Common/ArrayLinkedIndentifier.h"
//#include "Common/Constant.h"
//#include "Common/Loop.h"

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
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
}

ArrayListSampleInstrument::ArrayListSampleInstrument() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeScalarEvolutionWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
    initializePostDominatorTreeWrapperPassPass(Registry);
    initializeDominatorTreeWrapperPassPass(Registry);
}

bool ArrayListSampleInstrument::runOnModule(Module &M) {
    return false;
}


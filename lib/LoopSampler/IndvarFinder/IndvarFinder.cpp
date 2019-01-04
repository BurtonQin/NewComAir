#include "LoopSampler/IndvarFinder/IndvarFinder.h"

#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Support/FileSystem.h>
#include "Common/ArrayLinkedIndentifier.h"  // TODO: name conflicts with Common/Search.h

using namespace llvm;
using std::map;

static RegisterPass<IndvarFinder> X("indvar-find",
                                    "find indvar and its stride in loop",
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

char IndvarFinder::ID = 0;

IndvarFinder::IndvarFinder() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeScalarEvolutionWrapperPassPass(Registry);
}

void IndvarFinder::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<ScalarEvolutionWrapperPass>();
}

bool IndvarFinder::runOnModule(Module &M) {

    std::error_code ec;
    raw_fd_ostream myfile("indvar.info", ec, sys::fs::OpenFlags::F_RW);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction) {
        errs() << "Cannot find the input function\n";
        return false;
    }

    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
    ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>(*pFunction).getSE();

    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);

    auto mapInstSCEV = searchInstSCEV(pLoop, SE);
    for (auto &kv : mapInstSCEV) {
        auto stride = searchStride(kv.second, SE);
        const Instruction *pInst = kv.first;

        unsigned operandIdx = 0;
        if (pInst->getOpcode() == Instruction::Store) {
            operandIdx = 1;
        } else if (pInst->getOpcode() != Instruction::Load) {
            continue;
        }
        errs() << pInst->getOperand(operandIdx)->getName() << '\n';
        errs() << *stride << '\n';

        myfile << pInst->getOperand(operandIdx)->getName() << '\n';
        myfile << *stride << '\n';
    }

    myfile.close();

    return false;
}

std::map<const llvm::Instruction *, const llvm::SCEV *>
IndvarFinder::searchInstSCEV(llvm::Loop *pLoop, llvm::ScalarEvolution &SE) {

    std::map<const llvm::Instruction *, const llvm::SCEV *> mapInstSCEV;

    for (auto &BB : pLoop->getBlocks()) {
        for (auto &II : *BB) {
            Instruction *pInst = &II;

            Value *operand = nullptr;
            if (pInst->getOpcode() == Instruction::Load) {
                operand = pInst->getOperand(0);
            } else if (pInst->getOpcode() == Instruction::Store) {
                operand = pInst->getOperand(1);
            }

            if (operand) {
                if (SE.isSCEVable(operand->getType())) {
                    const SCEV *scev = SE.getSCEV(operand);
                    mapInstSCEV[pInst] = scev;
                }
            }
        }
    }

    return mapInstSCEV;
}

const llvm::SCEV *IndvarFinder::searchStride(const llvm::SCEV *scev, llvm::ScalarEvolution &SE) {

    struct FindStride {
        bool foundStride = false;
        const SCEV *stride = nullptr;

        explicit FindStride(ScalarEvolution &SE) : _SE(SE) {

        }

        bool follow(const SCEV *scev) {
            auto addrec = dyn_cast<SCEVAddRecExpr>(scev);
            if (addrec) {
                stride = addrec->getStepRecurrence(_SE);
                foundStride = true;
            }
            return true;
        }

        bool isDone() const {
            return foundStride;
        }

    private:
        ScalarEvolution &_SE;
    };

    FindStride f(SE);
    SCEVTraversal<FindStride> st(f);
    st.visitAll(scev);

    if (f.foundStride) {
        return f.stride;
    } else {
        return nullptr;
    }
}
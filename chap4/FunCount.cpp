#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
 
using namespace llvm;
 
namespace {
struct FunctionCount : public FunctionPass {
  static char ID;
 
  FunctionCount() : FunctionPass(ID) {
  }
 
  bool runOnFunction(Function &F) override {
    LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    errs() << "Function: " << F.getName() << "\n";
    for (Loop *L : *LI) {
      countBlocksInloop(L, 0);
    }
    return false;
  }

  void countBlocksInloop(Loop *L, unsigned nest) {
    unsigned numBlocks = 0;
    Loop::block_iterator bb;

    for (bb = L->block_begin(); bb != L->block_end(); ++bb) {
      numBlocks++;
    }
    for (int idx = 0; idx < nest * 2; idx++) {
      errs() << " ";
    }
    errs() << "Loop level " << nest << " has " << numBlocks << " Blocks\n";
    std::vector<Loop *> subLoops = L->getSubLoops();
    Loop::iterator j;
    for (j = subLoops.begin(); j != subLoops.end(); j++) {
      countBlocksInloop(*j, nest + 1);
    }
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
  }
}; 
}  
 
char FunctionCount::ID = 0;
static RegisterPass<FunctionCount> X("fc", "count the functions",
                                     false /* Only looks at CFG */,
                                     false /* Analysis Pass */);


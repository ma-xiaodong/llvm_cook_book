#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
 
using namespace llvm;
 
namespace {
struct FunctionCount : public FunctionPass {
  static char ID;
 
  FunctionCount() : FunctionPass(ID) {}
 
  bool runOnFunction(Function &F) override {
    errs() << "Function: " << F.getName() << "\n";
    return false;
  }
}; 
}  
 
char FunctionCount::ID = 0;
static RegisterPass<FunctionCount> X("fc", "count the functions",
                                     false /* Only looks at CFG */,
                                     false /* Analysis Pass */);


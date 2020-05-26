#define DEBUG_TYPE "opcodeCounter"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

namespace {
struct CountOpcode: public FunctionPass {
  std::map<std::string, int> opcodeCounter;
  static char ID;

  CountOpcode(): FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
    llvm::outs() << "Function: " << F.getName() << "\n";
    return true;
  }
};
}

char CountOpcode::ID = 0;                                                     
static RegisterPass<CountOpcode> X("fc", "count the functions",               
                                   false /* Only looks at CFG */,             
                                   false /* Analysis Pass */);


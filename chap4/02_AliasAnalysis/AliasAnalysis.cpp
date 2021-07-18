#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace {
struct EverythingMustAlias : public ImmutablePass, public AliasAnalysis {
  static char ID;
  EverythingMustAlias() : ImmutablePass(ID) {}
  initializeEverythingMustAliasPass();

};
}


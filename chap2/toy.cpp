#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <llvm-c/Core.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>

using namespace llvm;

// some static variables
static LLVMContext context;
static Module *Module_ob;
static IRBuilder<> Builder(context);
static std::map<std::string, Value*> Named_Values;

enum Token_Type
{
  EOF_TOKEN = 0,
  NUMERIC_TOKEN,
  IDENTIFIER_TOKEN,
  LPARAN_TOKEN,
  RPARAN_TOKEN,
  DEF_TOKEN,
  COMM_TOKEN,
  COMMENT_TOKEN,
};

class BaseAST
{
public:
  BaseAST(){}; 
  virtual ~BaseAST(){};

  virtual Value *code_gen() = 0;
};

class VariableAST: public BaseAST
{
  std::string Var_Name;
public:
  VariableAST(std::string &name): Var_Name(name)
  {
  }
  ~VariableAST()
  {
#ifdef DUMP_AST
    std::cout << "VariableAST: " << Var_Name << std::endl;
#endif
  }

  virtual Value *code_gen();
};

Value *VariableAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "VariableAST CG: " << Var_Name << std::endl;
#endif
  Value *V = Named_Values[Var_Name];
  return V ? V : 0;
}

class NumericAST: public BaseAST
{
  int numeric_val;
public:
  NumericAST(int val): numeric_val(val)
  {
  }
  ~NumericAST()
  {
#ifdef DUMP_AST
    std::cout << "NumericAST: " << numeric_val << std::endl;
#endif
  }

  virtual Value *code_gen();
};

Value *NumericAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "NumericAST CG: " << numeric_val << std::endl;
#endif
  return ConstantInt::get(Type::getInt32Ty(context), numeric_val);
}

class BinaryAST: public BaseAST
{
  std::string Bin_Operator;
  BaseAST *LHS, *RHS;

public:
  BinaryAST(std::string op, BaseAST *lhs, BaseAST *rhs): 
  Bin_Operator(op), LHS(lhs), RHS(rhs)
  {
  }

  ~BinaryAST()
  {
    if(LHS)
      delete LHS;
    if(RHS)
      delete RHS;
#ifdef DUMP_AST
    printf("BinaryAST\n");
#endif
  }
  virtual Value *code_gen();
};

Value *BinaryAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "BinaryAST CG: " << std::endl;
#endif
  Value *L = LHS->code_gen();
  Value *R = RHS->code_gen();

  if(L == 0 || R == 0)
    return 0;

  switch(atoi(Bin_Operator.c_str()))
  {
    case '+':
      return Builder.CreateAdd(L, R, "addtmp");
    case '-':
      return Builder.CreateSub(L, R, "subtmp");
    case '*':
      return Builder.CreateMul(L, R, "multmp");
    case '/':
      return Builder.CreateUDiv(L, R, "divtmp");
    default:
      return 0;
  }
}

class FunctionDeclAST: public BaseAST {
  std::string Func_name;
  std::vector<std::string> Arguments;

public:
  FunctionDeclAST(const std::string &name, 
                  const std::vector<std::string> &args):
                  Func_name(name), Arguments(args) {
  }

  ~FunctionDeclAST()
  {
#ifdef DUMP_AST
    std::cout << "FunctionDeclAST: " << Func_name << std::endl;
#endif
  }
  virtual Value *code_gen();
};

Value *FunctionDeclAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "FunctionDeclAST CG: " << std::endl;
#endif
  std::vector<Type *> Integers(Arguments.size(), Type::getInt32Ty(context));
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(context), 
                                       Integers, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, 
                                 Func_name, Module_ob);

  if(F->getName() != Func_name)
  {
    F->eraseFromParent();
    F = Module_ob->getFunction(Func_name);

    if(F->empty())  return 0;
    if(F->arg_size() != Arguments.size()) return 0;
  }

  unsigned idx = 0;
  for(Function::arg_iterator arg_it = F->arg_begin(); idx != Arguments.size(); 
      ++arg_it, ++idx)
  {
    arg_it->setName(Arguments[idx]);
    Named_Values[Arguments[idx]] = &(*arg_it);
  }
  return F;
}

class FunctionDefnAST: public BaseAST {
  FunctionDeclAST *Func_Decl;
  BaseAST *Body;
  
public:
  FunctionDefnAST(FunctionDeclAST *proto, BaseAST *body): 
  Func_Decl(proto), Body(body)
  {
  }

  ~FunctionDefnAST()
  {
    if(Func_Decl)
      delete Func_Decl;
    if(Body)
      delete Body;
#ifdef DUMP_AST
    printf("FunctionDefnAST\n");
#endif
  }
  virtual Value *code_gen();
};

Value *FunctionDefnAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "FunctionDefnAST CG: " << std::endl;
#endif
  Named_Values.clear();
  Function *theFunction = (Function *)(Func_Decl->code_gen());
  if(theFunction == 0)
    return 0;

  BasicBlock *BB_begin = BasicBlock::Create(context, "entry", theFunction);
  Builder.SetInsertPoint(BB_begin);

  if(Value *retVal = Body->code_gen())
  {
    Builder.CreateRet(retVal);
    verifyFunction(*theFunction);

    return theFunction;
  }

  theFunction->eraseFromParent();
  return 0;
}

class FunctionCallAST: public BaseAST
{
  std::string Function_Callee;
  std::vector<BaseAST *> Function_Arguments;

public:
  FunctionCallAST(const std::string &callee, std::vector<BaseAST *> &args)
      : Function_Callee(callee), Function_Arguments(args) {
  }

  ~FunctionCallAST() {
    size_t len = Function_Arguments.size();
    for(size_t idx = 0; idx < len; idx++) {
      if(Function_Arguments[idx]) {
	delete Function_Arguments[idx];
      }
    }
#ifdef DUMP_AST
    printf("FunctionCallAST\n");
#endif
  }
  virtual Value *code_gen();
};

Value *FunctionCallAST::code_gen()
{
#ifdef DUMP_CG
  std::cout << "FunctionCallAST CG: " << std::endl;
#endif
  Function *callee_f = Module_ob->getFunction(Function_Callee);
  std::vector<Value *> ArgsV;

  for(unsigned i = 0, e = Function_Arguments.size(); i != e; ++i) {
    ArgsV.push_back(Function_Arguments[i]->code_gen());
    if(ArgsV.back() == 0)
      return 0;
  }

  if(callee_f == NULL) {
    std::vector<Type *> Integers(Function_Arguments.size(), 
                                 Type::getInt32Ty(context));
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(context), 
                                         Integers, false);
    Constant *func_tmp = Module_ob->getOrInsertFunction("calltmp", FT);

    return Builder.CreateCall(func_tmp, ArgsV);
  }
  else
    return Builder.CreateCall(callee_f, ArgsV, "calltmp");
}

static int Numeric_Val;
static std::string Identifier_string;
static FILE *file;
static int LastChar = ' ';
static int Current_token;
static std::map<char, int> OperatorPrece;

// declaration of parser functions
static BaseAST *numeric_parser();
static BaseAST *identifier_parser();
static FunctionDeclAST *func_decl_parser();
static FunctionDefnAST *func_defn_parser();
static BaseAST *expression_parser();
static BaseAST *paran_parser();
static BaseAST *Base_Parser();
static BaseAST *binary_op_parser(int Old_prec, BaseAST *LHS);

static void init_precedence();
static int getBinOpPrecedence();
static void Driver();

static int get_token() {
  while(isspace(LastChar))
    LastChar = fgetc(file);

  if(isalpha(LastChar)) {
    Identifier_string = LastChar;

    while(isalnum((LastChar = fgetc(file))))
      Identifier_string += LastChar;

    if(Identifier_string == "def") {
#ifdef DUMP_TOKEN
      printf("DEF_TOKEN\n");
#endif
      return DEF_TOKEN;
    } else {
#ifdef DUMP_TOKEN
      printf("IDENTIFIER_TOKEN\n");
#endif
      return IDENTIFIER_TOKEN;
    }
  }

  if(isdigit(LastChar)) {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = fgetc(file);
    } while(isdigit(LastChar));

    Numeric_Val = strtod(NumStr.c_str(), 0);
#ifdef DUMP_TOKEN
    printf("NUMERIC_TOKEN: %d\n", Numeric_Val);
#endif
    return NUMERIC_TOKEN;
  }

  if(LastChar == '#') {
    do {
      LastChar = fgetc(file);
    } while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');
   
    LastChar = fgetc(file);
    // The next char of EOF is still EOF
#ifdef DUMP_TOKEN
    printf("COMMENT_TOKEN: %c\n", LastChar);
#endif
    return COMM_TOKEN;
  }

  if(LastChar == '(') {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("LPARAN_TOKEN\n");
#endif
    return LPARAN_TOKEN;
  }

  if(LastChar == ')') {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("RPARAN_TOKEN\n");
#endif
    return RPARAN_TOKEN;
  }

  if(LastChar == ',') {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("COMM_TOKEN\n");
#endif
    return COMM_TOKEN;
  }

  if(LastChar == EOF)
    return EOF_TOKEN;

#ifdef DUMP_TOKEN
  printf("Others %c\n", LastChar);
#endif
  int ThisChar = LastChar;
  LastChar = fgetc(file);
  return ThisChar;
}

static int next_token()
{
  Current_token = get_token();
  return Current_token;
}

static BaseAST *numeric_parser()
{
  BaseAST *Result = new NumericAST(Numeric_Val);
  next_token();
  return Result;
}

static BaseAST *identifier_parser()
{
  std::string IdName = Identifier_string;
  next_token();

  if(Current_token != LPARAN_TOKEN)
    return new VariableAST(IdName);

  next_token();
  std::vector<BaseAST *> Args;
  if(Current_token != RPARAN_TOKEN) {
    while(true) {
      BaseAST *Arg = expression_parser();
      if(!Arg) {
	printf("Error from expression_parser!\n");
	return 0;
      }
      Args.push_back(Arg);
      if(Current_token == RPARAN_TOKEN)
	break;
      if(Current_token != COMM_TOKEN) {
	printf("Error in identifier_parser!\n");
	return 0;
      }
      next_token();
    }
  }
  // equal to RPARAN_TOKEN
  next_token();
  return new FunctionCallAST(IdName, Args);
}

static FunctionDeclAST *func_decl_parser() {
  if(Current_token != IDENTIFIER_TOKEN) {
    printf("Error in func_decl_parser: no function identifier!\n");
    return 0;
  }

  std::string FnName = Identifier_string;
  
  next_token();
  if(Current_token != LPARAN_TOKEN) {
    printf("Error in func_decl_parser: no left paran!\n");
    return 0;
  }

  std::vector<std::string> FunctionArgNames;
  while(next_token() == IDENTIFIER_TOKEN)
    FunctionArgNames.push_back(Identifier_string);

  if(Current_token != RPARAN_TOKEN) {
    printf("Error in func_decl_parser: no right paran!\n");
    return 0;
  }
  next_token();
  return new FunctionDeclAST(FnName, FunctionArgNames);
}

static FunctionDefnAST *func_defn_parser() {
  // skip the 'def' token
  next_token();
  FunctionDeclAST *Decl = func_decl_parser();
  if(!Decl) {
    printf("Error in func_defn_parser: from func_decl_parser!\n");
    return 0;
  }

  if(BaseAST *Body = expression_parser())
    return new FunctionDefnAST(Decl, Body);

  printf("Error in func_defn_parser!\n");
  return 0;
}

static BaseAST *expression_parser() {
  BaseAST *LHS = Base_Parser();
  if(!LHS) {
    printf("Error in expression_parser: from Base_Parser!\n");
    return 0;
  }

  if(Current_token == EOF_TOKEN || Current_token == '\r' || 
     Current_token == '\n')
    return LHS;
  else
    return binary_op_parser(0, LHS);
}

static BaseAST *paran_parser() {
  next_token();
  BaseAST *V = expression_parser();
  if(!V) {
    printf("Error in paran_parser: from expression_parser!\n");
    return 0;
  }

  if(Current_token != RPARAN_TOKEN)
    return 0;
  return V;
}

static BaseAST *Base_Parser() {
  switch(Current_token) {
    case IDENTIFIER_TOKEN:
      return identifier_parser();
    case NUMERIC_TOKEN:
      return numeric_parser();
    case LPARAN_TOKEN:
      return paran_parser();
    default:
      return 0;
  }
}

static void init_precedence() {
  OperatorPrece['-'] = 1;
  OperatorPrece['+'] = 2;
  OperatorPrece['/'] = 3;
  OperatorPrece['*'] = 4;
}

static int getBinOpPrecedence() {
  if(Current_token != '+' && Current_token != '-' && 
     Current_token != '*' && Current_token != '/') {
    return -1;
  }

  int TokPrec = OperatorPrece[Current_token];
  if(TokPrec <= 0) {
    printf("Error in getBinOpPrecedence: Token_Type!\n");
    return 0;
  }
  return TokPrec;
}

static BaseAST *binary_op_parser(int old_prec, BaseAST *LHS) {
  while(1) {
    int cur_prec = getBinOpPrecedence();

    if(cur_prec < old_prec)
      return LHS;
    
    int BinOp = Current_token;
    next_token();

    BaseAST *RHS = Base_Parser();
    if(!RHS) {
      printf("Error in binary_op_parser: from Base_Parser!\n");
      return 0;
    }

    int next_prec = getBinOpPrecedence();
    if(cur_prec < next_prec) {
      RHS = binary_op_parser(cur_prec + 1, RHS);
      if(!RHS) {
	printf("Error in binary_op_parser: from binary_op_parser!\n");
	return 0;
      }
    }
    LHS = new BinaryAST(std::to_string(BinOp), LHS, RHS);
  }
}

static void HandleDefn() {
  if(FunctionDefnAST *F = func_defn_parser()) {
    if(Function *LF = (Function *)(F->code_gen())) {
      ;
    }
    delete F;
  }
  else {
    printf("Error in HandleDefn\n!");
  }
  return;
}

static void HandleTopExpression() {
  if(BaseAST *E = expression_parser()) {
    if(E->code_gen()) {
      ;
    }
    delete E;
  }
  else {
    printf("Error in HandleTopExpression\n");
  }
  return;
}

static void Driver() {
  while(true) {
    switch(Current_token) {
      case EOF_TOKEN:
        return;
      case ';':
        next_token();
        break;
      case DEF_TOKEN:
        HandleDefn();
        break;
      default:
        HandleTopExpression();
        break; 
    }
  }
}

int main(int argc, char **argv) {
  init_precedence();
  file = fopen(argv[1], "r");
  if(file == NULL) {
    printf("Error: unable to open %s.\n", argv[1]);
    return 0;
  }

  Module_ob = new Module("my compiler", context);
  next_token();
  Driver();

  Module_ob->print(outs(), nullptr);
  fclose(file);
}


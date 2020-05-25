#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <llvm-c/Core.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
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
static ExecutionEngine *TheEngine;

enum Token_Type {
  EOF_TOKEN = 0,
  NUMERIC_TOKEN,
  IDENTIFIER_TOKEN,
  LPARAN_TOKEN,
  RPARAN_TOKEN,
  DEF_TOKEN,
  COMM_TOKEN,
  COMMENT_TOKEN,
  IF_TOKEN,
  THEN_TOKEN,
  ELSE_TOKEN,
  FOR_TOKEN,
  IN_TOKEN,
  UNARY_TOKEN,
  BINARY_TOKEN
};

void check_cond(bool cond, std::string message) {
  if (!cond) {
    printf("%s", message.c_str());
    exit(0);
  }
  return;
}

class BaseAST
{
public:
  BaseAST(){}; 
  virtual ~BaseAST(){};

  virtual Value *code_gen() = 0;
};

static int Numeric_Val;
static std::string Identifier_string;
static FILE *file;
static int LastChar = ' ';
static int Current_token;
static std::map<char, int> OperatorPrece;
static std::map<int, std::string> dump_str;

// declaration of parser functions
static BaseAST *numeric_parser();
static BaseAST *identifier_parser();
static BaseAST *expression_parser();
static BaseAST *paran_parser();
static BaseAST *Base_Parser();
static BaseAST *binary_op_parser(int Old_prec, BaseAST *LHS);

static void init_precedence();
static int getBinOpPrecedence();
static void Driver();

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

Value *BinaryAST::code_gen() {
#ifdef DUMP_CG
  std::cout << "BinaryAST CG: " << std::endl;
#endif
  Value *L = LHS->code_gen();
  Value *R = RHS->code_gen();

  if(L == 0 || R == 0) {
    printf("Error in codegen of binary ast, no lhs or rhs!\n");
    exit(0);
  }

  switch(atoi(Bin_Operator.c_str())) {
    case '<':
      L = Builder.CreateICmpULT(L, R, "cmptmp");
      return Builder.CreateZExt(L, Type::getInt32Ty(context), "booltmp");
    case '+':
      return Builder.CreateAdd(L, R, "addtmp");
    case '-':
      return Builder.CreateSub(L, R, "subtmp");
    case '*':
      return Builder.CreateMul(L, R, "multmp");
    case '/':
      return Builder.CreateUDiv(L, R, "divtmp");
    default:
      break;
  }

  Function *F = Module_ob->getFunction(std::string("binary") + Bin_Operator);
  Value *Ops[2] = {L, R};
  return Builder.CreateCall(F, Ops, "binop");
}

class FunctionDeclAST: public BaseAST {
  std::string Func_name;
  std::vector<std::string> Arguments;
  bool isOperator;
  unsigned Precedence;

public:
  FunctionDeclAST(const std::string &name, 
                  const std::vector<std::string> &args,
                  bool isoperator = false,
                  unsigned prec = 0)
      : Func_name(name), Arguments(args), 
        isOperator(isoperator), Precedence(prec) {}

  ~FunctionDeclAST() {
#ifdef DUMP_AST
    std::cout << "FunctionDeclAST: " << Func_name << std::endl;
#endif
  }

  bool isUnaryOp() const {
    return isOperator && Arguments.size() == 1;
  }

  bool isBinaryOp() const {
    return isOperator && Arguments.size() == 2;
  }

  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Func_name[Func_name.size() - 1];
  }

  unsigned getBinaryPrecedence() const {
    return Precedence;
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
  if (Func_Decl->isBinaryOp()) {
    OperatorPrece[Func_Decl->getOperatorName()] = 
        Func_Decl->getBinaryPrecedence();
  }

  BasicBlock *BB_begin = BasicBlock::Create(context, "entry", theFunction);
  Builder.SetInsertPoint(BB_begin);

  if(Value *retVal = Body->code_gen()) {
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

Value *FunctionCallAST::code_gen() {
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

class ExprIfAST : public BaseAST {
  BaseAST *Cond, *Then, *Else;

public:
  ExprIfAST(BaseAST *cond, BaseAST *then, BaseAST *else_st)
      : Cond(cond), Then(then), Else(else_st) {}
  virtual Value *code_gen();
};

Value *ExprIfAST::code_gen() {
  Value *cond_tn = Cond->code_gen();
  if (cond_tn == 0)
    return 0;
  cond_tn = Builder.CreateICmpNE(cond_tn, Builder.getInt32(0), "ifcond");

  Function *TheFunc = Builder.GetInsertBlock()->getParent();
  BasicBlock *ThenBB = BasicBlock::Create(context, "then", TheFunc);
  BasicBlock *ElseBB = BasicBlock::Create(context, "else");
  BasicBlock *MergeBB = BasicBlock::Create(context, "ifcont");

  Builder.CreateCondBr(cond_tn, ThenBB, ElseBB);

  Builder.SetInsertPoint(ThenBB);
  Value *ThenVal = Then->code_gen();
  if (ThenVal == 0)
    return 0;
  Builder.CreateBr(MergeBB);
  ThenBB = Builder.GetInsertBlock();  

  TheFunc->getBasicBlockList().push_back(ElseBB);
  Builder.SetInsertPoint(ElseBB);
  Value *ElseVal = Else->code_gen();
  if (ElseVal == 0)
    return 0;
  Builder.CreateBr(MergeBB);
  ElseBB = Builder.GetInsertBlock();

  TheFunc->getBasicBlockList().push_back(MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *Phi = Builder.CreatePHI(Type::getInt32Ty(context), 2, "iftmp");
  Phi->addIncoming(ThenVal, ThenBB);
  Phi->addIncoming(ElseVal, ElseBB);

  return Phi;
}

class ExprForAST : public BaseAST {
  std::string Var_Name;
  BaseAST *Start, *End, *Step, *Body;

public:
  ExprForAST(const std::string &varname, BaseAST *start, BaseAST *end,
             BaseAST *step, BaseAST *body)
      : Var_Name(varname), Start(start), End(end), Step(step), Body(body) {}
  Value *code_gen() override;
};

Value *ExprForAST::code_gen() {
  Value *StartVal = Start->code_gen();
  check_cond(StartVal != 0, "Error, StartVal should not be null!\n");

  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *PreheaderBB = Builder.GetInsertBlock();

  BasicBlock *LoopBB =  BasicBlock::Create(context, "loop", TheFunction);
  Builder.CreateBr(LoopBB);
  Builder.SetInsertPoint(LoopBB);
  PHINode *Variable = Builder.CreatePHI(Type::getInt32Ty(context), 
                                        2, Var_Name.c_str());
  Variable->addIncoming(StartVal, PreheaderBB);
  Value *OldVal = Named_Values[Var_Name];
  Named_Values[Var_Name] = Variable;

  check_cond(Body->code_gen() != 0, "Error in code gen for body in for!\n");

  Value *StepVal;
  if (Step) {
    StepVal = Step->code_gen();
    check_cond(StepVal != 0, "Error when code_gen of StepVal!\n");
  } else {
    StepVal = ConstantInt::get(Type::getInt32Ty(context), 1);
  }

  Value *NextVar = Builder.CreateAdd(Variable, StepVal, "nextvar");

  Value *EndCond = End->code_gen();
  if (EndCond == 0) {
    return EndCond;
  }

  EndCond = Builder.CreateICmpNE(EndCond, 
                                 ConstantInt::get(Type::getInt32Ty(context), 0),
                                 "loopcond");
  BasicBlock *LoopEndBB = Builder.GetInsertBlock();
  BasicBlock *AfterBB = BasicBlock::Create(context, "afterloop", TheFunction);
  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

  Builder.SetInsertPoint(AfterBB);
  Variable->addIncoming(NextVar, LoopEndBB);

  if (OldVal) {
    Named_Values[Var_Name] = OldVal;
  } else {
    Named_Values.erase(Var_Name);
  }

  return Constant::getNullValue(Type::getInt32Ty(context));
}


static int get_token() {
  while(isspace(LastChar))
    LastChar = fgetc(file);

  if(isalpha(LastChar)) {
    Identifier_string = LastChar;

    while(isalnum((LastChar = fgetc(file))))
      Identifier_string += LastChar;

    if(Identifier_string == "def") {
      return DEF_TOKEN;
    } else if(Identifier_string == "if") {
      return IF_TOKEN;
    } else if(Identifier_string == "then") {
      return THEN_TOKEN;
    } else if(Identifier_string == "else") {
      return ELSE_TOKEN;
    } else if(Identifier_string == "for") {
      return FOR_TOKEN;
    } else if(Identifier_string == "in") {
      return IN_TOKEN;
    } else if(Identifier_string == "binary") {
      return BINARY_TOKEN;
    } else {
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
    return NUMERIC_TOKEN;
  }

  if(LastChar == '#') {
    do {
      LastChar = fgetc(file);
    } while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');
   
    LastChar = fgetc(file);
    // The next char of EOF is still EOF
    return COMMENT_TOKEN;
  }

  if(LastChar == '(') {
    LastChar = fgetc(file);
    return LPARAN_TOKEN;
  }

  if(LastChar == ')') {
    LastChar = fgetc(file);
    return RPARAN_TOKEN;
  }

  if(LastChar == ',') {
    LastChar = fgetc(file);
    return COMM_TOKEN;
  }

  if(LastChar == EOF)
    return EOF_TOKEN;

  int ThisChar = LastChar;
  LastChar = fgetc(file);
  return ThisChar;
}

static void dump_token() {
  std::map<int, std::string>::iterator it;

  it = dump_str.find(Current_token);
  if (it != dump_str.end()) {
    printf("%s", it->second.c_str());
  } else {
    printf("Undefined token: %c", Current_token);
  }
  printf(", LastChar: '%c'\n", LastChar);
  return;
}

static int next_token() {
  do {
    Current_token = get_token();
  } while (Current_token == COMMENT_TOKEN);
  // dump_token();
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
      check_cond(Arg != 0, "Error from expression_parser!\n");

      Args.push_back(Arg);
      if(Current_token == RPARAN_TOKEN)
	break;

      check_cond(Current_token == COMM_TOKEN, "Error in identifier_parser!\n");
      next_token();
    }
  }
  // equal to RPARAN_TOKEN
  next_token();
  return new FunctionCallAST(IdName, Args);
}

static FunctionDeclAST *func_decl_parser() {
  std::string FnName;
  unsigned Kind = 0;
  unsigned BinaryPrecedence = 30;

  switch (Current_token) {
    case IDENTIFIER_TOKEN:
      FnName = Identifier_string;
      Kind = 0;
      break;
    case UNARY_TOKEN:
      next_token();
      check_cond(isascii(Current_token) != 0, "Error token followed Unary!\n");

      FnName = "unary";
      FnName += (char)Current_token;
      Kind = 1;

      break;
    case BINARY_TOKEN:
      next_token();
      check_cond(isascii(Current_token) != 0, "Error token followed Unary!\n");

      FnName = "binary";
      FnName += (char)Current_token;
      Kind = 2;
      next_token();

      // if precedence is given
      if (Current_token == NUMERIC_TOKEN) {
        if (Numeric_Val < 1 || Numeric_Val > 100) {
          printf("Error: wrong precedence number!");
          exit(0);
        }
        BinaryPrecedence = (unsigned)Numeric_Val;
      }

      break;
    default:
      printf("Error occured in func_decl_parser!\n");
      exit(0);
  }

  next_token();
  check_cond(Current_token == LPARAN_TOKEN, 
             "Error in func_decl_parser: no left paran!\n");

  std::vector<std::string> FunctionArgNames;
  next_token();
  while(Current_token == IDENTIFIER_TOKEN || Current_token == COMM_TOKEN) {
    if (Current_token == IDENTIFIER_TOKEN) {
      FunctionArgNames.push_back(Identifier_string);
    }
    next_token();
  }

  check_cond(Current_token == RPARAN_TOKEN, 
             "Error in func_decl_parser: no right paran!\n");
  if (Kind && FunctionArgNames.size() != Kind) {
    printf("Error: kind and function arg name size do not match!\n");
    exit(0);
  }

  next_token();
  return new FunctionDeclAST(FnName, FunctionArgNames, 
                             Kind != 0, BinaryPrecedence);
}

static FunctionDefnAST *func_defn_parser() {
  // skip the 'def' token
  next_token();
  FunctionDeclAST *Decl = func_decl_parser();
  check_cond(Decl != 0, "Error in func_defn_parser: from func_decl_parser!\n");

  if(BaseAST *Body = expression_parser())
    return new FunctionDefnAST(Decl, Body);

  printf("Error in func_defn_parser!\n");
  exit(0);
}

static BaseAST *expression_parser() {
  BaseAST *LHS = Base_Parser();
  check_cond(LHS != 0, "Error in expression_parser: from Base_Parser!\n");

  if(Current_token == EOF_TOKEN || Current_token == '\r' || 
     Current_token == '\n')
    return LHS;
  else
    return binary_op_parser(0, LHS);
}

static BaseAST *paran_parser() {
  next_token();
  BaseAST *V = expression_parser();
  check_cond(V != 0, "Error in paran_parser: from expression_parser!\n");

  if(Current_token != RPARAN_TOKEN)
    return 0;
  return V;
}

static BaseAST *if_parser() {
  next_token();

  BaseAST *cond = expression_parser();
  check_cond(cond != 0, "Error in if_parser : empty cond!\n");

  check_cond(Current_token == THEN_TOKEN, 
             "Error in if_parser: THEN_TOKEN is not followed!\n");

  next_token();
  BaseAST *Then = expression_parser();
  check_cond(Then != 0, "Error in if_parser : empty Then!\n");
  check_cond(Current_token == ELSE_TOKEN, 
             "Error in if_parser: ELSE_TOKEN is not followed!\n");

  next_token();
  BaseAST *Else = expression_parser();
  check_cond(Else != 0, "Error in if_parser : empty Else!\n");

  return new ExprIfAST(cond, Then, Else);
}

static BaseAST *for_parser() {
  next_token();

  check_cond(Current_token == IDENTIFIER_TOKEN, 
             "Error in for_parser, IDENTIFIER_TOKEN expected!\n");
  std::string IdName = Identifier_string;

  next_token();
  check_cond(Current_token == '=', "Error in for_parser, '=' expected!\n");

  next_token();
  BaseAST *Start = expression_parser();
  check_cond(Start != 0, 
             "Error in for_parser (Start), from expression_parser!\n");

  check_cond(Current_token == COMM_TOKEN, 
             "Error in for_parser, COMM_TOKEN expected!\n");

  next_token();
  BaseAST *End = expression_parser();
  check_cond(End != 0, "Error in for_parser (End), from expression_parser!\n");
  check_cond(Current_token == COMM_TOKEN, 
             "Error in for_parser, COMM_TOKEN expected!\n");

  next_token();
  BaseAST *Step = expression_parser();
  check_cond(Step != 0, 
             "Error in for_parser (Step), from expression_parser!\n");

  check_cond(Current_token == IN_TOKEN, 
             "Error in for_parser, IN_TOKEN expected!\n");

  next_token();
  BaseAST *Body = expression_parser();
  check_cond(Body != 0, 
             "Error in for_parser (Body), from expression_parser!\n");

  return new ExprForAST (IdName, Start, End, Step, Body);
}

static BaseAST *Base_Parser() {
  switch(Current_token) {
    case IDENTIFIER_TOKEN:
      return identifier_parser();
    case NUMERIC_TOKEN:
      return numeric_parser();
    case LPARAN_TOKEN:
      return paran_parser();
    case IF_TOKEN:
      return if_parser();
    case FOR_TOKEN:
      return for_parser(); 
    default:
      return 0;
  }
}

static void init_precedence() {
  OperatorPrece['<'] = 1;
  OperatorPrece['-'] = 2;
  OperatorPrece['+'] = 2;
  OperatorPrece['/'] = 3;
  OperatorPrece['*'] = 3;
}

static int getBinOpPrecedence() {
  if(Current_token != '+' && Current_token != '-' && 
     Current_token != '*' && Current_token != '/' &&
     Current_token != '<') {
    return -1;
  }

  int TokPrec = OperatorPrece[Current_token];
  check_cond(TokPrec > 0, "Error in getBinOpPrecedence: Token_Type!\n");

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
    check_cond(RHS != 0, "Error in binary_op_parser: from Base_Parser!\n");

    int next_prec = getBinOpPrecedence();
    if(cur_prec < next_prec) {
      RHS = binary_op_parser(cur_prec + 1, RHS);
      check_cond(RHS != 0, 
                 "Error in binary_op_parser: from binary_op_parser!\n");
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
    printf("Error in HandleDefn!\n");
    exit(0);
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
    exit(0);
  }
  return;
}

static void Driver() {
  while(true) {
    switch(Current_token) {
      case EOF_TOKEN:
        return;
      case DEF_TOKEN:
        HandleDefn();
        break;
      default:
        HandleTopExpression();
        break; 
    }
  }
}

void assign_dump_str() {
  // dump information
  dump_str[EOF_TOKEN] = "EOF_TOKEN"; 
  dump_str[NUMERIC_TOKEN] = "NUMERIC_TOKEN"; 
  dump_str[IDENTIFIER_TOKEN] = "IDENTIFIER_TOKEN"; 
  dump_str[LPARAN_TOKEN] = "LPARAN_TOKEN";
  dump_str[RPARAN_TOKEN] = "RPARAN_TOKEN"; 
  dump_str[DEF_TOKEN] = "DEF_TOKEN";
  dump_str[COMM_TOKEN] = "COMM_TOKEN"; 
  dump_str[COMMENT_TOKEN] = "COMMENT_TOKEN";
  dump_str[IF_TOKEN] = "IF_TOKEN"; 
  dump_str[THEN_TOKEN] = "THEN_TOKEN";
  dump_str[ELSE_TOKEN] = "ELSE_TOKEN"; 
  dump_str[FOR_TOKEN] = "FOR_TOKEN";
  dump_str[IN_TOKEN] = "IN_TOKEN";
  dump_str[BINARY_TOKEN] = "BINARY_TOKEN"; 

  return;
}

int main(int argc, char **argv) {
  init_precedence();
  assign_dump_str();

  TheEngine = EngineBuilder(Module_ob).create();
  file = fopen(argv[1], "r");
  if(file == NULL) {
    printf("Error: unable to open %s.\n", argv[1]);
    exit(0);
  }

  Module_ob = new Module("my compiler", context);
  next_token();
  Driver();

  printf("================================\n");
  Module_ob->print(outs(), nullptr);
  fclose(file);
}


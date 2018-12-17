#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>

#include <llvm-c/Core.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

using namespace llvm;

enum Token_Type
{
  EOF_TOKEN = 0,
  NUMERIC_TOKEN,
  IDENTIFIER_TOKEN,
  LPARAN_TOKEN,
  RPARAN_TOKEN,
  DEF_TOKEN,
  COMM_TOKEN,
  SEMI_TOKEN
};

class BaseAST
{
public:
  BaseAST(){}; 
  virtual ~BaseAST(){};
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
    printf("VariableAST\n");
#endif
  }
};

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
    printf("NumericAST\n");
#endif
  }
};

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
};

class FunctionDeclAST
{
  std::string Func_name;
  std::vector<std::string> Arguments;

public:
  FunctionDeclAST(const std::string &name, const std::vector<std::string> &args):
  Func_name(name), Arguments(args)
  {
  }

  ~FunctionDeclAST()
  {
#ifdef DUMP_AST
    printf("FunctionDeclAST\n");
#endif
  }
};

class FunctionDefnAST
{
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
};

class FunctionCallAST: public BaseAST
{
  std::string Function_Callee;
  std::vector<BaseAST *> Function_Arguments;

public:
  FunctionCallAST(const std::string &callee, std::vector<BaseAST *> &args):
  Function_Callee(callee), Function_Arguments(args)
  {
  }

  ~FunctionCallAST()
  {
    size_t len = Function_Arguments.size();
    for(size_t idx = 0; idx < len; idx++)
    {
      if(Function_Arguments[idx])
	delete Function_Arguments[idx];
    }
#ifdef DUMP_AST
    printf("FunctionCallAST\n");
#endif
  }
};

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

static int get_token()
{
  while(isspace(LastChar))
    LastChar = fgetc(file);

  if(isalpha(LastChar)){
    Identifier_string = LastChar;

    while(isalnum((LastChar = fgetc(file))))
      Identifier_string += LastChar;

    if(Identifier_string == "def")
    {
#ifdef DUMP_TOKEN
      printf("DEF_TOKEN\n");
#endif
      return DEF_TOKEN;
    }
#ifdef DUMP_TOKEN
    printf("IDENTIFIER_TOKEN\n");
#endif
    return IDENTIFIER_TOKEN;
  }

  if(isdigit(LastChar))
  {
    std::string NumStr;
    do
    {
      NumStr += LastChar;
      LastChar = fgetc(file);
    }while(isdigit(LastChar));

    // Error handling
    if(LastChar != ' ' && LastChar != EOF && LastChar != '\n' && LastChar != '\r')
    {
      printf("Sytax err in number expression!\n");
      exit(0);
    }

    Numeric_Val = strtod(NumStr.c_str(), 0);
#ifdef DUMP_TOKEN
    printf("NUMERIC_TOKEN: %d\n", Numeric_Val);
#endif
    return NUMERIC_TOKEN;
  }

  if(LastChar == '#')
  {
    do
    {
      LastChar = fgetc(file);
    }while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');
   
    LastChar = fgetc(file); 
    if(LastChar != EOF)
      return get_token();
  }

  if(LastChar == '(')
  {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("LPARAN_TOKEN\n");
#endif
    return LPARAN_TOKEN;
  }

  if(LastChar == ')')
  {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("RPARAN_TOKEN\n");
#endif
    return RPARAN_TOKEN;
  }

  if(LastChar == ',')
  {
    LastChar = fgetc(file);
#ifdef DUMP_TOKEN
    printf("COMM_TOKEN\n");
#endif
    return COMM_TOKEN;
  }

  if(LastChar == EOF)
    return EOF_TOKEN;

#ifdef DUMP_TOKEN
  printf("Others: %c\n", LastChar);
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
  if(Current_token != RPARAN_TOKEN)
  {
    while(true)
    {
      BaseAST *Arg = expression_parser();
      if(!Arg)
      {
	printf("Error from expression_parser!\n");
	return 0;
      }
      Args.push_back(Arg);
      if(Current_token == RPARAN_TOKEN)
	break;
      if(Current_token != COMM_TOKEN)
      {
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

static FunctionDeclAST *func_decl_parser()
{
  if(Current_token != IDENTIFIER_TOKEN)
  {
    printf("Error in func_decl_parser: no function identifier!\n");
    return 0;
  }

  std::string FnName = Identifier_string;
  
  next_token();
  if(Current_token != LPARAN_TOKEN)
  {
    printf("Error in func_decl_parser: no left paran!\n");
    return 0;
  }

  std::vector<std::string> FunctionArgNames;
  while(next_token() == IDENTIFIER_TOKEN)
    FunctionArgNames.push_back(Identifier_string);

  if(Current_token != RPARAN_TOKEN)
  {
    printf("Error in func_decl_parser: no right paran!\n");
    return 0;
  }
  next_token();
  return new FunctionDeclAST(FnName, FunctionArgNames);
}

static FunctionDefnAST *func_defn_parser()
{
  // skip the 'def' token
  next_token();
  FunctionDeclAST *Decl = func_decl_parser();
  if(!Decl)
  {
    printf("Error in func_defn_parser: from func_decl_parser!\n");
    return 0;
  }

  if(BaseAST *Body = expression_parser())
    return new FunctionDefnAST(Decl, Body);

  printf("Error in func_defn_parser!\n");
  return 0;
}

static BaseAST *expression_parser()
{
  BaseAST *LHS = Base_Parser();
  if(!LHS)
  {
    printf("Error in expression_parser: from Base_Parser!\n");
    return 0;
  }

  if(Current_token == EOF_TOKEN || Current_token == '\r' || Current_token == '\n')
    return LHS;
  else
    return binary_op_parser(0, LHS);
}

static BaseAST *paran_parser()
{
  next_token();
  BaseAST *V = expression_parser();
  if(!V)
  {
    printf("Error in paran_parser: from expression_parser!\n");
    return 0;
  }

  if(Current_token != RPARAN_TOKEN)
    return 0;
  return V;
}

static BaseAST *Base_Parser()
{
  switch(Current_token)
  {
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

static void init_precedence()
{
  OperatorPrece['-'] = 1;
  OperatorPrece['+'] = 2;
  OperatorPrece['/'] = 3;
  OperatorPrece['*'] = 4;
}

static int getBinOpPrecedence()
{
  if(Current_token != '+' && Current_token != '-' && Current_token != '*' && Current_token != '/')
  {
    return -1;
  }

  int TokPrec = OperatorPrece[Current_token];
  if(TokPrec <= 0)
  {
    printf("Error in getBinOpPrecedence: Token_Type!\n");
    return 0;
  }
  return TokPrec;
}

static BaseAST *binary_op_parser(int old_prec, BaseAST *LHS)
{
  while(1)
  {
    int cur_prec = getBinOpPrecedence();

    if(cur_prec < old_prec)
      return LHS;
    
    int BinOp = Current_token;
    next_token();

    BaseAST *RHS = Base_Parser();
    if(!RHS)
    {
      printf("Error in binary_op_parser: from Base_Parser!\n");
      return 0;
    }

    int next_prec = getBinOpPrecedence();
    if(cur_prec < next_prec)
    {
      RHS = binary_op_parser(cur_prec + 1, RHS);
      if(!RHS)
      {
	printf("Error in binary_op_parser: from binary_op_parser!\n");
	return 0;
      }
    }
    LHS = new BinaryAST(std::to_string(BinOp), LHS, RHS);
  }
}

static void HandleDefn()
{
  if(FunctionDefnAST *F = func_defn_parser())
  {
    delete F;
  }
  else
  {
    printf("Error in HandleDefn\n!");
  }
  return;
}

static void HandleTopExpression()
{
  if(BaseAST *E = expression_parser())
  {
    delete E;
  }
  else
  {
    printf("Error in HandleTopExpression\n");
  }
  return;
}

static void Driver()
{
  switch(Current_token)
  {
    case EOF_TOKEN:
      return;
    case SEMI_TOKEN:
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

static  LLVMContext context;

int main(int argc, char **argv)
{

  init_precedence();
  file = fopen(argv[1], "r");
  if(file == NULL)
  {
    printf("Error: unable to open %s.\n", argv[1]);
    return 0;
  }

  next_token();
  Module *Module_ob = new Module("my compiler", context);

  Driver();
  Module_ob->dump();

  fclose(file);
}


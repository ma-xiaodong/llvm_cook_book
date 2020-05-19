# llvm_cook_book
## Chap 2

### toy.cpp

实现了一些抽象语法树类（Abstract Syntax Tree）。

BaseAST，一个基础的函数是code_gen，下面描述每个子类的code_gen的功能。

1. VariableAST，根据变量名字，从数组Named_Values中返回对应的Value类型的变量。
2. NumericAST，返回对应的常量值。
3. BinaryAST，调用左右操作数所对应的code_gen，并对返回值施加指定的操作。
4. FunctionDeclAST，根据函数的名字、参数个数和返回值，生成一个Function*类型的变量F，并在数组Named_Values中增加其参数的对应描述。
5. FunctionDefnAST，上述的AST子类，都只介绍了其code_gen函数，因为其它函数都相对比较简单。本类除了code_gen函数之外，还需要介绍构造函数。
   1. 构造函数中需要注意的就是对分别为FunctionDeclAST和BaseASTbody类型的两个变量的赋值，这相当于定义所对应的函数声明和函数体。
   2. code_gen函数的功能，是把所定义的函数体插入到函数声明中去，并且返回所得到的完整的函数。
6. FunctionCallAST，需要描述其3个成员函数。
   1. 构造函数。为两个成员变量Function_Callee和Function_Arguments赋值。
   2. 析构函数。释放Function_Arguments所占用的内存。
   3. code_gen函数。从代表函数名的变量Function_Callee中获取函数，用Function_Arguments填充参数，然后生成可以调用的函数。

主体程序的执行流程。感觉注释标志有问题。



INC_DIR=/usr/local/llvm-5.0/include
LIB_DIR=/usr/local/llvm-5.0/lib
LIBS=`llvm-config --libs`

toy: toy.cpp
	clang++ -g -std=c++11 -I${INC_DIR} -L${LIB_DIR} toy.cpp ${LIBS} -lpthread -lncurses -o ./build/toy

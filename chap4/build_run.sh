cd ./build
make
cd ../
opt -load ./build/libchap4.so -fc exam_00.ll -disable-output -debug-pass=Structure

cd ./build
rm -rf *
cmake ../
make
cd ../
opt -load ./build/libFunCount.so -fc exam_00.ll -disable-output -debug-pass=Structure

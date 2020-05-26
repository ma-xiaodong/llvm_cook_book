cd ./build
rm -rf *
cmake ../
make
cd ../
opt -load ./build/libInstCount.so -oc exam_00.bc -disable-output -debug-pass=Structure

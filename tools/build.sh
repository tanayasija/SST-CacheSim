cd ../../../../../
./autogen.sh
cd build/
../configure --prefix=$PWD/../sst-elements-install
make install
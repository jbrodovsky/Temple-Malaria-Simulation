#!/usr/bin/sh
# Use this script when building on owlsnest. Owlsnest is a shared HPC cluster and 
# does not have Docker installed or permit sudo access for typical user. This script
# effectively recreates the Dockerfile in a shell script.
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
# add vcpkg to path
export PATH=$PATH:$(pwd)/vcpkg
export VCPKG_ROOT=./vcpkg
export CMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
# Load Owlsnest modules
module load git/
module load gcc/12.2.0
module load cmake/3.22.1
module load git/2.34.1
# installing lz4 for libpqxx support
#export CFLAGS="-std=c99"
#export CXXFLAGS="-std=c++11"
#vcpkg install lz4
# installing openssl
#export CFLAGS="-DOPENSSL_NO_ASM" 
#export CXXFLAGS="-DOPENSSL_NO_ASM" 
#vcpkg install openssl 
# ./vcpkg install gsl yaml-cpp fmt date args CLI11 gtest catch easyloggingpp sqlite3 libpqxx
# Build the project
mkdir -p build 
cd build 
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake .. 
make -j8
cp -r ./bin/* ../bin/
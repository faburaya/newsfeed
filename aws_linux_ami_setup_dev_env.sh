#!/bin/bash

# # #
# SETUP DEV ENV
#
sudo yum groups install development
sudo yum groups install development-libs
sudo yum install clang
export CC=clang
export CXX=clang++
wget https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
tar -xf cmake-3.8.2.tar.gz
cd cmake-3.8.2
./bootstrap
make
sudo make install
export PATH=$PATH:/usr/local/bin
cd ..
rm -rf ./cmake*

# # #
# INSTALL BOOST
#
wget https://dl.bintray.com/boostorg/release/1.65.1/source/boost_1_65_1.tar.bz2
tar -xf boost_1_65_1.tar.bz2
cd boost_1_65_1/cd tools/build/
./bootstrap.sh
cd ../../
./tools/build/b2 -j 2 variant=debug link=static threading=multi toolset=clang runtime-link=shared --layout=tagged
./tools/build/b2 -j 2 variant=release link=static threading=multi toolset=clang runtime-link=shared --layout=tagged
export BOOST_HOME=/opt/boost-1.65.1
sudo mkdir $BOOST_HOME
sudo mkdir $BOOST_HOME/include
sudo mkdir $BOOST_HOME/lib
sudo mv stage/lib/* $BOOST_HOME/lib/
sudo mv boost $BOOST_HOME/include/
cd ..
rm -rf boost_1_65_1
rm boost_1_65_1.tar.bz2

# # #
# INSTALL POCO
#
wget https://pocoproject.org/releases/poco-1.7.8/poco-1.7.8p3.tar.gz
tar -xf poco-1.7.8p3.tar.gz
cd poco-1.7.8p3
./configure --config=Linux-clang --static --no-tests --no-samples --prefix=/opt/poco-1.7.8p3
make -s -j2
sudo make install
cd ..
rm -rf poco-1.7.8p3
rm poco-1.7.8p3.tar.gz

# # #
# INSTALL AWS C++ SDK
#
git clone https://github.com/aws/aws-sdk-cpp.git
mkdir aws-sdk-cpp-build
cd aws-sdk-cpp-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="dynamodb" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF ../aws-sdk-cpp
make
sudo make install
cd ..
rm -rf ./aws-sdk-cpp*

# # #
# INSTALL GRPC
#
git clone https://github.com/grpc/grpc -b v1.6.x
cd grpc
git submodule update --init
make
sudo make install
cd ../third_party/zlib
make
sudo make install
cd ../protobuf/
sudo make install
cd ../../../../
rm -rf grpc

# # #
# BUILD NEWSFEED
#
git clone https://github.com/faburaya/newsfeed.git
cd newsfeed
chmod +x *.sh
./configure.sh
./build.sh

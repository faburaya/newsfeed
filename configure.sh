#!/bin/bash

UBUNTU=$(cat /etc/issue | grep -i ubuntu | awk -F" " '{print $2}' | awk -F"." '{print $1}')
AMAZON=$(cat /etc/issue | grep -i "amazon linux ami release" | awk -F" " '{print $5}' | awk -F"." '{print $1}')

if [ -n "$UBUNTU" ] && [ $UBUNTU -le 14 ]; then
    export AWS_SDK_INSTALLATION=/usr/local/lib/x86_64-linux-gnu
elif [ -n "$UBUNTU" ] && [ $UBUNTU -gt 14 ]; then
    export AWS_SDK_INSTALLATION=/usr/local/lib
elif [ -n "$AMAZON" ] && [ $AMAZON = 2017 ]; then
    export AWS_SDK_INSTALLATION=/usr/local/lib64
else
    echo OS UNKNOWN
    exit
fi

CMAKE_OPTIONS='-DCMAKE_BUILD_TYPE=Release -DCMAKE_DEBUG_POSTFIX=d -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-std=c++11"'

{ ls lib || mkdir lib; } &> /dev/null
{ ls bin || mkdir bin; } &> /dev/null

cd common
echo Cleaning common...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring common...
cmake $CMAKE_OPTIONS

cd ../newsfeed_server
echo Cleaning newsfeed_server...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring newsfeed_server...
cmake $CMAKE_OPTIONS

cd ../newsfeed_client
echo Cleaning newsfeed_client...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring newsfeed_client...
cmake $CMAKE_OPTIONS

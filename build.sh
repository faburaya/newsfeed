#!/bin/bash

cd common
make
cd ../newsfeed_server
make install
cd ../newsfeed_client
make install
cd ../bin
ls -l


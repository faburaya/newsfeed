#!/bin/bash
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server.config
chmod +x newsfeed_server
sudo ./newsfeed_server

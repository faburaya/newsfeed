#!/bin/bash

wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server.config
chmod +x newsfeed newsfeed_server
mv newsfeed /etc/init.d/
mkdir /opt/newsfeed
mv newsfeed* /opt/newsfeed/
chkconfig --add newsfeed
/etc/init.d/newsfeed start

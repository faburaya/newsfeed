#!/bin/bash

wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed
chmod +x newsfeed
mv newsfeed /etc/init.d/
chkconfig --add newsfeed
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server
wget https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server.config
chmod +x newsfeed_server
mkdir /opt/newsfeed
mv newsfeed* /opt/newsfeed/
/etc/init.d/newsfeed start

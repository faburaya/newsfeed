This project is a news feed service that runs in AWS.

The service host is based in GRPC, which means it keeps persistent TCP connections with the clients using HTTP/2 and serialization by google protocol buffers. The idea of this POC came from a task assignment (cannot disclose the origin) that imposed the interface of the service. Also, it should rely on DynamoDB as storage, so as much as it could have been better designed by relying more on AWS SNS, this is the best I could do with the time I had at my disposal :)

(The service is stateless because it was meant to be scalable, which means you could deploy it in several instances and have load balancing.)

For deployment, choose "Amazon Linux AMI release 2017.3".

In order to build the project, just run the script 'aws_linux_ami_setup_dev_env.sh'. It will build & install all the dependencies, and in the end it will clone this repository and build this project as well. It will probably take a while, since it is built by clang & cmake and depends on the static libraries of Boost, Poco, GRPC and AWS C++ SDK.

Normally, when you get those libraries from pre-built packages, they come in the shared form (*.so), which is why they are not fit and you have to build the static form yourself. The choice for using static libraries is purely practical: this way the final binary is more independent of third party packages, and you can download the pre-built binaries and run them without having to do anything else (as long as you are using Amazon Linux AMI release 2017.3).

I have made available the pre-built binaries in the following locations:

https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_client
https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server
https://s3-sa-east-1.amazonaws.com/faburaya-builds/aws_newsfeed_v1_2017.09/newsfeed_server.config

Naturally, besides the installation, you need to setup the DynamoDB tables. You can find their definitions in the cloud formation template file 'deployment.json'. (Or, if you just want to run the application, deploy it in AWS by creating a stack with such template.)

Currently, because the cloud formation relies on an AMI image that I used for development, the only supported region is "us-east-1" (USA North Virginia). You can change that if you modify the XML configuration file (/opt/newsfeed/newsfeed_server.config) and cloud formation template.

Please notice that in the configuration file you can set your AWS credentials (and region), which allows you to choose the DynamoDB tables in other AWS accounts. That was meant for development enviroments other than AWS EC2 instances. You can leave the credentials blank if you are running the service from a EC2 instance that has an AIM role for accessing DynamoDB service. Whenever you set your credentials, they take precedence to define the account servicing DynamoDB.

In order to operate the client application, just type 'help' for assistance. Be aware that the client does not automatically shows the received responses. That is on purpose, because it prevents messages printing to screen in the middle of something you were just writing. When you command "receive 3", then the client prompt blocks for 3 seconds, during which all the accumulated (and arriving) responses will be flushed to screen.

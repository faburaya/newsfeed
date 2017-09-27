protoc -I . --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` newsfeed_service.proto
protoc -I . --cpp_out=. newsfeed_messages.proto
mv *.h ./include/


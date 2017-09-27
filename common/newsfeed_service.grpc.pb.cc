// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: newsfeed_service.proto

#include "newsfeed_messages.pb.h"
#include "newsfeed_service.grpc.pb.h"

#include <grpc++/impl/codegen/async_stream.h>
#include <grpc++/impl/codegen/async_unary_call.h>
#include <grpc++/impl/codegen/channel_interface.h>
#include <grpc++/impl/codegen/client_unary_call.h>
#include <grpc++/impl/codegen/method_handler_impl.h>
#include <grpc++/impl/codegen/rpc_service_method.h>
#include <grpc++/impl/codegen/service_type.h>
#include <grpc++/impl/codegen/sync_stream.h>
namespace newsfeed {
namespace proto {

static const char* Newsfeed_method_names[] = {
  "/newsfeed.proto.Newsfeed/Talk",
};

std::unique_ptr< Newsfeed::Stub> Newsfeed::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  std::unique_ptr< Newsfeed::Stub> stub(new Newsfeed::Stub(channel));
  return stub;
}

Newsfeed::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_Talk_(Newsfeed_method_names[0], ::grpc::RpcMethod::BIDI_STREAMING, channel)
  {}

::grpc::ClientReaderWriter< ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>* Newsfeed::Stub::TalkRaw(::grpc::ClientContext* context) {
  return new ::grpc::ClientReaderWriter< ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>(channel_.get(), rpcmethod_Talk_, context);
}

::grpc::ClientAsyncReaderWriter< ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>* Newsfeed::Stub::AsyncTalkRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::ClientAsyncReaderWriter< ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>::Create(channel_.get(), cq, rpcmethod_Talk_, context, tag);
}

Newsfeed::Service::Service() {
  AddMethod(new ::grpc::RpcServiceMethod(
      Newsfeed_method_names[0],
      ::grpc::RpcMethod::BIDI_STREAMING,
      new ::grpc::BidiStreamingHandler< Newsfeed::Service, ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>(
          std::mem_fn(&Newsfeed::Service::Talk), this)));
}

Newsfeed::Service::~Service() {
}

::grpc::Status Newsfeed::Service::Talk(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::newsfeed::proto::req_envelope, ::newsfeed::proto::req_envelope>* stream) {
  (void) context;
  (void) stream;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace newsfeed
}  // namespace proto

#ifndef SERVER_IMPL_H // header guard
#define SERVER_IMPL_H

#include "newsfeed_service.grpc.pb.h"
#include <string>


namespace newsfeed
{
    using std::string;

    using namespace grpc;


    typedef ServerReaderWriter<proto::req_envelope, proto::req_envelope> IOStream;

    typedef WriterInterface<proto::req_envelope> OutStream;

    /// <summary>
    /// Implements the Newsfeed web service host.
    /// </summary>
    /// <seealso cref="proto::Newsfeed::Service" />
    class ServiceHostImpl final : public proto::Newsfeed::Service
    {
    public:

        virtual Status Talk(ServerContext *context, IOStream *stream) override;

    private:

        Status Respond(const proto::register_request &message,
                       proto::global_error_t error,
                       string &userId,
                       string &topic,
                       proto::req_envelope &respBuffer,
                       OutStream &stream);

        Status Respond(const proto::topic_request &message,
                       proto::global_error_t error,
                       const string &userId,
                       string &topic,
                       proto::req_envelope &respBuffer,
                       OutStream &stream);

        Status Respond(const proto::post_news_request &message,
                       proto::global_error_t error,
                       const string &userId,
                       const string &topic,
                       proto::req_envelope &respBuffer,
                       OutStream &stream);
    };

}// end of namespace newsfeed

#endif // end of header guard

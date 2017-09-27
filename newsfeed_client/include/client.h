#ifndef CLIENT_H // header guard
#define CLIENT_H

#include "newsfeed_service.grpc.pb.h"
#include "newsfeed_messages.pb.h"
#include <grpc++/channel.h>
#include <memory>
#include <string>
#include <future>
#include <mutex>
#include <atomic>
#include <functional>

namespace newsfeed
{
    using std::string;


    /// <summary>
    /// Newsfeed service client.
    /// </summary>
    class ServiceClient
    {
    private:

        std::unique_ptr<proto::Newsfeed::Stub> m_stub;

        grpc::ClientContext m_context;

        proto::req_envelope m_request;

        std::future<bool> m_responseHandlerFuture;

        std::future<bool> m_requestSenderFuture;

        std::mutex m_reqAccessMutex;

        std::atomic<bool> m_shutdownFlag;

        std::function<void(const string &)> m_callbackOnNews;

        typedef std::shared_ptr<grpc::ClientReaderWriter<proto::req_envelope, proto::req_envelope>> IOStream;

        bool ReceiveResponses(IOStream stream);

        bool SendRequests(IOStream stream);

    public:

        ServiceClient(std::shared_ptr<grpc::Channel> channel);

        ~ServiceClient();

        void StartTalk(std::function<void (const string &)> callbackOnNews);

        void Register(const string &userId);

        void ChangeTopic(const string &topic);

        void PostNews(const string &news);

        bool IsOkay() const;

        void StopTalk();
    };
}

#endif // end of header guard

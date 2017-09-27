#include "client.h"
#include "common.h"
#include "InterleavedConsole.h"
#include <sstream>
#include <chrono>


namespace newsfeed
{
    using namespace std::chrono;


    /// <summary>
    /// Initializes a new instance of the <see cref="ServiceClient"/> class.
    /// </summary>
    /// <param name="channel">The communication channel.</param>
    ServiceClient::ServiceClient(std::shared_ptr<grpc::Channel> channel)
    try
        : m_stub(proto::Newsfeed::NewStub(channel))
        , m_context()
        , m_request()
        , m_responseHandlerFuture()
        , m_requestSenderFuture()
        , m_reqAccessMutex()
        , m_shutdownFlag(false)
    {
    }
    catch (std::system_error &ex)
    {
        std::ostringstream oss;
        oss << "System error when creating client: " << StdLibExt::GetDetailsFromSystemError(ex);
        throw AppException(oss.str());
    }
    catch (std::future_error &ex)
    {
        std::ostringstream oss;
        oss << "System error when creating client: " << StdLibExt::GetDetailsFromFutureError(ex);
        throw AppException(oss.str());
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when creating client: " << ex.what();
        throw AppException(oss.str());
    }


    /// <summary>
    /// Gets the message for a given error code.
    /// </summary>
    /// <param name="code">The error code.</param>
    /// <returns>The corresponding error message.</returns>
    static const char *GetErrorMessage(proto::global_error_t code)
    {
        switch (code)
        {
        case newsfeed::proto::ok:
            return nullptr;

        case newsfeed::proto::not_registered:
            return "server refused request because the user is not registered!";

        case newsfeed::proto::internal:
            return "server internal error!";

        default:
            return "server replied with unknown error code!";
        }
    }


    /// <summary>
    /// Throws an exception whenever the code means something went wrong
    /// </summary>
    /// <param name="code">The error code.</param>
    static void ThrowOnError(proto::global_error_t code)
    {
        if (code != newsfeed::proto::ok)
            throw AppException(GetErrorMessage(code));
    }


    // Handles response of registration request
    static void HandleResponse(const proto::register_response &message)
    {
        ThrowOnError(message.error());

        InterleavedConsole::Get().EnqueueLine(
            "registration successfull: user is currently subscribing to '%s'",
            message.topic().c_str()
        );
    }


    // Handles response of topic change request
    static void HandleResponse(const proto::topic_response &message)
    {
        if (message.error() != proto::global_error_t::ok)
        {
            InterleavedConsole::Get().EnqueueLine(
                "error! topic change failed: %s",
                GetErrorMessage(message.error())
            );
            return;
        }

        auto action = message.action();

        switch (action)
        {
        case newsfeed::proto::subscribe:
            InterleavedConsole::Get().EnqueueLine("subscribed successfully to new topic");
            break;

        case newsfeed::proto::unsubscribe:
            InterleavedConsole::Get().EnqueueLine("unsubscribed successfully from topic\n"
                                                  "you are currently subscribing to NO topics and will NOT receive any news");
            break;

        default:
            InterleavedConsole::Get().EnqueueLine("error! server replied with unknown action for topic change: %d", action);
            break;
        }
    }


    // Handles response of post news request
    static void HandleResponse(const proto::post_news_response &message)
    {
        if (message.error() != proto::global_error_t::ok)
        {
            InterleavedConsole::Get().EnqueueLine(
                "error! post failed: %s",
                GetErrorMessage(message.error())
            );
        }
    }


    /// <summary>
    /// Receives responses in the connection open with the service host.
    /// This is meant to be run in a separate thread.
    /// </summary>
    /// <param name="stream">The connection input stream.</param>
    bool ServiceClient::ReceiveResponses(IOStream stream)
    {
        try
        {
            proto::req_envelope response;

            while (!m_shutdownFlag.load(std::memory_order_acquire))
            {
                if (!stream->Read(&response))
                    return STATUS_FAIL;

                bool uncompliantPayload(false);

                auto respType = response.type();

                switch (respType)
                {
                case proto::req_envelope_msg_type_register_response_t:

                    if ((uncompliantPayload = !response.has_reg_resp()))
                        break;

                    HandleResponse(response.reg_resp());
                    break;

                case proto::req_envelope_msg_type_topic_response_t:

                    if ((uncompliantPayload = !response.has_topic_resp()))
                        break;

                    HandleResponse(response.topic_resp());
                    break;

                case proto::req_envelope_msg_type_post_news_response_t:

                    if ((uncompliantPayload = !response.has_post_resp()))
                        break;

                    HandleResponse(response.post_resp());
                    break;

                case proto::req_envelope_msg_type_news_t:

                    if ((uncompliantPayload = !response.has_news_data()))
                        break;

                    m_callbackOnNews(response.news_data().data());
                    break;

                case proto::req_envelope_msg_type_register_request_t:
                case proto::req_envelope_msg_type_topic_request_t:
                case proto::req_envelope_msg_type_post_news_request_t:
                    InterleavedConsole::Get().EnqueueLine("error! received a response whose type is unexpected: %d", respType);
                    break;

                default:
                    InterleavedConsole::Get().EnqueueLine("error! received a response whose type is unknown: %d", respType);
                    break;
                }

                if (uncompliantPayload)
                    InterleavedConsole::Get().EnqueueLine("error! response payload is uncompliant with message type: %d", respType);

                response.Clear();

            }// end of loop

            return STATUS_OKAY;
        }
        catch (AppException &ex)
        {
            std::cerr << "\nERROR - News feed client - On response handler thread - "
                      << ex.ToString() << std::endl;

            return STATUS_FAIL;
        }
        catch (std::system_error &ex)
        {
            std::cerr << "\nERROR - News feed client - On response handler thread - System error: "
                      << StdLibExt::GetDetailsFromSystemError(ex) << std::endl;

            return STATUS_FAIL;
        }
        catch (std::exception &ex)
        {
            std::cerr << "\nERROR - News feed client - On response handler thread - Generic failure: "
                      << ex.what() << std::endl;

            return STATUS_FAIL;
        }
    }


    /// <summary>
    /// Sends requests in the connection open with the service host.
    /// This is meant to be run in a separate thread.
    /// </summary>
    /// <param name="stream">The connection IO stream.</param>
    bool ServiceClient::SendRequests(IOStream stream)
    {
        try
        {
            while (!m_shutdownFlag.load(std::memory_order_acquire))
            {
                // quick nap
                std::this_thread::sleep_for(
                    std::chrono::seconds(1)
                );

                // acquire lock to access request buffer
                std::lock_guard<std::mutex> lock(m_reqAccessMutex);

                // no request available in the buffer?
                if (!m_request.has_type())
                    continue;

                // issue request
                if (!stream->Write(m_request))
                    throw AppException("Failed to write on stream when attempting to send request!");

                m_request.Clear();

            }// end of loop

            // close stream
            if (!stream->WritesDone())
                throw AppException("Failed to close connection output stream!");

            // finish connection
            auto status = stream->Finish();

            if (!status.ok())
            {
                std::ostringstream oss;
                oss << "code " << status.error_code()
                    << "; " << status.error_message()
                    << "; " << status.error_details();

                throw AppException("End of connection reported NOT OKAY status!", oss.str());
            }

            return STATUS_OKAY;
        }
        catch (AppException &ex)
        {
            std::cerr << "\nERROR - News feed client - On request sender thread - "
                      << ex.ToString() << std::endl;

            return STATUS_FAIL;
        }
        catch (std::system_error &ex)
        {
            std::cerr << "\nERROR - News feed client - On request sender thread - System error: "
                      << StdLibExt::GetDetailsFromSystemError(ex) << std::endl;

            return STATUS_FAIL;
        }
        catch (std::exception &ex)
        {
            std::cerr << "\nERROR - News feed client - On request sender thread - Generic failure: "
                      << ex.what() << std::endl;

            return STATUS_FAIL;
        }
    }


    /// <summary>
    /// Starts a thread where a persistent connection will be kept
    /// with the service host for exchange of requests & responses.
    /// </summary>
    /// <param name="callbackOnNews">The callback to invoke whenever news arrive.</param>
    void ServiceClient::StartTalk(std::function<void(const string &)> callbackOnNews)
    {
        try
        {
            m_callbackOnNews = callbackOnNews;

            // establish connection with host
            IOStream stream(
                m_stub->Talk(&m_context).release()
            );

            // cannot have 2 conversation with the host!

            assert(!m_requestSenderFuture.valid()
                   || m_requestSenderFuture.wait_for(seconds(0)) == std::future_status::ready);

            assert(!m_responseHandlerFuture.valid()
                   || m_responseHandlerFuture.wait_for(seconds(0)) == std::future_status::ready);

            m_shutdownFlag.store(false, std::memory_order_release);

            // start a thread to receive the responses:
            m_responseHandlerFuture =
                std::async(
                    std::launch::async,
                    [this](IOStream stream) { return ReceiveResponses(stream); },
                    stream
                );

            // start a thread to send the requests:
            m_requestSenderFuture =
                std::async(
                    std::launch::async,
                    [this](IOStream stream) { return SendRequests(stream);  },
                    stream
                );
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when setting up conversation with host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when setting up conversation with host: " << StdLibExt::GetDetailsFromFutureError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when setting up conversation with host: " << ex.what();
            throw AppException(oss.str());
        }
    }


    /// <summary>
    /// Registers the specified user identifier.
    /// </summary>
    /// <param name="userId">The user identifier.</param>
    void ServiceClient::Register(const string &userId)
    {
        try
        {
            // acquire lock to write into request buffer
            std::lock_guard<std::mutex> lock(m_reqAccessMutex);

            m_request.set_type(proto::req_envelope_msg_type::req_envelope_msg_type_register_request_t);
            m_request.mutable_reg_req()->set_userid(userId);

            /* at end of scope, buffer is available to be read by the thread
               sending messages to the host, which will issue this request */
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when preparing registration request: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when preparing registration request: " << ex.what();
            throw AppException(oss.str());
        }
    }
    

    /// <summary>
    /// Changes the topic.
    /// </summary>
    /// <param name="topic">The new topic to subscribe, or an empty string
    /// if unsubscribing (withdrawing current subscription).</param>
    void ServiceClient::ChangeTopic(const string &topic)
    {
        try
        {
            // acquire lock to write into request buffer
            std::lock_guard<std::mutex> lock(m_reqAccessMutex);

            m_request.set_type(proto::req_envelope_msg_type::req_envelope_msg_type_topic_request_t);
            
            m_request.mutable_topic_req()->set_action(
                !topic.empty()
                    ? proto::topic_action_t::subscribe
                    : proto::topic_action_t::unsubscribe
            );
            
            m_request.mutable_topic_req()->set_topic(topic);

            /* at end of scope, buffer is available to be read by the thread
               sending messages to the host, which will issue this request */
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when preparing request for change of topic: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when preparing request for change of topic: " << ex.what();
            throw AppException(oss.str());
        }
    }


    /// <summary>
    /// Posts the news.
    /// </summary>
    /// <param name="news">The news.</param>
    void ServiceClient::PostNews(const string &news)
    {
        try
        {
            // acquire lock to write into request buffer
            std::lock_guard<std::mutex> lock(m_reqAccessMutex);

            m_request.set_type(proto::req_envelope_msg_type::req_envelope_msg_type_post_news_request_t);
            m_request.mutable_post_req()->set_news(news);

            /* at end of scope, buffer is available to be read by the thread
               sending messages to the host, which will issue this request */
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when preparing request to post news: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when preparing request to post news: " << ex.what();
            throw AppException(oss.str());
        }
    }


    /// <summary>
    /// Determines whether the connection to the host is okay.
    /// </summary>
    /// <returns>
    ///   <c>true</c> if the connection is okay; otherwise, <c>false</c>.
    /// </returns>
    bool ServiceClient::IsOkay() const
    {
        return m_requestSenderFuture.valid()
            && m_responseHandlerFuture.valid()
            && m_requestSenderFuture.wait_for(seconds(0)) == std::future_status::timeout
            && m_responseHandlerFuture.wait_for(seconds(0)) == std::future_status::timeout;
    }


    /// <summary>
    /// Stops the exchange of messages with the host and closes the connection.
    /// </summary>
    void ServiceClient::StopTalk()
    {
        try
        {
            /* Signalize to thread to stop loop for
               message exchange and close the connection */
            m_shutdownFlag.store(true, std::memory_order_release);

            // wait for threads termination:

            if (m_requestSenderFuture.valid()
                && m_requestSenderFuture.get() == STATUS_OKAY)
            {
                InterleavedConsole::Get()
                    .PrintLine("Thread dedicated to send requests has finalized OKAY");
            }
            
            if (m_responseHandlerFuture.valid()
                && m_responseHandlerFuture.get() == STATUS_OKAY)
            {
                InterleavedConsole::Get()
                    .PrintLine("Thread dedicated to receive responses has finalized OKAY");
            }
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when finishing conversation with host: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when finishing conversation with host: " << StdLibExt::GetDetailsFromFutureError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when finishing conversation with host: " << ex.what();
            throw AppException(oss.str());
        }
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="ServiceClient"/> class.
    /// </summary>
    ServiceClient::~ServiceClient()
    {
        try
        {
            StopTalk();
        }
        catch (AppException &ex)
        {
            std::cerr << "\nERROR - News feed service client (upon termination) - " << ex.ToString() << std::endl;
        }
    }

}// end of namespace newsfeed

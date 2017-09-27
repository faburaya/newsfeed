#include "server_impl.h"
#include "common.h"
#include "configuration.h"
#include "DDBAccess.h"
#include <exception>
#include <iostream>
#include <sstream>
#include <chrono>
#include <atomic>
#include <thread>
#include <future>

namespace newsfeed
{
    using namespace std::chrono;


    ///////////////////
    // Helpers
    ///////////////////

    static void DumpMessage(const proto::register_request &msg)
    {
        std::clog << "Received register_request message: { userid = '"
                  << msg.userid() << "' }\n" << std::endl;
    }

    static void DumpMessage(const proto::topic_request &msg)
    {
        std::clog << "Received topic_request message: { action = " << msg.action()
                  << ", topic = '" << msg.topic() << "' }\n" << std::endl;
    }

    static void DumpMessage(const proto::post_news_request &msg)
    {
        std::clog << "Received post_news_request message: { news = '"
                  << msg.news() << "' }\n" << std::endl;
    }


    /// <summary>
    /// Logs server error information.
    /// </summary>
    /// <param name="message">The error main message.</param>
    /// <param name="details">The error details.</param>
    static void LogError(const char *message, const string &details)
    {
        std::cerr << "ERROR - " << message;

        if (!details.empty())
            std::cerr << " - " << details;

        std::cerr << std::endl;
    }


    /// <summary>
    /// Makes a <c>grpc::Status</c> object for an error situation.
    /// </summary>
    /// <param name="code">The status code.</param>
    /// <param name="message">The error message.</param>
    /// <param name="details">The error details.</param>
    /// <returns>The constructed <c>grpc::Status</c> object.</returns>
    static Status ErrorStatus(StatusCode code, const char *message, const string &details)
    {
        LogError(message, details);
        return Status(code, message, details);
    }


    ///////////////////////
    // SimpleSignal Class
    ///////////////////////

    /// <summary>
    /// Simple signal that guarantees
    /// signalization upon end of scope
    /// </summary>
    class SimpleSignal
    {
    private:

        std::atomic<bool> m_flag;

    public:

        SimpleSignal()
            : m_flag(false) {}

        ~SimpleSignal()
        {
            m_flag.store(true, std::memory_order_relaxed);
        }

        void Set()
        {
            m_flag.store(true, std::memory_order_relaxed);
        }

        bool IsSet() const
        {
            return m_flag.load(std::memory_order_relaxed);
        }

        bool IsNotSet() const
        {
            return !m_flag.load(std::memory_order_relaxed);
        }
    };

    
    //////////////////////////
    // ServiceHostImpl Class
    //////////////////////////

    /// <summary>
    /// Responds a register request message.
    /// </summary>
    /// <param name="message">The message in the request.</param>
    /// <param name="error">The error to notify the client, if one happened.</param>
    /// <param name="userId">Will receive the user ID.</param>
    /// <param name="topic">Will receive the topic to which the customer last subscribed.</param>
    /// <param name="respBuffer">The response buffer.</param>
    /// <param name="stream">The output stream.</param>
    /// <returns>
    /// The operation status.
    /// </returns>
    Status ServiceHostImpl::Respond(const proto::register_request &message,
                                    proto::global_error_t error,
                                    string &userId,
                                    string &topic,
                                    proto::req_envelope &respBuffer,
                                    OutStream &stream)
    {
#   ifndef NDEBUG
        DumpMessage(message);
#   endif
        Status status(Status::OK);

        respBuffer.Clear();
        respBuffer.set_type(proto::req_envelope_msg_type_register_response_t);

        if (error == proto::global_error_t::ok)
        {
            try
            {
                DDBAccess::GetInstance().GetOrPutUser(message.userid(), topic);
                userId = message.userid();
            }
            catch (AppException &ex)
            {
                error = proto::global_error_t::internal;
                status = ErrorStatus(StatusCode::INTERNAL, ex.what(), ex.GetDetails());
            }

            *respBuffer.mutable_reg_resp()->mutable_topic() = topic;
        }

        respBuffer.mutable_reg_resp()->set_error(error);

        // send response
        if (!stream.Write(respBuffer))
        {
            status = ErrorStatus(StatusCode::UNKNOWN,
                                 "Failed to write message on stream!",
                                 "Attempted to respond registration request");
        }

        return status;
    }


    /// <summary>
    /// Responds a topic request message.
    /// </summary>
    /// <param name="message">The message.</param>
    /// <param name="error">The error to notify the client, if one happened.</param>
    /// <param name="userId">The user ID.</param>
    /// <param name="topic">Will receive the new topic.</param>
    /// <param name="respBuffer">The response buffer.</param>
    /// <param name="stream">The output stream.</param>
    /// <returns>
    /// The operation status.
    /// </returns>
    Status ServiceHostImpl::Respond(const proto::topic_request &message,
                                    proto::global_error_t error,
                                    const string &userId,
                                    string &topic,
                                    proto::req_envelope &respBuffer,
                                    OutStream &stream)
    {
#   ifndef NDEBUG
        DumpMessage(message);
#   endif
        respBuffer.Clear();
        respBuffer.set_type(proto::req_envelope_msg_type_topic_response_t);

        if (error == proto::global_error_t::ok)
        {
            // user not registered?
            if (userId.empty())
            {
                error = proto::global_error_t::not_registered;
                LogError("Failed to change topic!", "User is not registered");
            }
            // no topic specified for subscription?
            else if (message.action() == proto::topic_action_t::subscribe
                     && message.topic().empty())
            {
                error = proto::global_error_t::internal;
                LogError("Failed to change topic!", "No topic has been specified");
            }
            // topic specified for unsubscription?
            else if (message.action() == proto::topic_action_t::unsubscribe
                     && !message.topic().empty())
            {
                error = proto::global_error_t::internal;
                LogError("Failed to change topic!", "Must not specify topic when unsubscribing");
            }
        }

        respBuffer.mutable_topic_resp()->set_action(message.action());

        if (error == proto::global_error_t::ok)
        {
            string newTopic;

            if (message.action() == proto::topic_action_t::subscribe)
                newTopic = message.topic();

            try
            {
                DDBAccess::GetInstance().UpdateUser(userId, newTopic);
                topic = newTopic;
            }
            catch (AppException &ex)
            {
                LogError(ex.what(), ex.GetDetails());
                error = proto::global_error_t::internal;
            }
        }

        respBuffer.mutable_topic_resp()->set_error(error);

        // send response
        if (stream.Write(respBuffer))
            return Status::OK;
        else
        {
            return ErrorStatus(StatusCode::UNKNOWN,
                               "Failed to write message on stream!",
                               "Attempted to respond topic change request");
        }
    }


    /// <summary>
    /// Responds a post news request message.
    /// </summary>
    /// <param name="message">The message.</param>
    /// <param name="error">The error to notify the client, if one happened.</param>
    /// <param name="userId">The user ID.</param>
    /// <param name="topic">The topic where the news will be posted.</param>
    /// <param name="respBuffer">The response buffer.</param>
    /// <param name="stream">The output stream.</param>
    /// <returns>
    /// The operation status.
    /// </returns>
    Status ServiceHostImpl::Respond(const proto::post_news_request &message,
                                    proto::global_error_t error,
                                    const string &userId,
                                    const string &topic,
                                    proto::req_envelope &respBuffer,
                                    OutStream &stream)
    {
    #ifndef NDEBUG
        DumpMessage(message);
    #endif

        respBuffer.Clear();
        respBuffer.set_type(proto::req_envelope_msg_type_post_news_response_t);

        if (error == proto::global_error_t::ok)
        {
            // user not registered?
            if (userId.empty())
            {
                error = proto::global_error_t::not_registered;
                LogError("Failed to post news!", "User is not registered");
            }
            // not subscribing to any topic?
            else if (topic.empty())
            {
                error = proto::global_error_t::internal;
                LogError("Failed to post news!", "User is not subscribing to any topic");
            }
        }

        if (error == proto::global_error_t::ok)
        {
            try
            {
                DDBAccess::GetInstance().PutNews(topic, userId, message.news());
            }
            catch (AppException &ex)
            {
                LogError(ex.what(), ex.GetDetails());
                error = proto::global_error_t::internal;
            }
        }

        respBuffer.mutable_post_resp()->set_error(error);

        // send response
        if (stream.Write(respBuffer))
            return Status::OK;
        else
        {
            return ErrorStatus(StatusCode::UNKNOWN,
                               "Failed to write message on stream!",
                               "Attempted to respond post news request");
        }
    }


    /// <summary>
    /// Sends back to the client any available news in its subcribed topic.
    /// </summary>
    /// <param name="userId">The user ID.</param>
    /// <param name="endOfConnection">A signal for end of connection.</param>
    /// <param name="stream">The output stream.</param>
    /// <returns>
    /// The thread final status.
    /// </returns>
    Status SendAvailableNews(const string &userId,
                             const SimpleSignal &endOfConnection,
                             OutStream &stream)
    {
        try
        {
            proto::req_envelope buffer;
            buffer.set_type(proto::req_envelope_msg_type::req_envelope_msg_type_news_t);

            std::vector<string> news;

            while (endOfConnection.IsNotSet())
            {
                try
                {
                    DDBAccess::GetInstance().GetNews(userId, news);
                }
                catch (AppException &ex)
                {
                    LogError(ex.what(), ex.GetDetails());
                }

                for (auto &entry : news)
                {
                    buffer.mutable_news_data()->set_data(std::move(entry));
                    
                    if (!stream.Write(buffer))
                    {
                        return ErrorStatus(StatusCode::UNKNOWN,
                                           "Failed to write message on stream!",
                                           "Attempted to send news to client");
                    }
                }

                std::this_thread::sleep_for(
                    seconds(Configuration::Get().settings.newsPollingIntervalSecs)
                );
            }

            return Status::OK;
        }
        catch (std::exception &ex)
        {
            return ErrorStatus(StatusCode::INTERNAL,
                               "Generic failure in parallel thread",
                               ex.what());
        }
    }


    /// <summary>
    /// Receives and process a request from a client application.
    /// </summary>
    /// <param name="context">The call context.</param>
    /// <param name="stream">The server synchronous IO stream.</param>
    /// <returns>The status upon closure of connection.</returns>
    Status ServiceHostImpl::Talk(ServerContext *context, IOStream *stream)
    {
        try
        {
            Status status(Status::OK);

            SimpleSignal endOfConnection;
            
            std::future<Status> writerFuture;

            string curUserId;
            string curTopic;

            proto::req_envelope response;
            proto::req_envelope request;
            
            // loop interrupts when the connection is idle for too long
            while (stream->Read(&request))
            {
                auto error = proto::global_error_t::ok;

                bool uncompliantPayload;

                auto reqType = request.type();

                switch (reqType)
                {
                case proto::req_envelope_msg_type_register_request_t:

                    if ((uncompliantPayload = !request.has_reg_req()))
                        error = proto::global_error_t::internal;

                    if (!curUserId.empty())
                    {
                        error = proto::global_error_t::internal;
                        LogError("Could not register user!", "Only one registration per session is allowed");
                    }

                    status = Respond(request.reg_req(),
                                     error, 
                                     curUserId,
                                     curTopic,
                                     response,
                                     *stream);

                    if (!writerFuture.valid())
                    {
                        // Start a parallel thread to monitor for news and send back to the client:
                        writerFuture = std::async(std::launch::async,
                            [this, &curUserId, &endOfConnection, stream]()
                            {
                                return SendAvailableNews(curUserId, endOfConnection, *stream);
                            });
                    }
                    break;

                case proto::req_envelope_msg_type_topic_request_t:

                    if ((uncompliantPayload = !request.has_topic_req()))
                        error = proto::global_error_t::internal;

                    status = Respond(request.topic_req(),
                                     error,
                                     curUserId,
                                     curTopic,
                                     response,
                                     *stream);
                    break;

                case proto::req_envelope_msg_type_post_news_request_t:

                    if ((uncompliantPayload = !request.has_post_req()))
                        error = proto::global_error_t::internal;

                    status = Respond(request.post_req(),
                                     error,
                                     curUserId,
                                     curTopic,
                                     response,
                                     *stream);
                    break;

                case proto::req_envelope_msg_type_register_response_t:
                case proto::req_envelope_msg_type_topic_response_t:
                case proto::req_envelope_msg_type_post_news_response_t:
                    {
                        std::ostringstream oss;
                        oss << "News feed server has received a request whose type is unexpected: " << reqType;
                        status = ErrorStatus(StatusCode::DO_NOT_USE, "Unexpected message type!", oss.str());
                        break;
                    }

                default:
                    {
                        std::ostringstream oss;
                        oss << "News feed server has received a request whose type is unknown: " << reqType;
                        status = ErrorStatus(StatusCode::UNIMPLEMENTED, "Unknown message type!", oss.str());
                        break;
                    }
                }

                request.Clear();

                // any trouble giving the first response?

                if (uncompliantPayload)
                {
                    std::ostringstream oss;
                    oss << "Request payload is uncompliant with message type " << reqType;
                    status = ErrorStatus(StatusCode::INVALID_ARGUMENT, "Invalid request!", oss.str());
                    break;
                }

                if (!status.ok())
                    break;
                
                // any trouble to feed the client with news?
                if (writerFuture.wait_for(seconds(0)) == std::future_status::ready)
                    return writerFuture.get();

            }// end of loop

            /* We get here when there is no more messages or an error
               took place, so ask writer loop to finish as well: */

            endOfConnection.Set();
            
            if (!status.ok())
                return status;
            else
                return writerFuture.get();
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "News feed service host had a system error: " << StdLibExt::GetDetailsFromSystemError(ex);
            return ErrorStatus(StatusCode::INTERNAL, "Server error", oss.str());
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "News feed service host had a system error: " << StdLibExt::GetDetailsFromFutureError(ex);
            return ErrorStatus(StatusCode::INTERNAL, "Server error", oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "News feed service host had a generic failure: " << ex.what();
            return ErrorStatus(StatusCode::INTERNAL, "Server error", oss.str());
        }
    }

}// end of namespace newsfeed

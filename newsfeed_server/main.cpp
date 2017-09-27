#include <exception>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <csignal>
#include <aws/core/Aws.h>
#include <grpc++/server_builder.h>
#include <grpc++/security/server_credentials.h>
#include <grpc++/server.h>
#include "server_impl.h"
#include "configuration.h"

using std::string;


grpc::ServerInterface *serverIntfPtr(nullptr);

/// <summary>
/// Handles termination initiated by the user by Ctrl+C.
/// </summary>
/// <param name="signum">The received signal.</param>
void termSignalHandler(int signum)
{
    if (serverIntfPtr != nullptr)
    {
        std::cerr << "\nCtrl+C captured: news feed service host will be shutdown "
                     "when all current connections are closed..." << std::endl;

        serverIntfPtr->Shutdown();
        serverIntfPtr = nullptr;
    }
}


/// <summary>
/// Takes care of setup, initialization & finalization of AWS C++ SDK.
/// </summary>
class AwsCppSdk
{
private:

    Aws::SDKOptions m_options;

public:

    AwsCppSdk()
    {
        m_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        Aws::InitAPI(m_options);
    }

    ~AwsCppSdk()
    {
        Aws::ShutdownAPI(m_options);
    }
};


/// <summary>
/// ENTRY POINT
/// </summary>
int main(int argc, char *argv[])
{
    try
    {
        using namespace newsfeed;

        AwsCppSdk awsFramework;

        ServiceHostImpl newsfeedSvcHostImpl;

        const string &svcEndpoint = Configuration::Get().settings.serviceEndpoint;

        grpc::ServerBuilder srvBuilder;
        srvBuilder.AddListeningPort(svcEndpoint, grpc::InsecureServerCredentials());
        srvBuilder.RegisterService(&newsfeedSvcHostImpl);
        
        std::unique_ptr<grpc::Server> server = srvBuilder.BuildAndStart();

        serverIntfPtr = server.get(); // for global access (signal handling)

        signal(SIGINT, &termSignalHandler);

        std::cout << "News feed service host is listening on " << svcEndpoint << '\n' << std::endl;

        server->Wait();

        return EXIT_SUCCESS;
    }
    catch (std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}

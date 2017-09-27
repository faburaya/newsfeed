#include <iostream>
#include <fstream>
#include <string>
#include <exception>
#include <algorithm>
#include <grpc++/create_channel.h>
#include "client.h"
#include "common.h"
#include "InterleavedConsole.h"

#ifdef _WIN32
#    define strcasecmp stricmp
#endif


/// <summary>
/// Shows the help for possible actions when connected to host.
/// </summary>
static void ShowHelp()
{
    InterleavedConsole::Get().PrintLine(
        "Available commands are:\n\n"
        "\tsubscribe new_topic_name\n"
        "\tunsubscribe topic_name\n"
        "\tpost news_content\n"
        "\treceive for_seconds"
    );
}


// ENTRY POINT
int main(int argc, char *argv[])
{
    using std::string;

    using namespace newsfeed;

    try
    {
        if (argc != 3)
        {
            std::cerr << "ERROR - Invalid arguments! Usage:\n\n"
                      << "\tclient (host_address:port) (news_feed_user_id)\n\n" << std::endl;

            return EXIT_FAILURE;
        }

        const char *svcHostEndpoint = argv[1];
        const char *myUserId = argv[2];
        
        std::cout << "News feed service client:\n"
                  << "will connect to service host in " << svcHostEndpoint
                  << " identified as '" << myUserId << "'...\n" << std::endl;

        // create service client
        ServiceClient client(
            grpc::CreateChannel(svcHostEndpoint, grpc::InsecureChannelCredentials())
        );

        // will handle arriving news in a parallel thread
        auto displayNewsHandler = [](const string &news)
        {
            time_t now = time(nullptr);

            char timestamp[21];
            strftime(timestamp,
                     sizeof timestamp,
                     "%Y-%b-%d %H:%M:%S",
                     localtime(&now));

            InterleavedConsole::Get().EnqueueLine("NEWS @(%s): %s", timestamp, news.c_str());
        };

        /* establish a persistent connection to the host in order
           to start a conversation based on requests & responses
           that are issued and received in other threads */
        
        client.StartTalk(displayNewsHandler);

        // login to news feed
        client.Register(myUserId);

        // in this loop we receive instructions from the command line:
        while (client.IsOkay())
        {
            string line = InterleavedConsole::Get().GetLine();

            // parse the line:

            auto lineCStr = line.c_str();
            auto whiteSpaceIdx = strcspn(lineCStr, "\t ");

            if (lineCStr[whiteSpaceIdx] == 0
                || lineCStr[whiteSpaceIdx + 1] == 0)
            {
                if (strcasecmp(lineCStr, "unsubscribe") == 0) // unsubscribe
                {
                    client.ChangeTopic("");
                }
                else if (strcasecmp(lineCStr, "quit") == 0) // exit
                {
                    client.StopTalk();
                    break;
                }
                else if (strcasecmp(lineCStr, "help") == 0) // show help
                {
                    ShowHelp();
                }
                else if (whiteSpaceIdx != 0)
                {
                    InterleavedConsole::Get().PrintLine("unknown action (or wrong syntax)!");
                    ShowHelp();
                }
                
                continue;
            }

            string parameter(lineCStr + whiteSpaceIdx + 1);
            string action(lineCStr, lineCStr + whiteSpaceIdx);

            // What action to take?

            if (strcasecmp(action.c_str(), "subscribe") == 0)
            {
                client.ChangeTopic(parameter);
            }
            else if (strcasecmp(action.c_str(), "post") == 0)
            {
                client.PostNews(parameter);
            }
            else if (strcasecmp(action.c_str(), "receive") == 0)
            {
                InterleavedConsole::Get().FlushQueueFor(
                    static_cast<uint16_t> (std::stoul(parameter.c_str()))
                );
            }
            else
            {
                InterleavedConsole::Get().PrintLine("unknown action (or wrong syntax)!");
                ShowHelp();
            }
        }

        InterleavedConsole::Get().FlushQueueFor(1);
        InterleavedConsole::Get().PrintLine("client application is closing");

        return EXIT_SUCCESS;
    }
    catch (AppException &ex)
    {
        std::cerr << "\nERROR - News feed client - " << ex.ToString() << std::endl;
        return EXIT_FAILURE;
    }
    catch (std::exception &ex)
    {
        std::cerr << "\nERROR - News feed client had a generic failure: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}

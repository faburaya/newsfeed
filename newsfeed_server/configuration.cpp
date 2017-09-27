#include "configuration.h"
#include <exception>
#include <sstream>
#include <Poco/AutoPtr.h>
#include <Poco/Util/XMLConfiguration.h>

namespace newsfeed
{
    std::unique_ptr<Configuration> Configuration::singleton;


    /// <summary>
    /// Prevents a default instance of the <see cref="Configuration"/> class from being created.
    /// </summary>
    Configuration::Configuration()
    {
        using Poco::AutoPtr;
        using Poco::Util::XMLConfiguration;

        // Load the configurations in the file
        AutoPtr<XMLConfiguration> config(new XMLConfiguration("./newsfeed_server.config"));

        settings.serviceEndpoint         = config->getString("entry[@key='serviceEndpoint'][@value]", "0.0.0.0:8080");
        settings.awsRegion               = config->getString("entry[@key='awsRegion'][@value]", "us-east-1");
        settings.awsAccessKeyId          = config->getString("entry[@key='awsAccessKeyId'][@value]", "undefined access key id");
        settings.awsSecretKey            = config->getString("entry[@key='awsSecretKey'][@value]", "undefined secret key");
        settings.dbReqMaxRetryCount      = config->getUInt("entry[@key='dbReqMaxRetryCount'][@value]", 2);
        settings.dbReqRetryIntervalMs    = config->getUInt("entry[@key='dbReqRetryIntervalMs'][@value]", 30);
        settings.dbOldNewsPurgeAgeSecs   = config->getUInt("entry[@key='dbOldNewsPurgeAgeSecs'][@value]", 60);
        settings.newsPollingIntervalSecs = config->getUInt("entry[@key='newsPollingIntervalSecs'][@value]", 5);
    }


    /// <summary>
    /// Gets the singleton.
    /// This implementatio is NOT THREAD SAFE.
    /// </summary>
    /// <returns>A read-only reference to the singleton.</returns>
    const Configuration & Configuration::Get()
    {
        try
        {
            if (singleton)
                return *singleton;

            singleton.reset(new Configuration());
            return *singleton;
        }
        catch (Poco::Exception &ex)
        {
            std::ostringstream oss;
            oss << "Failed to load configurations for newsfeed service host: " << ex.message();
            throw std::runtime_error(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented initialization of newsfeed service host: " << ex.what();
            throw std::runtime_error(oss.str());
        }
    }

}// end of namespace

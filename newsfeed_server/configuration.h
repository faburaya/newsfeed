#ifndef CONFIGURATION_H // header guard
#define CONFIGURATION_H

#include <string>
#include <memory>
#include <cinttypes>

namespace newsfeed
{
    using std::string;


    /// <summary>
    /// A singleton to hold the service host settings.
    /// This implementation is NOT THREAD SAFE, but that is okay, because
    /// initialization is meant to happen on entry point function, and after
    /// that we only have read access to the cached values.
    /// </summary>
    class Configuration
    {
    private:

        static std::unique_ptr<Configuration> singleton;

        Configuration();

    public:

        struct {

            string serviceEndpoint;

            string awsRegion;

            string awsAccessKeyId;

            string awsSecretKey;

            uint32_t dbReqMaxRetryCount;

            uint32_t dbReqRetryIntervalMs;

            uint32_t dbOldNewsPurgeAgeSecs;

            uint32_t newsPollingIntervalSecs;

        } settings;

        static const Configuration &Get();
    };

}// end of namespace newsfeed

#endif // end of header guard

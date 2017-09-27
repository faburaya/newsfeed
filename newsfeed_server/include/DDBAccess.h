#ifndef DDBACCESS_H // header guard
#define DDBACCESS_H

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <boost/lockfree/queue.hpp>
#include "DbConnPool.h"

namespace newsfeed
{
    using std::string;


    /// <summary>
    /// Provides access to AWS DynamoDB database.
    /// </summary>
    class DDBAccess
    {
    private:

        DbConnPool m_dbConnPool;

        static std::atomic<DDBAccess *> singletonAtomicPtr;

        static std::unique_ptr<DDBAccess> singleton;

        static std::mutex singletonCreationMutex;

        DDBAccess();

    public:

        static DDBAccess &GetInstance();

        ~DDBAccess();

        void GetOrPutUser(const string &userId, string &currentTopic);

        void UpdateUser(const string &userId, const string &topic);

        void PutNews(const string &topic, const string &userId, const string &news);

        void GetNews(const string &userId, std::vector<string> &news);
    };

}// end of namespace newsfeed

#endif // header guard

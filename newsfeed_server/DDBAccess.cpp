#include "DDBAccess.h"
#include "common.h"
#include "configuration.h"
#include <aws/core/Aws.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/GetItemResult.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>
#include <aws/dynamodb/model/UpdateItemResult.h>
#include <aws/dynamodb/model/DeleteItemRequest.h>
#include <aws/dynamodb/model/DeleteItemResult.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/QueryResult.h>
#include <aws/dynamodb/model/BatchWriteItemRequest.h>
#include <aws/dynamodb/model/BatchWriteItemResult.h>
#include <aws/core/utils/Outcome.h>
#include <sstream>
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <array>

#define DDB_TABNAME_TOPIC_BY_USER "newsfeed_topic_by_user"
#define DDB_TABATTR_TBU_PK_USER   "user_id"
#define DDB_TABATTR_TBU_TOPIC     "topic"
#define DDB_TABATTR_TBU_LFTIME    "last_feed_time"

#define DDB_TABNAME_NEWS_BY_TOPIC "newsfeed_news_by_topic"
#define DDB_TABATTR_NBT_PK_TOPIC  "topic"
#define DDB_TABATTR_NBT_SK_BINTB  "bin_time_based_sk"
#define DDB_TABATTR_NBT_NEWS      "news"


namespace newsfeed
{
    using namespace Aws::DynamoDB::Model;

    typedef Aws::Map<Aws::String, AttributeValue> AwsDdbItem;


    //////////////
    // Helpers
    //////////////

    /// <summary>
    /// Dumps an userItem from a DynamoDB table to an output stream.
    /// </summary>
    /// <param name="userItem">The userItem to dump.</param>
    /// <param name="out">The output stream.</param>
    static void DumpItem(const AwsDdbItem &item, std::ostream &out)
    {
        out << "===============================\n";

        for (auto &attr : item)
            out << attr.first << ": " << attr.second.SerializeAttribute();

        out << "===============================\n";
    }


    /// <summary>
    /// Dumps a list of items from a DynamoDB table to an output stream.
    /// </summary>
    /// <param name="items">The list of items.</param>
    /// <param name="out">The output stream.</param>
    static void DumpItems(const Aws::Vector<AwsDdbItem> &items, std::ostream &out)
    {
        for (size_t idx = 0; idx < items.size(); ++idx)
        {
            auto &item = items[idx];

            out << "===============================\n";
            out << "(item #" << idx << ")\n";

            for (auto &attr : item)
                out << attr.first << ": " << attr.second.SerializeAttribute();
        }

        out << "===============================\n";
    }


    /// <summary>
    /// Makes a binary time-based sort key composed of (time) + (hash of user ID).
    /// </summary>
    /// <param name="epochTime">The time (seconds since epoch).</param>
    /// <param name="userId">The user ID. Its hash is appended to the
    /// end of the sort key, unless the ID is an empty string.</param>
    /// <returns>A buffer with the generated sort key.</returns>
    static Aws::Utils::ByteBuffer MakeBinTimeBasedSortKey(time_t epochTime, const string &userId = "")
    {
        Aws::Utils::ByteBuffer buffer(sizeof (time_t) + sizeof (size_t));

        auto nBitsToShift = static_cast<int> ((sizeof(time_t) - 1) * 8);

        int idx(0);
        while (nBitsToShift >= 0)
        {
            buffer[idx++] = static_cast<unsigned char> ((epochTime >> nBitsToShift) & 0xFF);
            nBitsToShift -= 8;
        }

        size_t hashOfUserId(0);

        if (!userId.empty())
        {
            std::hash<string> hash;
            hashOfUserId = hash(userId);
        }

        memcpy(buffer.GetUnderlyingData() + idx,
               &hashOfUserId,
               sizeof (size_t));

        return std::move(buffer);
    }


    /// <summary>
    /// Gets the time (prefix) from the provided sort key.
    /// </summary>
    /// <param name="key">The sort key whose time prefix will be extracted.</param>
    /// <returns>The time (from key prefix) as seconds since epoch.</returns>
    static time_t GetTimeFromSortKey(const Aws::Utils::ByteBuffer &key)
    {
        time_t value(0);
        
        for (int idx = 0; idx < sizeof (time_t); ++idx)
        {
            auto byte = key[idx];
            value <<= 8;
            value += byte;
        }

        return value;
    }


    ////////////////////
    // Class DDBAccess
    ////////////////////

    std::unique_ptr<DDBAccess> DDBAccess::singleton;

    std::atomic<DDBAccess *> DDBAccess::singletonAtomicPtr;

    std::mutex DDBAccess::singletonCreationMutex;


    /// <summary>
    /// Initializes a new instance of the <see cref="DDBAccess"/> class.
    /// </summary>
    DDBAccess::DDBAccess()
    {
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="DDBAccess"/> class.
    /// </summary>
    /// <returns></returns>
    DDBAccess::~DDBAccess()
    {
        try
        {
            std::lock_guard<std::mutex> lock(singletonCreationMutex);

            singletonAtomicPtr.store(nullptr, std::memory_order_relaxed);
            singleton.reset();
        }
        catch (std::system_error &ex)
        {
            std::cerr << "\nERROR - News feed client - System error when finalizing cached data access: "
                      << StdLibExt::GetDetailsFromSystemError(ex);
        }
        catch (std::exception &ex)
        {
            std::cerr << "\nERROR - News feed client - Generic error when finalizing cached data access: " << ex.what();
        }
    }


    /// <summary>
    /// Gets the singleton.
    /// </summary>
    /// <returns>A reference to the singleton</returns>
    DDBAccess & DDBAccess::GetInstance()
    {
        try
        {
            auto *ptr = singletonAtomicPtr.load(std::memory_order_relaxed);

            if (ptr != nullptr)
                return *ptr;

            std::lock_guard<std::mutex> lock(singletonCreationMutex);

            if (static_cast<DDBAccess *> (singletonAtomicPtr) == nullptr)
            {
                singleton.reset(new DDBAccess());
                singletonAtomicPtr.store(singleton.get(), std::memory_order_relaxed);
            }

            return *singleton;
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when initializing data access: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic error when initializing data access: " << ex.what();
            throw AppException(oss.str());
        }
    }


    /// <summary>
    /// Gets an item from a DynamoDB table.
    /// </summary>
    /// <param name="actionLabel">The action label (to be used for error/trace report).</param>
    /// <param name="conn">The database connection.</param>
    /// <param name="request">The request to issue.</param>
    /// <param name="item">Will receive the returned item.</param>
    /// <returns>Whether an userItem has been found.</returns>
    static bool GetItem(const char *actionLabel,
                        DbConnection *conn,
                        const GetItemRequest &request,
                        AwsDdbItem &item)
    {
#   ifndef NDEBUG
        std::clog << "DynamoDB - GET REQUEST: " << request.SerializePayload() << std::endl;
#   endif
        item.clear();

        GetItemOutcome outcome;

        static const auto maxRetry = Configuration::Get().settings.dbReqMaxRetryCount;

        for (uint32_t retryCount = 0; retryCount < maxRetry; ++retryCount)
        {
            outcome = conn->GetItem(request);
            
            // error?
            if (!outcome.IsSuccess())
            {
                if (!outcome.GetError().ShouldRetry())
                    break;

                static const std::chrono::milliseconds retryInterval(
                    Configuration::Get().settings.dbReqRetryIntervalMs
                );

                std::this_thread::sleep_for(retryInterval);
                continue;
            }

            item = outcome.GetResult().GetItem();
            
            // item not found:
            if (item.empty())
            {
#   ifndef NDEBUG
                std::clog << "DynamoDB - GET RESULT: (NOT FOUND)\n" << std::endl;
#   endif
                return false;
            }

            // item found:
#   ifndef NDEBUG
            std::clog << "DynamoDB - GET RESULT:\n";
            DumpItem(item, std::clog);
            std::clog << std::endl;
#   endif
            return true;
        }

        std::ostringstream oss;
        oss << "Failed to " << actionLabel;
        throw AppException(oss.str(), outcome.GetError().GetMessage());
    }


    /// <summary>
    /// Puts (or replaces) an item into a DynamoDB table.
    /// </summary>
    /// <param name="actionLabel">The action label (to be used for error/trace report).</param>
    /// <param name="conn">The database connection.</param>
    /// <param name="request">The request to issue.</param>
    /// <returns>Whether the create/replace operation was successfull with all conditions satisfied.</returns>
    static bool PutItem(const char *actionLabel,
                        DbConnection *conn,
                        PutItemRequest &request)
    {
#   ifndef NDEBUG
        std::clog << "DynamoDB - PUT: " << request.SerializePayload() << std::endl;
#   endif
        PutItemOutcome outcome;

        static const auto maxRetry = Configuration::Get().settings.dbReqMaxRetryCount;

        for (uint32_t retryCount = 0; retryCount < maxRetry; ++retryCount)
        {
            outcome = conn->PutItem(request);

            if (outcome.IsSuccess())
                return true;
            else if (outcome.GetError().GetErrorType() == Aws::DynamoDB::DynamoDBErrors::CONDITIONAL_CHECK_FAILED)
                return false;

            if (!outcome.GetError().ShouldRetry())
                break;

            static const std::chrono::milliseconds retryInterval(
                Configuration::Get().settings.dbReqRetryIntervalMs
            );

            std::this_thread::sleep_for(retryInterval);
        }

        std::ostringstream oss;
        oss << "Failed to " << actionLabel;
        throw AppException(oss.str(), outcome.GetError().GetMessage());
    }


    /// <summary>
    /// Updates an item into a DynamoDB table.
    /// </summary>
    /// <param name="actionLabel">The action label (to be used for error/trace report).</param>
    /// <param name="conn">The database connection.</param>
    /// <param name="request">The request to issue.</param>
    /// <param name="oldItem">When not null, this item will
    /// be set with the updated attributes (before change).</param>
    /// <returns>Whether the update/create operation was successfull with all conditions satisfied.</returns>
    static bool UpdateItem(const char *actionLabel,
                           DbConnection *conn,
                           UpdateItemRequest &request,
                           AwsDdbItem *oldItem)
    {
#   ifndef NDEBUG
        std::clog << "DynamoDB - UPDATE: " << request.SerializePayload() << std::endl;
#   endif
        if (oldItem != nullptr)
            oldItem->clear();
        
        UpdateItemOutcome outcome;

        static const auto maxRetry = Configuration::Get().settings.dbReqMaxRetryCount;

        for (uint32_t retryCount = 0; retryCount < maxRetry; ++retryCount)
        {
            outcome = conn->UpdateItem(request);

            if (outcome.IsSuccess())
            {
                if (oldItem != nullptr)
                    *oldItem = outcome.GetResult().GetAttributes();

                return true;
            }
            else if (outcome.GetError().GetErrorType() == Aws::DynamoDB::DynamoDBErrors::CONDITIONAL_CHECK_FAILED)
            {
                return false;
            }

            if (!outcome.GetError().ShouldRetry())
                break;

            static const std::chrono::milliseconds retryInterval(
                Configuration::Get().settings.dbReqRetryIntervalMs
            );

            std::this_thread::sleep_for(retryInterval);
        }

        std::ostringstream oss;
        oss << "Failed to " << actionLabel;
        throw AppException(oss.str(), outcome.GetError().GetMessage());
    }


    /// <summary>
    /// Executes a batch of write operations in a DynamoDB table.
    /// </summary>
    /// <param name="actionLabel">The action label (to be used for error/trace report).</param>
    /// <param name="conn">The table to write into.</param>
    /// <param name="conn">The database connection.</param>
    /// <param name="requests">The requests to issue.</param>
    static void WriteItems(const char *actionLabel,
                           const char *table,
                           DbConnection *conn,
                           const Aws::Vector<WriteRequest> &requests)
    {
        BatchWriteItemOutcome outcome;

        uint32_t failCount(0);

        const int maxNumReqsPerBatch(25);

        size_t idxBegin(0);

        while (idxBegin < requests.size())
        {
            auto idxEnd = std::min(requests.size(), idxBegin + maxNumReqsPerBatch);

            BatchWriteItemRequest batchRequest;
            batchRequest.AddRequestItems(table,
                Aws::Vector<WriteRequest>(requests.begin() + idxBegin,
                                          requests.begin() + idxEnd)
            );

#   ifndef NDEBUG
            std::clog << "DynamoDB - BATCH WRITE: " << batchRequest.SerializePayload() << std::endl;
#   endif
            static const auto maxRetry = Configuration::Get().settings.dbReqMaxRetryCount;

            for (uint32_t retryCount = 0; retryCount < maxRetry; ++retryCount)
            {
                outcome = conn->BatchWriteItem(batchRequest);

                if (outcome.IsSuccess() || !outcome.GetError().ShouldRetry())
                    break;

                static const std::chrono::milliseconds retryInterval(
                    Configuration::Get().settings.dbReqRetryIntervalMs
                );

                std::this_thread::sleep_for(retryInterval);
            }

            // error?
            if (!outcome.IsSuccess())
                failCount += outcome.GetResult().GetUnprocessedItems().size();

            idxBegin = idxEnd;
            
        }// end of outer loop

        // any failure?
        if (failCount > 0)
        {
            std::ostringstream oss;
            oss << "Failed to " << actionLabel << " ("
                << failCount << " items left unprocessed out of "
                << requests.size() << " in total)";

            throw AppException(oss.str(), outcome.GetError().GetMessage());
        }
    }


    /// <summary>
    /// Query items from a DynamoDB table.
    /// </summary>
    /// <param name="actionLabel">The action label (to be used for error/trace report).</param>
    /// <param name="conn">The database connection.</param>
    /// <param name="request">The request to issue.</param>
    /// <param name="items">Will receive the returned items.</param>
    static void QueryItems(const char *actionLabel,
                           DbConnection *conn,
                           const QueryRequest &request,
                           Aws::Vector<AwsDdbItem> &items)
    {
#   ifndef NDEBUG
        std::clog << "DynamoDB - QUERY REQUEST: " << request.SerializePayload() << std::endl;
#   endif
        items.clear();

        QueryOutcome outcome;

        static const auto maxRetry = Configuration::Get().settings.dbReqMaxRetryCount;

        for (uint32_t retryCount = 0; retryCount < maxRetry; ++retryCount)
        {
            outcome = conn->Query(request);

            // error?
            if (!outcome.IsSuccess())
            {
                if (outcome.GetError().ShouldRetry())
                {
                    static const std::chrono::milliseconds retryInterval(
                        Configuration::Get().settings.dbReqRetryIntervalMs
                    );

                    std::this_thread::sleep_for(retryInterval);
                    continue;
                }
                else
                    break;
            }

            items = outcome.GetResult().GetItems();

            // item not found:
            if (items.empty())
            {
#   ifndef NDEBUG
                std::clog << "DynamoDB - QUERY RESULT: (NOT FOUND)\n" << std::endl;
#   endif
                return;
            }

            // item found:
#   ifndef NDEBUG
            std::clog << "DynamoDB - QUERY RESULT:\n";
            DumpItems(items, std::clog);
            std::clog << std::endl;
#   endif
            return;
        }

        std::ostringstream oss;
        oss << "Failed to " << actionLabel;
        throw AppException(oss.str(), outcome.GetError().GetMessage());
    }


    /// <summary>
    /// Gets user data or, if not there, put it.
    /// </summary>
    /// <param name="userId">The user ID.</param>
    /// <param name="currentTopic">The topic to which the user is currently subscribing.</param>
    void DDBAccess::GetOrPutUser(const string &userId, string &currentTopic)
    {
        currentTopic.clear();

        AwsDdbItem item;

        GetItemRequest getRequest;
        getRequest
            .WithTableName(DDB_TABNAME_TOPIC_BY_USER)
            .AddKey(DDB_TABATTR_TBU_PK_USER, AttributeValue(userId))
            .AddAttributesToGet(DDB_TABATTR_TBU_TOPIC);

        auto conn = m_dbConnPool.Get();

        bool found = GetItem("get user from database table "
                             DDB_TABNAME_TOPIC_BY_USER,
                             conn.Get(),
                             getRequest,
                             item);
        if (found)
        {
            auto iter = item.find(DDB_TABATTR_TBU_TOPIC);

            if (iter == item.end())
            {
                std::ostringstream oss;
                oss << "Could not find attribute " DDB_TABATTR_TBU_TOPIC
                       " in item retrieved from table " DDB_TABNAME_TOPIC_BY_USER
                       " for user '" << userId << '\'';

                throw AppException("Cannot recognize schema of user data item!", oss.str());
            }

            currentTopic = iter->second.GetS();
            return;
        }

        PutItemRequest putRequest;
        putRequest
            .WithTableName(DDB_TABNAME_TOPIC_BY_USER)
            .WithConditionExpression("attribute_not_exists(" DDB_TABATTR_TBU_PK_USER ")") // do insert, not replace
            .AddItem(DDB_TABATTR_TBU_PK_USER, AttributeValue(userId))
            .AddItem(DDB_TABATTR_TBU_TOPIC, AttributeValue().SetNull(true))
            .AddItem(DDB_TABATTR_TBU_LFTIME, AttributeValue().SetNull(true));

        bool putDone = PutItem("put new user into database table "
                               DDB_TABNAME_TOPIC_BY_USER,
                               conn.Get(),
                               putRequest);

        if (!putDone)
        {
            throw AppException("Failed to create new user on table " DDB_TABNAME_TOPIC_BY_USER,
                               "Record with same key already existed");
        }
    }


    /// <summary>
    /// Updates the user.
    /// </summary>
    /// <param name="userId">The user identifier.</param>
    /// <param name="topic">The topic.</param>
    void DDBAccess::UpdateUser(const string &userId, const string &topic)
    {
        char strEpochTime[21];
        snprintf(strEpochTime, sizeof strEpochTime, "%ld", time(nullptr));

        UpdateItemRequest updateRequest;
        updateRequest
            .WithTableName(DDB_TABNAME_TOPIC_BY_USER)
            .AddKey(DDB_TABATTR_TBU_PK_USER, AttributeValue(userId))
            .AddAttributeUpdates(DDB_TABATTR_TBU_TOPIC,
                AttributeValueUpdate()
                    .WithAction(AttributeAction::PUT)
                    .WithValue(!topic.empty() ? AttributeValue(topic) : AttributeValue().SetNull(true))
            )
            .AddAttributeUpdates(DDB_TABATTR_TBU_LFTIME,
                AttributeValueUpdate()
                    .WithAction(AttributeAction::PUT)
                    .WithValue(AttributeValue().SetN(strEpochTime))
            )
            .WithReturnValues(ReturnValue::UPDATED_OLD);

        AwsDdbItem oldUpdAttrs;

        auto conn = m_dbConnPool.Get();

        bool updateDone = UpdateItem("update user data in table "
                                     DDB_TABNAME_TOPIC_BY_USER,
                                     conn.Get(),
                                     updateRequest,
                                     &oldUpdAttrs);
        
        /* no unsubscription has been carried out?
           then we are done here: */
        if (!updateDone || !topic.empty())
            return;

        ///////////////////////////////////////////////////////////////
        // Upon unsubscription, delete the news old enough to purge:

        static const auto oldNewsPurgeAgeSecs = Configuration::Get().settings.dbOldNewsPurgeAgeSecs;

        auto prevTopic = oldUpdAttrs[DDB_TABATTR_TBU_TOPIC].GetS();

        QueryRequest queryRequest;
        queryRequest
            .WithTableName(DDB_TABNAME_NEWS_BY_TOPIC)
            .WithKeyConditionExpression(
                DDB_TABATTR_NBT_PK_TOPIC " = :topic AND "
                DDB_TABATTR_NBT_SK_BINTB " < :bintbsk"
            )
            .AddExpressionAttributeValues(":topic", AttributeValue(prevTopic))
            .AddExpressionAttributeValues(":bintbsk",
                AttributeValue().SetB(
                    MakeBinTimeBasedSortKey(time(nullptr) - oldNewsPurgeAgeSecs)
                )
            );

        Aws::Vector<AwsDdbItem> newsItems;

        QueryItems("get expired news from database table",
                   conn.Get(),
                   queryRequest,
                   newsItems);

        if (newsItems.empty())
            return;

        Aws::Vector<WriteRequest> writeRequests(newsItems.size());

        for (int idx = 0; idx < newsItems.size(); ++idx)
        {
            writeRequests[idx].WithDeleteRequest(
                DeleteRequest()
                    .AddKey(DDB_TABATTR_NBT_PK_TOPIC, AttributeValue(prevTopic))
                    .AddKey(DDB_TABATTR_NBT_SK_BINTB, newsItems[idx][DDB_TABATTR_NBT_SK_BINTB])
            );
        }

        WriteItems("purge expired news from database",
                   DDB_TABNAME_NEWS_BY_TOPIC,
                   conn.Get(),
                   writeRequests);
    }


    /// <summary>
    /// Puts news in a given topic.
    /// </summary>
    /// <param name="topic">The topic.</param>
    /// <param name="news">The news.</param>
    void DDBAccess::PutNews(const string &topic,
                            const string &userId,
                            const string &news)
    {
        PutItemRequest putRequest;
        putRequest
            .WithTableName(DDB_TABNAME_NEWS_BY_TOPIC)
            .WithConditionExpression("attribute_not_exists(" DDB_TABATTR_NBT_PK_TOPIC ")") // do insert, not replace
            .AddItem(DDB_TABATTR_NBT_PK_TOPIC, AttributeValue(topic))
            .AddItem(DDB_TABATTR_NBT_SK_BINTB,
                AttributeValue().SetB(
                    MakeBinTimeBasedSortKey(time(nullptr), userId)
                )
            )
            .AddItem(DDB_TABATTR_NBT_NEWS, AttributeValue(news));

        auto conn = m_dbConnPool.Get();

        bool putDone = PutItem("put news in database table "
                               DDB_TABATTR_NBT_NEWS,
                               conn.Get(),
                               putRequest);

        if (!putDone)
        {
            throw AppException("Failed to create new user on table " DDB_TABNAME_NEWS_BY_TOPIC,
                               "Record with same key already existed");
        }
    }


    /// <summary>
    /// Gets the news in a given topic.
    /// </summary>
    /// <param name="userId">The ID of the user requesting the news.</param>
    /// <param name="news">All the news found since last feed.</param>
    void DDBAccess::GetNews(const string &userId, std::vector<string> &news)
    {
        news.clear();

        ///////////////////
        // Get user info:

        AwsDdbItem userItem;

        GetItemRequest getRequest;
        getRequest
            .WithTableName(DDB_TABNAME_TOPIC_BY_USER)
            .AddKey(DDB_TABATTR_TBU_PK_USER, AttributeValue(userId))
            .AddAttributesToGet(DDB_TABATTR_TBU_TOPIC)
            .AddAttributesToGet(DDB_TABATTR_TBU_LFTIME);

        auto conn = m_dbConnPool.Get();

        bool found = GetItem("get user from database table "
                             DDB_TABNAME_TOPIC_BY_USER,
                             conn.Get(),
                             getRequest,
                             userItem);
        if (!found)
        {
            std::ostringstream oss;
            oss << "User '" << userId << "' not found in database table " DDB_TABNAME_TOPIC_BY_USER << '!';
            throw AppException("Could not retrieve news for user topic!", oss.str());
        }

        auto iter = userItem.find(DDB_TABATTR_TBU_TOPIC);

        if (iter == userItem.end())
        {
            std::ostringstream oss;
            oss << "Could not find attribute " DDB_TABATTR_TBU_TOPIC
                   " in item retrieved from table " DDB_TABNAME_TOPIC_BY_USER
                   " for user '" << userId << '\'';

            throw AppException("Cannot recognize schema of user data item!", oss.str());
        }

        string topic = iter->second.GetS();

        if (topic.empty())
            return;

        iter = userItem.find(DDB_TABATTR_TBU_LFTIME);

        if (iter == userItem.end())
        {
            std::ostringstream oss;
            oss << "Could not find attribute " DDB_TABATTR_TBU_LFTIME
                   " in item retrieved from table " DDB_TABNAME_TOPIC_BY_USER
                   " for user '" << userId << '\'';

            throw AppException("Cannot recognize schema of user data item!", oss.str());
        }

        time_t lastFeedTime;

        if (!iter->second.GetN().empty())
            lastFeedTime = strtoll(iter->second.GetN().c_str(), nullptr, 10);
        else
            lastFeedTime = std::numeric_limits<time_t>::min();

        //////////////////
        // Get the news:

        QueryRequest queryRequest;
        queryRequest
            .WithTableName(DDB_TABNAME_NEWS_BY_TOPIC)
            .WithKeyConditionExpression(
                DDB_TABATTR_NBT_PK_TOPIC " = :topic AND "
                DDB_TABATTR_NBT_SK_BINTB " >= :bintbsk"
            )
            .AddExpressionAttributeValues(":topic", AttributeValue(topic))
            .AddExpressionAttributeValues(":bintbsk",
                AttributeValue().SetB(
                    MakeBinTimeBasedSortKey(lastFeedTime + 1)
                )
            );

        Aws::Vector<AwsDdbItem> newsItems;

        QueryItems("get news from database table",
                   conn.Get(),
                   queryRequest,
                   newsItems);

        if (newsItems.empty())
            return;

        news.reserve(newsItems.size());

        for (auto &entry : newsItems)
        {
            iter = entry.find(DDB_TABATTR_NBT_SK_BINTB);

            if (iter == userItem.end())
            {
                std::ostringstream oss;
                oss << "Could not find attribute " DDB_TABATTR_NBT_SK_BINTB
                       " in item retrieved from table " DDB_TABNAME_NEWS_BY_TOPIC
                       " for topic '" << topic << '\'';

                throw AppException("Cannot recognize schema of news item!", oss.str());
            }

            time_t lftime = GetTimeFromSortKey(iter->second.GetB());

            // get the latest feed time:
            if (lftime >= lastFeedTime)
                lastFeedTime = lftime;

            iter = entry.find(DDB_TABATTR_NBT_NEWS);

            if (iter == userItem.end())
            {
                std::ostringstream oss;
                oss << "Could not find attribute " DDB_TABATTR_NBT_NEWS
                       " in item retrieved from table " DDB_TABNAME_NEWS_BY_TOPIC
                       " for topic '" << topic << '\'';

                throw AppException("Cannot recognize schema of news item!", oss.str());
            }

            news.push_back(iter->second.GetS());
        }

        ///////////////////////////
        // Update last feed time:
        
        char strLFTime[21];
        snprintf(strLFTime, sizeof strLFTime, "%ld", lastFeedTime);

        UpdateItemRequest updateRequest;
        updateRequest
            .WithTableName(DDB_TABNAME_TOPIC_BY_USER)
            .AddKey(DDB_TABATTR_TBU_PK_USER, AttributeValue(userId))
            .WithConditionExpression(DDB_TABATTR_TBU_TOPIC " = :topic")
            .AddExpressionAttributeValues(":topic", AttributeValue(topic))
            .WithUpdateExpression("SET " DDB_TABATTR_TBU_LFTIME " = :lftime")
            .AddExpressionAttributeValues(":lftime", AttributeValue().SetN(strLFTime));

        bool updateDone = UpdateItem("update user data in table "
                                     DDB_TABNAME_TOPIC_BY_USER,
                                     conn.Get(),
                                     updateRequest,
                                     nullptr);
        if (!updateDone)
        {
            std::clog << "WARNING - UPDATE operation on database was expected to update 'last feed time' of user '"
                         DDB_TABATTR_TBU_PK_USER "' on table " DDB_TABNAME_TOPIC_BY_USER
                         ", but the record was found with an unexpected topic!" << std::endl;
        }
    }

}// end of namespace newsfeed

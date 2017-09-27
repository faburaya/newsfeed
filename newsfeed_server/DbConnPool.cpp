#include "DbConnPool.h"
#include "configuration.h"
#include "common.h"
#include <cmath>
#include <aws/core/auth/AWSCredentialsProvider.h>

namespace newsfeed
{
    /// <summary>
    /// Initializes a new instance of the <see cref="DbConnPool"/> class.
    /// </summary>
    DbConnPool::DbConnPool()
        : m_connections(256)
        , m_poolCtionTime(time(nullptr))
        , m_lastAddTime(0)
        , m_totalConnsCount(0)
        , m_activeConnsCount(0)
        , m_twAvgNumActConns(0)
    {}


    /// <summary>
    /// Finalizes an instance of the <see cref="DbConnPool"/> class.
    /// </summary>
    DbConnPool::~DbConnPool()
    {
        m_connections.consume_all([](DbConnection *conn) { Aws::Delete<DbConnection>(conn); });
    }


    /// <summary>
    /// Increments the stats after delivering a connection.
    /// </summary>
    /// <param name="isNew">Whether a new connection will be created.</param>
    void DbConnPool::IncrementStats(bool isNew)
    {
        time_t now = time(nullptr);

        // acquire semantics
        auto activeConnsCount = m_activeConnsCount.fetch_add(1, std::memory_order_acquire);

        // update time-weighted average number of active connections:
        m_twAvgNumActConns = static_cast<float> (
            m_twAvgNumActConns
            * (m_lastAddTime - m_poolCtionTime)
            + activeConnsCount
            * (now - m_lastAddTime)
        ) / (now - m_poolCtionTime);

        m_lastAddTime = now;

        // release semantics
        m_totalConnsCount.fetch_add(1, std::memory_order_release);
    }
    

    /// <summary>
    /// Gets a connection from the pool.
    /// </summary>
    /// <returns>A database connection.</returns>
    DbConnPool::ConnWrapper DbConnPool::Get()
    {
        DbConnection *conn;
        
        if (m_connections.pop(conn))
        {
            IncrementStats(false);
            return ConnWrapper(*this, conn);
        }

        IncrementStats(true);

        Aws::Client::ClientConfiguration config;
        config.region = Configuration::Get().settings.awsRegion;

        conn = Aws::New<DbConnection>("ALLOC_TAG",
            Aws::DynamoDB::DynamoDBClient(
                Aws::Auth::AWSCredentials(
                    Configuration::Get().settings.awsAccessKeyId,
                    Configuration::Get().settings.awsSecretKey
                ),
                config
            )
        );

        return ConnWrapper(*this, conn);
    }


    /// <summary>
    /// Decrements the stats after receiving back a connection.
    /// </summary>
    /// <param name="discard">Set to <c>true</c> whenever
    /// the update on stats indicates the returned connection
    /// should be deleted rather than kept on the pool.</param>
    void DbConnPool::DecrementStats(bool &discard)
    {
        time_t now = time(nullptr);

        auto activeConnsCount = m_activeConnsCount.fetch_sub(1, std::memory_order_acquire);

        // update time-weighted average number of active connections:
        m_twAvgNumActConns = static_cast<float> (
            m_twAvgNumActConns
            * (m_lastAddTime - m_poolCtionTime)
            + activeConnsCount
            * (now - m_lastAddTime)
        ) / (now - m_poolCtionTime);

        m_lastAddTime = now;

        if (m_totalConnsCount > static_cast<uint32_t> (1.2F * m_twAvgNumActConns + 0.5))
        {
            discard = true;
            m_totalConnsCount.fetch_sub(1, std::memory_order_release); // release semantics
        }
        else
        {
            discard = false;
            atomic_thread_fence(std::memory_order_release); // release semantics
        }
    }


    /// <summary>
    /// Returns a connection to the pool.
    /// </summary>
    /// <param name="conn">The database connection.</param>
    void DbConnPool::Return(DbConnection *conn)
    {
        assert(conn != nullptr);

        bool discard;
        DecrementStats(discard);

        if (discard)
        {
            Aws::Delete(conn);
            return;
        }

        if (!m_connections.push(conn))
            throw AppException("Failed to return database connection back to the pool!");
    }

}// end of namespace newsfeed

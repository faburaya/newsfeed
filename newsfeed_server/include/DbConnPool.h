#ifndef DBCONNPOOL_H // header guard
#define DBCONNPOOL_H

#include <ctime>
#include <cinttypes>
#include <atomic>
#include <boost/lockfree/stack.hpp>
#include <aws/dynamodb/DynamoDBClient.h>

namespace newsfeed
{
    typedef Aws::DynamoDB::DynamoDBClient DbConnection;

    /// <summary>
    /// Pool of connections for AWS DynamoDB.
    /// This implementation is thread safe and (mostly) lock-free.
    /// </summary>
    class DbConnPool
    {
    private:

        boost::lockfree::stack<DbConnection *> m_connections;

        const time_t m_poolCtionTime;

        time_t m_lastAddTime;
        float m_twAvgNumActConns;

        std::atomic<uint32_t> m_totalConnsCount;
        std::atomic<uint32_t> m_activeConnsCount;

        void IncrementStats(bool isNew);
        void DecrementStats(bool &discard);

    public:

        /// <summary>
        /// Wraps a connection for automatic return to the pool upon end of scope.
        /// </summary>
        class ConnWrapper
        {
        private:

            DbConnPool &m_pool;
            DbConnection *m_connection;

        public:

            ConnWrapper(DbConnPool &pool, DbConnection *conn)
                : m_pool(pool), m_connection(conn) {}

            ConnWrapper(const ConnWrapper &ob) = delete;

            ConnWrapper(ConnWrapper &&ob)
                : m_pool(ob.m_pool)
                , m_connection(ob.m_connection)
            {
                ob.m_connection = nullptr;
            }

            ~ConnWrapper()
            {
                if (m_connection != nullptr)
                    m_pool.Return(m_connection);
            }

            DbConnection *Get() { return m_connection; }
        };

        DbConnPool();

        ~DbConnPool();

        ConnWrapper Get();

        void Return(DbConnection *conn);
    };

}// end of namespace newsfeed

#endif // end of header guard

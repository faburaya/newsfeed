#ifndef COMMON_H // header guard
#define COMMON_H

#include <exception>
#include <future>
#include <string>

#define STATUS_OKAY true
#define STATUS_FAIL false

namespace newsfeed
{
    using std::string;


    /// <summary>
    /// Aggregates extension functions that work on objects of the C++ Standard Library.
    /// </summary>
    class StdLibExt
    {
    public:

        static string GetDetailsFromSystemError(const std::system_error &ex);

        static string GetDetailsFromFutureError(const std::future_error &ex);
    };


    /// <summary>
    /// Exception for any runtime error issued by client or server code.
    /// </summary>
    /// <seealso cref="std::runtime_error" />
    class AppException : public std::runtime_error
    {
    private:

        string m_what;
        string m_details;

#   ifndef NDEBUG
        string m_stackTrace;
#   endif

    public:

        AppException(const string &what);

        AppException(const string &what, const string &details);

        const char *GetDetails() const { return m_details.c_str();  }

#   ifndef NDEBUG
        const char *GetStackTrace() const { return m_stackTrace.c_str();  }
#   endif

        string ToString() const;
    };

}// end of namespace newsfeed

#endif // end of header guard

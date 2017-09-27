#include "common.h"
#include <sstream>
#include <future>
#include <array>
#include <execinfo.h>

//#define PRINT_EX_BACK_TRACE

namespace newsfeed
{
    /// <summary>
    /// Gets the details from a system error.
    /// </summary>
    /// <param name="ex">The system error.</param>
    /// <returns>A string with a detailed description of the given system error.</returns>
    string StdLibExt::GetDetailsFromSystemError(const std::system_error &ex)
    {
        std::ostringstream oss;
        oss << ex.code().category().name() << " / " << ex.code().message();
        return oss.str();
    }

    /// <summary>
    /// Gets the details from a future error.
    /// </summary>
    /// <param name="ex">The future error.</param>
    /// <returns>A string with a detailed description of the given future error.</returns>
    string StdLibExt::GetDetailsFromFutureError(const std::future_error &ex)
    {
        std::ostringstream oss;
        oss << ex.code().category().name() << " / " << ex.code().message();
        return oss.str();
    }
    

    ///////////////////////////////
    // AppException Class
    ///////////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="AppException"/> class.
    /// </summary>
    /// <param name="what">The main error message.</param>
    AppException::AppException(const string &what)
        : AppException(what, "") {}


    /// <summary>
    /// Initializes a new instance of the <see cref="AppException"/> class.
    /// </summary>
    /// <param name="what">The main error message.</param>
    /// <param name="details">The error details.</param>
    AppException::AppException(const string &what, const string &details)
        : std::runtime_error(what)
        , m_details(details)
    {
#   ifndef NDEBUG

        std::ostringstream oss;

        oss << "### CALL STACK TRACE ###\n\n";

        std::array<void *, 32> buffer;

        int frameCount = backtrace(buffer.data(), buffer.size());

        if (frameCount <= 0)
        {
            oss << "secondary failure prevented retrieval of backtrace\n";
            m_stackTrace = oss.str();
            return;
        }

        char **symbols = backtrace_symbols(buffer.data(), frameCount);

        if (symbols == nullptr)
        {
            oss << "secondary failure prevented retrieval of backtrace\n";
            m_stackTrace = oss.str();
            return;
        }

        for (int idx = 0; idx < frameCount; ++idx)
        {
            oss << symbols[idx] << '\n';
        }

        m_stackTrace = oss.str();
#   endif
    }


    /// <summary>
    /// Serializes all the content in this exception to a string.
    /// </summary>
    /// <returns>A full report of the represented error.</returns>
    string AppException::ToString() const
    {
        std::ostringstream oss;

        oss << what();

        if (!m_details.empty())
            oss << " - " << m_details;

        oss << "\n\n";

#   ifdef PRINT_EX_BACK_TRACE

        if (!m_stackTrace.empty())
            oss << m_stackTrace << "\n";
#   endif

        return oss.str();
    }

}// end of namespace newsfeed

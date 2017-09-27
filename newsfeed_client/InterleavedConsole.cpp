#include "InterleavedConsole.h"
#include <ctime>
#include <thread>

std::unique_ptr<InterleavedConsole> InterleavedConsole::singleton;


/// <summary>
/// Prevents a default instance of the <see cref="InterleavedConsole"/> class from being created.
/// </summary>
InterleavedConsole::InterleavedConsole()
    : m_messageQueue(256) {}


/// <summary>
/// Gets the singleton.
/// Creation is NOT thread safe.
/// </summary>
/// <returns>A reference to the singleton</returns>
InterleavedConsole & InterleavedConsole::Get()
{
    if (singleton)
        return *singleton;
    
    singleton.reset(new InterleavedConsole());
    return *singleton;
}


/// <summary>
/// Gets the user typed content from prompt.
/// This call locks access to console IO and blocks the caller.
/// </summary>
/// <returns>The typed content when the user hits enter.</returns>
string InterleavedConsole::GetLine()
{
    std::cout << "\n? ";

    string line;
    std::getline(std::cin, line, '\n');

    return std::move(line);
}


/// <summary>
/// Assembles a message with the given format and
/// locks the console to print it.
/// </summary>
/// <param name="format">The format string. The implementation already
/// takes care of skipping lines in order not to concatenate messages.</param>
/// <param name="...">Varying list of arguments</param>
void InterleavedConsole::PrintLine(const char *format, ...)
{
    char buffer[2048];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);

    fprintf(stdout, "* %s\n", buffer);
}


/// <summary>
/// Assembles a message with the given format and
/// enqueues it to be print in the console.
/// </summary>
/// <param name="format">The format string. The implementation already
/// takes care of skipping lines in order not to concatenate messages.</param>
/// <param name="...">Varying list of arguments</param>
void InterleavedConsole::EnqueueLine(const char *format, ...)
{
    char buffer[2048];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);

    m_messageQueue.push(new string(buffer));
}


/// <summary>
/// Flushes the queue of messages to the console.
/// </summary>
void InterleavedConsole::FlushQueue()
{
    m_messageQueue.consume_all([](const string *msg)
    {
        printf("* %s\n", msg->c_str());
        delete msg;
    });
}


/// <summary>
/// Keeps the queue open and flushing directly
/// to console for a given amount of time.
/// </summary>
/// <param name="seconds">The interval to flush in seconds.</param>
void InterleavedConsole::FlushQueueFor(uint16_t seconds)
{
    time_t start = time(nullptr);

    do
    {
        m_messageQueue.consume_all([](const string *msg)
        {
            printf("* %s\n", msg->c_str());
            delete msg;
        });

        std::this_thread::sleep_for(
            std::chrono::milliseconds(250)
        );

    } while (time(nullptr) - start < seconds);
}
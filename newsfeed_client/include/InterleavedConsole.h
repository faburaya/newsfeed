#ifndef INTERLEAVEDCONSOLE_H // header guard
#define INTERLEAVEDCONSOLE_H

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <memory>
#include <cinttypes>

#include <boost/lockfree/queue.hpp>

using std::string;


/// <summary>
/// A wrapper for interleaved IO access to console.
/// </summary>
class InterleavedConsole
{
private:

    boost::lockfree::queue<string *> m_messageQueue;

    static std::unique_ptr<InterleavedConsole> singleton;

    InterleavedConsole();

public:

    static InterleavedConsole &Get();

    string GetLine();

    void PrintLine(const char *format, ...);

    void EnqueueLine(const char *format, ...);

    void FlushQueue();

    void FlushQueueFor(uint16_t seconds);
};



#endif // end of header guard

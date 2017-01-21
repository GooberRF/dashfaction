#pragma once

#include <log/BaseAppender.h>
#include <fstream>

namespace logging
{
    class FileAppender : public BaseAppender
    {
    public:
        FileAppender(const std::string &filename, bool append = true, bool flush = true) :
            m_filename(filename), m_append(append), m_flush(flush) {}

        virtual void append(LogLevel lvl, const std::string &str);

    private:
        std::string m_filename;
        std::ofstream m_fileStream;
        bool m_append, m_flush;
    };
}
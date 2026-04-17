// logger.h – rotating file logger (Meyers singleton, C++17)
#pragma once

#include <cstdio>
#include <string>
#include "base.hpp"
#include "jthread.hpp"

namespace pdk {

#define LOG_ROOT_PATH "../bin/log/"

class Logger : public Thread {
public:
    enum LOGLEVEL { Critical = 0, Error, Warn, Info, Boardapi, Api, Event, MaxLevel };

    // Meyers singleton
    static Logger* get();

    int log(const char* pFormat, ...);
    int log(LOGLEVEL l, const char* pFormat, ...);
    int log(const char* file, int line, LOGLEVEL l, const char* pFormat, ...);

    bool create_file(const char* pPrefix, int nQuata);
    bool create_file(const char* pPrefix, const char* pPath, int nQuata = 0);

    void set_level(LOGLEVEL l) noexcept { level_ = l; }
    LOGLEVEL get_level() noexcept { return level_; }
    void set_file_line_level(LOGLEVEL l) noexcept { file_line_level_ = l; }

    ~Logger() override;

private:
    struct LogMsg {
        enum class Type { Log, Reopen, Stop } type;
        std::string text;    // Log
        std::string prefix;  // Reopen (empty = keep existing)
        std::string path;    // Reopen (empty = keep existing)
        int         quota{0};// Reopen (0 = keep existing)
    };

    Logger();
    int  make_date();
    bool check_quota();
    bool do_open();
    void push_log(std::string text);
    void* thread_proc() override;

    Queue1To1<LogMsg> queue_;
    std::string prefix_;
    std::string root_path_;
    std::string logger_path_;
    int quota_{0};
    File file_;
    int date_{0};
    LOGLEVEL level_{MaxLevel};
    LOGLEVEL file_line_level_{Warn};
    pid_t pid_{0};
};

}  // namespace pdk

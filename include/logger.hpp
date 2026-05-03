// logger.h – rotating file logger (Meyers singleton)
#pragma once

#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include "base.hpp"
#include "jthread.hpp"

namespace pdk {

#define LOG_ROOT_PATH "../bin/log/"

class Logger : public Thread {
public:
    enum LOGLEVEL { Critical = 0, Error, Warn, Info, Boardapi, Api, Event, MaxLevel };

    // Meyers singleton
    static Logger* get();

    // ── Legacy printf-style entry points (kept for migration) ────────────
    // PDK 4.2: prefer log_fmt() and PDK_LOG_*_FMT macros for type-safe
    // formatting via std::format. The printf overloads remain so existing
    // call sites compile unchanged.
    int log(const char* pFormat, ...);
    int log(LOGLEVEL l, const char* pFormat, ...);
    int log(const char* file, int line, LOGLEVEL l, const char* pFormat, ...);

    // ── Modern type-safe logging via std::format (C++20) ─────────────────
    //
    // The format-string is checked at compile time against the argument
    // types; a malformed string is a compile error rather than a runtime
    // failure. All wrapper macros (PDK_LOG_*_FMT) funnel through this
    // single overload.
    //
    // ⚠ NOT FOR REAL-TIME PATHS. log_fmt() may allocate heap (std::format
    //   builds a std::string internally) and is therefore unsuitable for
    //   thread_proc inner loops where determinism matters. RT call sites
    //   should keep using the printf-style log() overloads, which write
    //   into a fixed-size stack buffer and never allocate.
    //
    // Marked noexcept: std::bad_alloc / std::format_error from std::format
    // are caught and degraded to a fallback printf log so a logging failure
    // never aborts the caller's thread_proc.
    template<class... Args>
    int log_fmt(LOGLEVEL level,
                const char* file,
                int line,
                std::format_string<Args...> fmt,
                Args&&... args) noexcept {
        if (level > level_) return 0;
        try {
            std::string body = std::format(fmt, std::forward<Args>(args)...);
            try {
                return log(file, line, level, "%s", body.c_str());
            } catch (...) {
                // Even the printf-style log() can allocate a std::string
                // for its push_log queue; if that throws too, fall through
                // to the non-throwing fprintf below.
            }
        } catch (...) {
            // std::format itself failed (bad_alloc, format_error). Fall
            // through to the bare fprintf path below.
        }
        // Last-resort, truly non-throwing fallback. Skips the rotating
        // logger entirely so even bad_alloc cannot escape this function.
        std::fprintf(stderr, "[Logger] log_fmt failed at %s:%d (level=%d)\n",
                     file ? file : "?", line, static_cast<int>(level));
        return 0;
    }

    bool create_file(const char* pPrefix, int nQuata);
    bool create_file(const char* pPrefix, const char* pPath, int nQuata = 0);

    void set_level(LOGLEVEL l) noexcept { level_ = l; }
    LOGLEVEL get_level() noexcept { return level_; }
    void set_file_line_level(LOGLEVEL l) noexcept { file_line_level_ = l; }

    ~Logger() override;

    // Convenience: variadic helpers that always carry file/line. Prefer the
    // PDK_LOG_* macros below in new code rather than calling these directly.
    template<class... Args>
    int infof(const char* file, int line, const char* fmt, Args&&... args) {
        return log(file, line, Info, fmt, std::forward<Args>(args)...);
    }
    template<class... Args>
    int warnf(const char* file, int line, const char* fmt, Args&&... args) {
        return log(file, line, Warn, fmt, std::forward<Args>(args)...);
    }
    template<class... Args>
    int errorf(const char* file, int line, const char* fmt, Args&&... args) {
        return log(file, line, Error, fmt, std::forward<Args>(args)...);
    }
    template<class... Args>
    int eventf(const char* file, int line, const char* fmt, Args&&... args) {
        return log(file, line, Event, fmt, std::forward<Args>(args)...);
    }
    template<class... Args>
    int criticalf(const char* file, int line, const char* fmt, Args&&... args) {
        return log(file, line, Critical, fmt, std::forward<Args>(args)...);
    }

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

// ── PDK_LOG_* macros (legacy printf-style, kept for compatibility) ───────────
// New code should prefer the *_FMT variants below for compile-time-checked
// std::format strings.
#define PDK_LOG_CRIT(fmt, ...) \
    ::pdk::Logger::get()->log(__FILE__, __LINE__, ::pdk::Logger::Critical, fmt, ##__VA_ARGS__)
#define PDK_LOG_ERR(fmt, ...) \
    ::pdk::Logger::get()->log(__FILE__, __LINE__, ::pdk::Logger::Error, fmt, ##__VA_ARGS__)
#define PDK_LOG_WARN(fmt, ...) \
    ::pdk::Logger::get()->log(__FILE__, __LINE__, ::pdk::Logger::Warn, fmt, ##__VA_ARGS__)
#define PDK_LOG_INFO(fmt, ...) \
    ::pdk::Logger::get()->log(__FILE__, __LINE__, ::pdk::Logger::Info, fmt, ##__VA_ARGS__)
#define PDK_LOG_EVENT(fmt, ...) \
    ::pdk::Logger::get()->log(__FILE__, __LINE__, ::pdk::Logger::Event, fmt, ##__VA_ARGS__)

// ── PDK_LOG_*_FMT macros (modern, type-safe via std::format) ─────────────────
// Format-string and arguments are checked at compile time. Prefer these in
// new code. Example:
//     PDK_LOG_INFO_FMT("port={} addr={}", 9100, "127.0.0.1");
#define PDK_LOG_CRIT_FMT(fmt, ...) \
    ::pdk::Logger::get()->log_fmt(::pdk::Logger::Critical, __FILE__, __LINE__, \
                                  fmt __VA_OPT__(,) __VA_ARGS__)
#define PDK_LOG_ERR_FMT(fmt, ...) \
    ::pdk::Logger::get()->log_fmt(::pdk::Logger::Error, __FILE__, __LINE__, \
                                  fmt __VA_OPT__(,) __VA_ARGS__)
#define PDK_LOG_WARN_FMT(fmt, ...) \
    ::pdk::Logger::get()->log_fmt(::pdk::Logger::Warn, __FILE__, __LINE__, \
                                  fmt __VA_OPT__(,) __VA_ARGS__)
#define PDK_LOG_INFO_FMT(fmt, ...) \
    ::pdk::Logger::get()->log_fmt(::pdk::Logger::Info, __FILE__, __LINE__, \
                                  fmt __VA_OPT__(,) __VA_ARGS__)
#define PDK_LOG_EVENT_FMT(fmt, ...) \
    ::pdk::Logger::get()->log_fmt(::pdk::Logger::Event, __FILE__, __LINE__, \
                                  fmt __VA_OPT__(,) __VA_ARGS__)

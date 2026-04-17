// logger.cpp – Logger implementation
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include "logger.hpp"

namespace pdk {

// ── Singleton ─────────────────────────────────────────────────────────────────
Logger* Logger::get()
{
    static Logger instance;
    return &instance;
}

Logger::Logger() : prefix_("log"), root_path_(LOG_ROOT_PATH), date_(make_date()), pid_(getpid())
{
    Thread::create();
}

Logger::~Logger()
{
    queue_.put({ LogMsg::Type::Stop, {}, {}, {}, 0 });
    Thread::close();
}

// ── make_date ──────────────────────────────────────────────────────────────────
int Logger::make_date()
{
    Time tCur = Time::current_time_ms();
    int nDate = tCur.GetYear() * 10000 + tCur.GetMonth() * 100 + tCur.GetDay();
    return nDate;
}

// ── check_quota ────────────────────────────────────────────────────────────────
#define MAX_QUATA_INDEX 7
bool Logger::check_quota()
{
    struct stat buf;
    if (quota_ < 1)
        return false;

    if (logger_path_.empty()) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s%s.0", root_path_.c_str(), prefix_.c_str());
        logger_path_ = tmp;
    }

    if (!stat(logger_path_.c_str(), &buf)) {
        if (static_cast<double>(buf.st_size) / 1048576.0 > static_cast<double>(quota_)) {
            if (file_.fd_ != File::hFileNull)
                file_.close();

            char oldName[512], newName[512];
            for (int i = MAX_QUATA_INDEX - 1; i > 0; i--) {
                snprintf(oldName,
                         sizeof(oldName),
                         "%s%s.%d",
                         root_path_.c_str(),
                         prefix_.c_str(),
                         i - 1);
                if (stat(oldName, &buf) != 0)
                    continue;
                snprintf(
                    newName, sizeof(newName), "%s%s.%d", root_path_.c_str(), prefix_.c_str(), i);
                rename(oldName, newName);
            }
        }
    }
    return true;
}

// ── do_open – consumer-only file open (equivalent to create_file(nullptr,0)) ──
bool Logger::do_open()
{
    char tmp[512];
    if (!check_quota()) {
        snprintf(tmp, sizeof(tmp), "%s%s.%04d", root_path_.c_str(), prefix_.c_str(), date_ % 10000);
        logger_path_ = tmp;
    }

    if (file_.fd_ != File::hFileNull)
        file_.close();

    bool ok = file_.open(logger_path_.c_str(), File::modeWrite | File::modeNoTruncate);
    if (!ok)
        fprintf(stderr, "[Logger] failed to open log file: %s\n", logger_path_.c_str());
    return ok;
}

// ── create_file – enqueue Reopen message (async) ─────────────────────────────
bool Logger::create_file(const char* pPrefix, int nQuata)
{
    LogMsg msg;
    msg.type   = LogMsg::Type::Reopen;
    msg.prefix = pPrefix ? pPrefix : "";  // empty = keep existing
    msg.path   = "";                       // keep existing root_path_
    msg.quota  = nQuata;
    queue_.put(std::move(msg));
    return true;
}

bool Logger::create_file(const char* pPrefix, const char* pPath, int nQuata)
{
    LogMsg msg;
    msg.type   = LogMsg::Type::Reopen;
    msg.prefix = pPrefix ? pPrefix : "";
    msg.path   = pPath ? pPath : LOG_ROOT_PATH;
    msg.quota  = nQuata;
    queue_.put(std::move(msg));
    return true;
}

// ── push_log – enqueue formatted log string (producer) ───────────────────────
void Logger::push_log(std::string text)
{
    LogMsg msg;
    msg.type = LogMsg::Type::Log;
    msg.text = std::move(text);
    queue_.put(std::move(msg));
}

// ── thread_proc – consumer ────────────────────────────────────────────────────
void* Logger::thread_proc()
{
    while (true) {
        LogMsg msg = queue_.get();

        if (msg.type == LogMsg::Type::Stop) {
            LogMsg rem;
            while (queue_.try_get(&rem)) {
                if (rem.type == LogMsg::Type::Reopen) {
                    if (!rem.prefix.empty()) prefix_    = rem.prefix;
                    if (!rem.path.empty())   root_path_ = rem.path;
                    if (rem.quota)           quota_     = rem.quota;
                    logger_path_.clear();
                    do_open();
                } else if (rem.type == LogMsg::Type::Log) {
                    int nDate = make_date();
                    if (check_quota())
                        do_open();
                    else if (date_ < nDate) {
                        date_ = nDate;
                        do_open();
                    }
                    file_.write(rem.text.c_str(), static_cast<unsigned int>(rem.text.size()));
                }
            }
            break;
        }

        if (msg.type == LogMsg::Type::Reopen) {
            if (!msg.prefix.empty())
                prefix_ = msg.prefix;
            if (!msg.path.empty())
                root_path_ = msg.path;
            if (msg.quota)
                quota_ = msg.quota;
            logger_path_.clear();
            do_open();
            continue;
        }

        // Log: check quota / date rollover, then write
        int nDate = make_date();
        if (check_quota())
            do_open();
        else if (date_ < nDate) {
            date_ = nDate;
            do_open();
        }
        file_.write(msg.text.c_str(), static_cast<unsigned int>(msg.text.size()));
    }
    file_.close();
    return nullptr;
}

// ── Log helpers ───────────────────────────────────────────────────────────────
static std::string FormatTime(TimeMs& t)
{
    return t.Format("%T");
}

int Logger::log(const char* pFormat, ...)
{
    char szTmp[20480];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(szTmp, sizeof(szTmp), pFormat, ap);
    va_end(ap);

    char szDbg[20480 + 64];
    snprintf(szDbg, sizeof(szDbg), "[p%05ld] %s\n", static_cast<long>(pid_), szTmp);
    int len = static_cast<int>(strlen(szDbg));

    push_log(std::string(szDbg, len));
    return len;
}

int Logger::log(LOGLEVEL l, const char* pFormat, ...)
{
    if (l > level_)
        return 0;

    char szTmp[20480];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(szTmp, sizeof(szTmp), pFormat, ap);
    va_end(ap);

    TimeMs tCur;
    std::string tStr = FormatTime(tCur);

    char szDbg[20480 + 64];
    snprintf(
        szDbg, sizeof(szDbg), "[p%05ld] %s> %s\n", static_cast<long>(pid_), tStr.c_str(), szTmp);
    int len = static_cast<int>(strlen(szDbg));

    push_log(std::string(szDbg, len));
    return len;
}

int Logger::log(const char* file, int line, LOGLEVEL l, const char* pFormat, ...)
{
    if (l > level_)
        return 0;

    char szTmp[20480];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(szTmp, sizeof(szTmp), pFormat, ap);
    va_end(ap);

    TimeMs tCur;
    std::string tStr = FormatTime(tCur);

    char szDbg[20480 + 128];
    if (l <= file_line_level_)
        snprintf(szDbg,
                 sizeof(szDbg),
                 "[p%05ld] %s> [%s:%d] %s\n",
                 static_cast<long>(pid_),
                 tStr.c_str(),
                 file,
                 line,
                 szTmp);
    else
        snprintf(szDbg,
                 sizeof(szDbg),
                 "[p%05ld] %s> %s\n",
                 static_cast<long>(pid_),
                 tStr.c_str(),
                 szTmp);

    int len = static_cast<int>(strlen(szDbg));
    push_log(std::string(szDbg, len));
    return len;
}

}  // namespace pdk

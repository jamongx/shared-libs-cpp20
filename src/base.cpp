// base.cpp – Time, TimeMs implementations

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include "base.hpp"

namespace pdk {

// ── Time::Format ──────────────────────────────────────────────────────────────
std::string Time::Format(const char* pFormat) const
{
    struct tm tm {};
    GetLocalTm(&tm);
    char buf[256];
    strftime(buf, sizeof(buf), pFormat, &tm);
    return buf;
}

// ── TimeMs ─────────────────────────────────────────────────────────────────────
TimeMs::TimeMs() : msec_(static_cast<int>(usec_ / 1000))
{
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    time_ = tv.tv_sec;
    usec_ = tv.tv_usec;
}

std::string TimeMs::Format(const char* pFormat) const
{
    return Time::Format(pFormat);
}

}  // namespace pdk

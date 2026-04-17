#pragma once

#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef LINUX
#include <sys/time.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "types.hpp"

namespace pdk {

// ── Object ────────────────────────────────────────────────────────────────────
// Minimal polymorphic base. RTTI macros (DECLARE_DYNAMIC etc.) removed.
class Object {
public:
    virtual ~Object() = default;
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

protected:
    Object() = default;
};

// ── Exception ─────────────────────────────────────────────────────────────────
class Exception : public std::runtime_error {
public:
    explicit Exception(const char* msg) : std::runtime_error(msg) {}
    static Exception format(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        return Exception(buf);
    }
};

// ── Time ──────────────────────────────────────────────────────────────────────
class Time {
public:
    static Time current_time_ms() noexcept { return Time(::time(nullptr)); }

    Time() noexcept = default;
    explicit Time(time_t t) noexcept : time_(t) {}
    Time(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, int nDST = -1) noexcept
    {
        struct tm tm {};
        tm.tm_year = nYear - 1900;
        tm.tm_mon = nMonth - 1;
        tm.tm_mday = nDay;
        tm.tm_hour = nHour;
        tm.tm_min = nMin;
        tm.tm_sec = nSec;
        tm.tm_isdst = nDST;
        time_ = mktime(&tm);
    }
    Time(const Time&) noexcept = default;
    Time& operator=(const Time&) noexcept = default;
    Time& operator=(time_t t) noexcept
    {
        time_ = t;
        return *this;
    }

    struct tm* GetLocalTm(struct tm* ptm) const noexcept { return localtime_r(&time_, ptm); }

    [[nodiscard]] time_t GetTime() const noexcept { return time_; }
    [[nodiscard]] int GetYear() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_year + 1900;
    }
    [[nodiscard]] int GetMonth() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_mon + 1;
    }
    [[nodiscard]] int GetDay() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_mday;
    }
    [[nodiscard]] int GetHour() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_hour;
    }
    [[nodiscard]] int GetMinute() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_min;
    }
    [[nodiscard]] int GetSecond() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_sec;
    }
    [[nodiscard]] int GetDayOfWeek() const noexcept
    {
        struct tm t {};
        GetLocalTm(&t);
        return t.tm_wday + 1;
    }

    bool operator==(Time o) const noexcept { return time_ == o.time_; }
    bool operator!=(Time o) const noexcept { return time_ != o.time_; }
    bool operator<(Time o) const noexcept { return time_ < o.time_; }
    bool operator>(Time o) const noexcept { return time_ > o.time_; }
    bool operator<=(Time o) const noexcept { return time_ <= o.time_; }
    bool operator>=(Time o) const noexcept { return time_ >= o.time_; }

    std::string Format(const char* pFormat) const;

protected:
    time_t time_{0};
};

// TimeMs – sub-second precision
class TimeMs : public Time {
public:
    TimeMs();
    std::string Format(const char* pFormat) const;

protected:
    long usec_{0};
    int msec_{0};
};

// ── File ──────────────────────────────────────────────────────────────────────
// RAII wrapper around a POSIX file descriptor.
class File : public Object {
public:
    enum OpenFlags {
        modeRead = 0x0000,
        modeWrite = 0x0001,
        modeReadWrite = 0x0002,
        modeCreate = 0x1000,
        modeNoTruncate = 0x2000,
        typeText = 0x4000,
        typeBinary = static_cast<int>(0x8000)
    };
    enum SeekPosition { begin = SEEK_SET, current = SEEK_CUR, end = SEEK_END };
    enum { hFileNull = -1 };

    File() noexcept = default;
    explicit File(int hFile) noexcept : fd_(hFile), close_on_delete_(false) {}
    File(const char* lpszFileName, unsigned int nOpenFlags, mode_t rmode = 0);

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    ~File() override
    {
        if (close_on_delete_ && fd_ != hFileNull)
            ::close(fd_);
    }

    operator int() const noexcept { return fd_; }

    int fd_{hFileNull};

    [[nodiscard]] virtual long position() const;
    [[nodiscard]] virtual std::string filename() const { return filename_; }

    virtual bool open(const char* lpszFileName, unsigned int nOpenFlags, mode_t rmode = 0);

    static void rename(const char* lpszOldName, const char* lpszNewName);
    static void remove(const char* lpszFileName);

    long seek_to_end() { return seek(0, end); }
    void seek_to_begin() { seek(0, begin); }

    virtual long seek(long lOff, unsigned int nFrom);
    [[nodiscard]] virtual unsigned long length() const;
    virtual unsigned int read(void* lpBuf, unsigned int nCount);
    virtual unsigned int write(const void* lpBuf, unsigned int nCount);
    virtual void flush();
    virtual void close();

protected:
    bool close_on_delete_{true};
    std::string filename_;
};

// ── Archive ───────────────────────────────────────────────────────────────────
// Binary serialisation using std::vector<uint8_t> (replaces raw pointer buffer).
class Archive {
public:
    enum Mode { store = 0, load = 1 };

    Archive(File* pFile, unsigned int nMode, int nBufSize = 4096);
    ~Archive();

    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;

    [[nodiscard]] bool is_loading() const noexcept { return (mode_ & load) != 0; }
    [[nodiscard]] bool is_storing() const noexcept { return (mode_ & load) == 0; }
    [[nodiscard]] File* file() const noexcept { return m_pFile; }

    void flush();
    void close();

    unsigned int read(void* pBuf, unsigned int nMax);
    void write(const void* pBuf, unsigned int nMax);

    void write_string(const char* lpsz);
    char* read_string(char* lpsz, unsigned int nMax);
    bool read_string(std::string& rString);

    uint32_t read_count();
    void write_count(uint32_t dwCount);
    void fill_buffer(unsigned int nBytesNeeded);

    // store
    Archive& operator<<(uint8_t by);
    Archive& operator<<(uint16_t w);
    Archive& operator<<(int32_t l);
    Archive& operator<<(uint32_t dw);
    Archive& operator<<(float f);
    Archive& operator<<(double d);
    Archive& operator<<(char c) { return *this << static_cast<uint8_t>(c); }
    Archive& operator<<(bool b) { return *this << static_cast<int32_t>(b); }

    // load
    Archive& operator>>(uint8_t& by);
    Archive& operator>>(uint16_t& w);
    Archive& operator>>(int32_t& l);
    Archive& operator>>(uint32_t& dw);
    Archive& operator>>(float& f);
    Archive& operator>>(double& d);
    Archive& operator>>(char& c) { return *this >> reinterpret_cast<uint8_t&>(c); }
    Archive& operator>>(bool& b);

    unsigned int object_schema_{0};
    std::string filename_;

private:
    unsigned int mode_;
    File* m_pFile;
    std::vector<uint8_t> buf_;
    size_t buf_cur_{0};
    size_t buf_fill_{0};

    void _ensureWrite(size_t n);
    void _ensureRead(size_t n);
};

}  // namespace pdk

// stream.h – in-memory stream backed by std::vector<uint8_t>
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "base.hpp"

namespace pdk {

enum TSeekOrigin { soBeginning, soCurrent, soEnd };

// ── Stream ───────────────────────────────────────────────────────────────────
class Stream : public Object {
public:
    Stream() = default;
    ~Stream() override = default;

    int32_t position() { return seek(0, soCurrent); }
    void set_position(int32_t pos) { seek(pos, soBeginning); }
    int32_t GetSize()
    {
        int32_t cur = seek(0, soCurrent);
        int32_t end = seek(0, soEnd);
        seek(cur, soBeginning);
        return end;
    }

    virtual void SetSize(int32_t) {}
    virtual int32_t read(void* buf, int32_t n)
    {
        (void)buf;
        (void)n;
        return 0;
    }
    virtual int32_t write(const void* buf, int32_t n)
    {
        (void)buf;
        (void)n;
        return 0;
    }
    virtual int32_t seek(int32_t offset, TSeekOrigin origin)
    {
        (void)offset;
        (void)origin;
        return 0;
    }

    void read_buffer(uint8_t* buf, int32_t n)
    {
        if (n != 0 && read(buf, n) != n)
            throw Exception("Stream: read error");
    }
    void write_buffer(uint8_t* buf, int32_t n)
    {
        if (n != 0 && write(buf, n) != n)
            throw Exception("Stream: write error");
    }
};

// ── MemoryStream ─────────────────────────────────────────────────────────────
class MemoryStream : public Stream {
public:
    MemoryStream() = default;
    ~MemoryStream() override = default;

    int32_t read(void* buf, int32_t n) override;
    int32_t write(const void* buf, int32_t n) override;
    int32_t seek(int32_t offset, TSeekOrigin origin) override;
    void SetSize(int32_t n) override;
    void clear()
    {
        buf_.clear();
        pos_ = 0;
    }

    uint8_t* data() { return buf_.empty() ? nullptr : buf_.data(); }
    int32_t capacity() { return static_cast<int32_t>(buf_.capacity()); }

private:
    std::vector<uint8_t> buf_;
    int32_t pos_{0};
};

inline int32_t MemoryStream::read(void* buf, int32_t n)
{
    if (pos_ < 0 || n <= 0)
        return 0;
    int32_t avail = static_cast<int32_t>(buf_.size()) - pos_;
    if (avail <= 0)
        return 0;
    int32_t cnt = (n < avail) ? n : avail;
    std::memcpy(buf, buf_.data() + pos_, static_cast<size_t>(cnt));
    pos_ += cnt;
    return cnt;
}

inline int32_t MemoryStream::write(const void* buf, int32_t n)
{
    if (pos_ < 0 || n <= 0)
        return 0;
    int32_t newSize = pos_ + n;
    if (newSize > static_cast<int32_t>(buf_.size()))
        buf_.resize(static_cast<size_t>(newSize));
    std::memcpy(buf_.data() + pos_, buf, static_cast<size_t>(n));
    pos_ = newSize;
    return n;
}

inline int32_t MemoryStream::seek(int32_t offset, TSeekOrigin origin)
{
    switch (origin) {
        case soBeginning:
            pos_ = offset;
            break;
        case soCurrent:
            pos_ += offset;
            break;
        case soEnd:
            pos_ = static_cast<int32_t>(buf_.size()) + offset;
            break;
    }
    return pos_;
}

inline void MemoryStream::SetSize(int32_t n)
{
    buf_.resize(static_cast<size_t>(n > 0 ? n : 0));
    if (pos_ > n)
        pos_ = n;
}

}  // namespace pdk

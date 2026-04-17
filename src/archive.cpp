// archive.cpp – Archive using std::vector<uint8_t>

#include <cstring>
#include "base.hpp"

namespace pdk {

// ── Constructor / Destructor ───────────────────────────────────────────────────
Archive::Archive(File* pFile, unsigned int nMode, int nBufSize)
    : mode_(nMode),
      m_pFile(pFile),
      buf_(static_cast<size_t>(nBufSize > 128 ? nBufSize : 128))
{}

Archive::~Archive()
{
    try {
        close();
    } catch (...) {}
}

// ── flush (store mode) ─────────────────────────────────────────────────────────
void Archive::flush()
{
    if (is_storing() && buf_cur_ > 0 && m_pFile) {
        m_pFile->write(buf_.data(), static_cast<unsigned int>(buf_cur_));
        buf_cur_ = 0;
    }
}

void Archive::close()
{
    flush();
    m_pFile = nullptr;
}

// ── fill_buffer (load mode) ─────────────────────────────────────────────────────
void Archive::fill_buffer(unsigned int /*nBytesNeeded*/)
{
    if (!m_pFile)
        return;
    size_t remaining = buf_fill_ - buf_cur_;
    if (remaining > 0)
        memmove(buf_.data(), buf_.data() + buf_cur_, remaining);
    size_t toRead = buf_.size() - remaining;
    unsigned int nRead = m_pFile->read(buf_.data() + remaining, static_cast<unsigned int>(toRead));
    buf_fill_ = remaining + nRead;
    buf_cur_ = 0;
}

// ── Internal helpers ───────────────────────────────────────────────────────────
void Archive::_ensureWrite(size_t n)
{
    if (buf_cur_ + n > buf_.size()) {
        flush();
        if (n > buf_.size())
            buf_.resize(n * 2);
    }
}

void Archive::_ensureRead(size_t n)
{
    if (buf_cur_ + n > buf_fill_)
        fill_buffer(static_cast<unsigned int>(n));
    if (buf_cur_ + n > buf_fill_)
        throw Exception("Archive: premature end of stream");
}

// ── Raw read / write ───────────────────────────────────────────────────────────
unsigned int Archive::read(void* pBuf, unsigned int nMax)
{
    _ensureRead(nMax);
    memcpy(pBuf, buf_.data() + buf_cur_, nMax);
    buf_cur_ += nMax;
    return nMax;
}

void Archive::write(const void* pBuf, unsigned int nMax)
{
    _ensureWrite(nMax);
    memcpy(buf_.data() + buf_cur_, pBuf, nMax);
    buf_cur_ += nMax;
}

// ── String IO ──────────────────────────────────────────────────────────────────
void Archive::write_string(const char* lpsz)
{
    auto len = static_cast<uint32_t>(strlen(lpsz));
    write_count(len);
    write(lpsz, len);
}

char* Archive::read_string(char* lpsz, unsigned int nMax)
{
    uint32_t len = read_count();
    if (len >= nMax)
        len = nMax - 1;
    read(lpsz, len);
    lpsz[len] = '\0';
    return lpsz;
}

bool Archive::read_string(std::string& rString)
{
    uint32_t len = read_count();
    rString.resize(len);
    if (len > 0)
        read(&rString[0], len);
    return true;
}

// ── Count IO ───────────────────────────────────────────────────────────────────
void Archive::write_count(uint32_t dwCount)
{
    *this << dwCount;
}
uint32_t Archive::read_count()
{
    uint32_t dw{};
    *this >> dw;
    return dw;
}

// ── Store operators ────────────────────────────────────────────────────────────
Archive& Archive::operator<<(uint8_t by)
{
    _ensureWrite(1);
    buf_[buf_cur_++] = by;
    return *this;
}
Archive& Archive::operator<<(uint16_t w)
{
    _ensureWrite(sizeof(w));
    memcpy(buf_.data() + buf_cur_, &w, sizeof(w));
    buf_cur_ += sizeof(w);
    return *this;
}
Archive& Archive::operator<<(int32_t l)
{
    _ensureWrite(sizeof(l));
    memcpy(buf_.data() + buf_cur_, &l, sizeof(l));
    buf_cur_ += sizeof(l);
    return *this;
}
Archive& Archive::operator<<(uint32_t dw)
{
    _ensureWrite(sizeof(dw));
    memcpy(buf_.data() + buf_cur_, &dw, sizeof(dw));
    buf_cur_ += sizeof(dw);
    return *this;
}
Archive& Archive::operator<<(float f)
{
    _ensureWrite(sizeof(f));
    memcpy(buf_.data() + buf_cur_, &f, sizeof(f));
    buf_cur_ += sizeof(f);
    return *this;
}
Archive& Archive::operator<<(double d)
{
    _ensureWrite(sizeof(d));
    memcpy(buf_.data() + buf_cur_, &d, sizeof(d));
    buf_cur_ += sizeof(d);
    return *this;
}

// ── Load operators ─────────────────────────────────────────────────────────────
Archive& Archive::operator>>(uint8_t& by)
{
    _ensureRead(1);
    by = buf_[buf_cur_++];
    return *this;
}
Archive& Archive::operator>>(uint16_t& w)
{
    _ensureRead(sizeof(w));
    memcpy(&w, buf_.data() + buf_cur_, sizeof(w));
    buf_cur_ += sizeof(w);
    return *this;
}
Archive& Archive::operator>>(int32_t& l)
{
    _ensureRead(sizeof(l));
    memcpy(&l, buf_.data() + buf_cur_, sizeof(l));
    buf_cur_ += sizeof(l);
    return *this;
}
Archive& Archive::operator>>(uint32_t& dw)
{
    _ensureRead(sizeof(dw));
    memcpy(&dw, buf_.data() + buf_cur_, sizeof(dw));
    buf_cur_ += sizeof(dw);
    return *this;
}
Archive& Archive::operator>>(float& f)
{
    _ensureRead(sizeof(f));
    memcpy(&f, buf_.data() + buf_cur_, sizeof(f));
    buf_cur_ += sizeof(f);
    return *this;
}
Archive& Archive::operator>>(double& d)
{
    _ensureRead(sizeof(d));
    memcpy(&d, buf_.data() + buf_cur_, sizeof(d));
    buf_cur_ += sizeof(d);
    return *this;
}
Archive& Archive::operator>>(bool& b)
{
    int32_t i{};
    *this >> i;
    b = (i != 0);
    return *this;
}

}  // namespace pdk

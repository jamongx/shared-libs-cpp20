// file.cpp – File RAII implementation

#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "base.hpp"

namespace pdk {

// ── Constructor: open by name ──────────────────────────────────────────────────
File::File(const char* lpszFileName, unsigned int nOpenFlags, mode_t rmode)
{
    if (!open(lpszFileName, nOpenFlags, rmode))
        throw Exception::format("File: cannot open '%s': %s", lpszFileName, strerror(errno));
}

// ── Open ───────────────────────────────────────────────────────────────────────
bool File::open(const char* lpszFileName, unsigned int nOpenFlags, mode_t rmode)
{
    int flags = 0;
    if ((nOpenFlags & modeReadWrite) == modeReadWrite)
        flags = O_RDWR;
    else if (nOpenFlags & modeWrite)
        flags = O_WRONLY;
    else
        flags = O_RDONLY;

    if (nOpenFlags & modeCreate)
        flags |= O_CREAT;
    // Append if writing without explicit NoTruncate suppression
    if ((nOpenFlags & modeWrite) && (nOpenFlags & modeNoTruncate))
        flags |= O_APPEND;
    else if ((nOpenFlags & modeWrite) && !(nOpenFlags & modeNoTruncate) &&
             (nOpenFlags & modeCreate))
        flags |= O_TRUNC;

    mode_t mode = rmode ? rmode : 0666;
    int fd = ::open(lpszFileName, flags, mode);
    if (fd == -1)
        return false;

    if (fd_ != hFileNull && close_on_delete_)
        ::close(fd_);

    fd_ = fd;
    close_on_delete_ = true;
    filename_ = lpszFileName;
    return true;
}

// ── Seek ───────────────────────────────────────────────────────────────────────
long File::seek(long lOff, unsigned int nFrom)
{
    return static_cast<long>(lseek(fd_, lOff, static_cast<int>(nFrom)));
}

// ── position ────────────────────────────────────────────────────────────────
long File::position() const
{
    return static_cast<long>(lseek(fd_, 0, SEEK_CUR));
}

// ── length ──────────────────────────────────────────────────────────────────
unsigned long File::length() const
{
    struct stat st {};
    if (fstat(fd_, &st) == 0)
        return static_cast<unsigned long>(st.st_size);
    return 0;
}

// ── Read / Write ───────────────────────────────────────────────────────────────
unsigned int File::read(void* lpBuf, unsigned int nCount)
{
    ssize_t n = ::read(fd_, lpBuf, nCount);
    return n < 0 ? 0 : static_cast<unsigned int>(n);
}

unsigned int File::write(const void* lpBuf, unsigned int nCount)
{
    ssize_t n = ::write(fd_, lpBuf, nCount);
    return n < 0 ? 0 : static_cast<unsigned int>(n);
}

// ── Flush / Close ──────────────────────────────────────────────────────────────
void File::flush()
{
    if (fd_ != hFileNull)
        fsync(fd_);
}

void File::close()
{
    if (fd_ != hFileNull && close_on_delete_) {
        ::close(fd_);
        fd_ = hFileNull;
    }
}

// ── Static helpers ─────────────────────────────────────────────────────────────
void File::rename(const char* lpszOldName, const char* lpszNewName)
{
    ::rename(lpszOldName, lpszNewName);
}

void File::remove(const char* lpszFileName)
{
    unlink(lpszFileName);
}

}  // namespace pdk

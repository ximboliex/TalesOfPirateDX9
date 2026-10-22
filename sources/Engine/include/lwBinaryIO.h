#pragma once
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <limits>

// File offsets are addresses within the file, not process pointers. The disk
// format still uses DWORD offsets; seek with a 64-bit API to avoid signed-long
// truncation between 2 and 4 GiB on Windows.
inline int lwFileSeek(FILE* file, int64_t offset, int origin)
{
#if defined(_WIN32)
    return _fseeki64(file, offset, origin);
#else
    return fseeko(file, offset, origin);
#endif
}
inline int64_t lwFileTell(FILE* file)
{
#if defined(_WIN32)
    return _ftelli64(file);
#else
    return ftello(file);
#endif
}
inline int64_t lwFileRemaining(FILE* file)
{
    const int64_t position = lwFileTell(file);
    if (position < 0 || lwFileSeek(file, 0, SEEK_END) != 0) return -1;
    const int64_t end = lwFileTell(file);
    if (lwFileSeek(file, position, SEEK_SET) != 0 || end < position) return -1;
    return end - position;
}

template<class T> inline bool lwReadArray(FILE* file, T*& target, size_t count)
{
    const int64_t remaining = lwFileRemaining(file);
    if (remaining < 0 || count > static_cast<uint64_t>(remaining) / sizeof(T) ||
        count > (std::numeric_limits<size_t>::max)() / sizeof(T)) return false;
    if (count == 0) { delete[] target; target = NULL; return true; }
    std::unique_ptr<T[]> data(new T[count]);
    if (fread(data.get(), sizeof(T), count, file) != count) return false;
    delete[] target;
    target = data.release();
    return true;
}

template<class T> inline bool lwWriteArray(FILE* file, const T* values, size_t count)
{
    if (count == 0) return true;
    return values && count <= (std::numeric_limits<size_t>::max)() / sizeof(T) &&
        fwrite(values, sizeof(T), count, file) == count;
}

class lwScopedFile
{
    FILE* file_;
    lwScopedFile(const lwScopedFile&) = delete;
    lwScopedFile& operator=(const lwScopedFile&) = delete;
public:
    explicit lwScopedFile(FILE* file) : file_(file) {}
    ~lwScopedFile() { if (file_) fclose(file_); }
};

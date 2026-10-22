#include <cassert>
#include "lwBinaryIO.h"
#include "lwMeshFormat.h"

int main()
{
    FILE* file = tmpfile();
    assert(file);
    lwScopedFile guard(file);
    assert(lwTextureCoordinateCount(0x302) == 3);
    assert(lwTextureCoordinateCount(0x212) == 2);
    assert(lwTextureCoordinateCount(0x112) == 1);
    const uint32_t values[] = {0, 1, 0xffffffff};
    assert(fwrite(values, sizeof(values), 1, file) == 1);
    assert(lwFileSeek(file, 0, SEEK_SET) == 0);
    uint32_t* data = NULL;
    assert(lwReadArray(file, data, 3));
    assert(data[2] == 0xffffffff);
    uint32_t* previous = data;
    assert(!lwWriteArray<uint32_t>(file, NULL, 1));
    assert(lwWriteArray<uint32_t>(file, NULL, 0));
    assert(!lwReadArray(file, data, 1));
    assert(data == previous); // A truncated read cannot replace the old data.
    assert(!lwReadArray(file, data, (std::numeric_limits<size_t>::max)()));
    delete[] data;

    // A sparse file exercises actual seek/tell above the signed Win32 limit.
    const int64_t offset = INT64_C(0x80000100);
    assert(lwFileSeek(file, offset, SEEK_SET) == 0);
    assert(fwrite(values, sizeof(values), 1, file) == 1);
    assert(lwFileTell(file) == offset + sizeof(values));
    assert(lwFileSeek(file, offset, SEEK_SET) == 0);
    assert(lwFileRemaining(file) == sizeof(values));
    assert(lwFileTell(file) == offset);
}

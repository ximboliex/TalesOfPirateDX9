#include <cassert>
#include <cstring>
#include <cstdio>
#include "lwTextureDisk.h"

struct NativeTexture
{
    uint32_t metadata[11];
    char file_name[64];
    void* data;
    uint32_t tss_set[8][3];
};

int main(int argc, char** argv)
{
    static_assert(sizeof(void*) == 8, "Exercise native 64-bit pointer padding");
    static_assert(sizeof(NativeTexture) != sizeof(lwTextureDisk32), "Exercise different disk/native layouts");
    lwTextureDisk32 disk = {};
    disk.stage = 3;
    disk.type = 0;
    disk.format = 21;
    std::strcpy(disk.file_name, "legacy_texture.dds");
    disk.obsolete_data_pointer = 0xdeadbeef;
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned j = 0; j < 3; ++j) disk.tss_set[i][j] = 100*i+j;
    NativeTexture native = {};
    lwTextureFromDisk(native, disk);
    assert(native.data == nullptr);
    assert(std::strcmp(native.file_name, disk.file_name) == 0);
    assert(std::memcmp(native.tss_set, disk.tss_set, sizeof(disk.tss_set)) == 0);
    native.data = reinterpret_cast<void*>(uintptr_t(0x123456789abcdef0ULL));
    const auto result = lwTextureToDisk(native);
    assert(result.obsolete_data_pointer == 0);
    disk.obsolete_data_pointer = 0;
    assert(std::memcmp(&result, &disk, sizeof(disk)) == 0);
    if (argc > 1)
    {
        FILE* input = std::fopen(argv[1], "rb");
        assert(input);
        size_t records = 0, bytes;
        while ((bytes = std::fread(&disk, 1, sizeof(disk), input)) != 0)
        {
            assert(bytes == sizeof(disk));
            lwTextureFromDisk(native, disk);
            assert(native.data == nullptr);
            const auto restored = lwTextureToDisk(native);
            disk.obsolete_data_pointer = 0;
            assert(std::memcmp(&restored, &disk, sizeof(disk)) == 0);
            ++records;
        }
        assert(!std::ferror(input));
        std::fclose(input);
        assert(records > 0);
        std::printf("Real model texture records roundtripped: %zu\n", records);
    }
}

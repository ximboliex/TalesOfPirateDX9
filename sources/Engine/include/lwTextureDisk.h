#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Legacy LGO/LMO texture record. Never serialize a native void* into this ABI.
struct lwTextureDisk32
{
    uint32_t stage, level, usage, format, pool, byte_alignment_flag, type;
    uint32_t width, height, colorkey_type, colorkey;
    char file_name[64];
    uint32_t obsolete_data_pointer;
    uint32_t tss_set[8][3];
};
static_assert(sizeof(lwTextureDisk32) == 208, "Legacy texture record must remain 208 bytes");
static_assert(offsetof(lwTextureDisk32, obsolete_data_pointer) == 108, "Legacy texture prefix changed");
static_assert(offsetof(lwTextureDisk32, tss_set) == 112, "Legacy texture states moved");

template<class Texture> inline void lwTextureFromDisk(Texture& texture, const lwTextureDisk32& disk)
{
    static_assert(offsetof(Texture, file_name) == 44, "Native texture prefix changed");
    static_assert(sizeof(texture.file_name) == 64, "Native texture filename changed");
    static_assert(sizeof(texture.tss_set) == sizeof(disk.tss_set), "Native texture states changed");
    memcpy(static_cast<void*>(&texture), &disk, offsetof(lwTextureDisk32, obsolete_data_pointer));
    texture.data = NULL;
    memcpy(texture.tss_set, disk.tss_set, sizeof(disk.tss_set));
}

template<class Texture> inline lwTextureDisk32 lwTextureToDisk(const Texture& texture)
{
    lwTextureDisk32 disk = {};
    memcpy(&disk, &texture, offsetof(lwTextureDisk32, obsolete_data_pointer));
    memcpy(disk.tss_set, texture.tss_set, sizeof(disk.tss_set));
    return disk;
}

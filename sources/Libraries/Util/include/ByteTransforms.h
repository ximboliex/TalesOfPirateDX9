#pragma once

// Match the legacy x86 ROL/ROR byte helpers without depending on registers or
// calling conventions. Counts >= 8 were rejected by those helpers.
namespace ByteTransforms
{
inline bool RotateLeft(unsigned char* data, unsigned int offset, unsigned char bits)
{
    if (bits >= 8) return false;
    const unsigned int value = data[offset];
    data[offset] = static_cast<unsigned char>((value << bits) | (value >> ((8 - bits) & 7)));
    return true;
}

inline bool RotateRight(unsigned char* data, unsigned int offset, unsigned char bits)
{
    if (bits >= 8) return false;
    const unsigned int value = data[offset];
    data[offset] = static_cast<unsigned char>((value >> bits) | (value << ((8 - bits) & 7)));
    return true;
}

inline bool Xor(unsigned char* data, unsigned int offset, unsigned char mask)
{
    data[offset] ^= mask;
    return true;
}
}

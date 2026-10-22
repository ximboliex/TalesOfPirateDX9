#pragma once
#include <stdint.h>

// D3DFVF_TEX0..8 encode a count, not independent flags: TEX3 (0x300)
// contains TEX1's bit and must not be interpreted as one coordinate set.
constexpr unsigned lwTextureCoordinateCount(uint32_t fvf)
{
    return (fvf >> 8) & 0xf;
}

#include <cassert>
#include "ByteTransforms.h"

int main()
{
    // Exhaustive comparison with the 8-bit x86 ROL/ROR operation.
    for (unsigned int value = 0; value < 256; ++value)
        for (unsigned int count = 0; count < 256; ++count)
        {
            unsigned char left = static_cast<unsigned char>(value);
            unsigned char right = left;
            bool l = ByteTransforms::RotateLeft(&left, 0, count);
            bool r = ByteTransforms::RotateRight(&right, 0, count);
            assert(l == (count < 8) && r == l);
            if (count < 8)
            {
                unsigned char expected = static_cast<unsigned char>(value);
                for (unsigned int i = 0; i < count; ++i)
                    expected = static_cast<unsigned char>((expected << 1) | (expected >> 7));
                assert(left == expected);
                ByteTransforms::RotateRight(&left, 0, count);
                ByteTransforms::RotateLeft(&right, 0, count);
            }
            assert(left == value && right == value);
        }
    for (unsigned int value = 0; value < 256; ++value)
        for (unsigned int mask = 0; mask < 256; ++mask)
        {
            unsigned char data = static_cast<unsigned char>(value);
            ByteTransforms::Xor(&data, 0, mask);
            assert(data == (value ^ mask));
        }
}

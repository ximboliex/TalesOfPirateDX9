// Compile the engine's real asset types with MSVC's Windows LLP64 layout.
// This test requires only SDK headers, not the game or D3DX import libraries.
#include <cstdio>
#include "lwExpObj.h"
#include "lwMeshFormat.h"

int main()
{
    using namespace MindPower;
    static_assert(sizeof(void*) == 8 && sizeof(DWORD) == 4 && sizeof(long) == 4);
    static_assert(sizeof(lwMaterial) == 68);
    static_assert(sizeof(lwRenderStateAtom) == 12);
    static_assert(sizeof(lwTexInfo) == 216);
    static_assert(sizeof(lwTextureDisk32) == 208);
    static_assert(sizeof(lwTexInfo_0001) == 240);
    static_assert(sizeof(lwMeshInfoHeader) == 128);
    static_assert(sizeof(lwGeomObjInfo::lwGeomObjInfoHeader) == 116);
    static_assert(sizeof(lwModelObjInfo::lwModelObjInfoHeader) == 12);
    static_assert(sizeof(lwHelperDummyInfo) == 140);
    static_assert(sizeof(lwHelperBoxInfo) == 132);
    static_assert(sizeof(lwHelperMeshFaceInfo) == 52);
    static_assert(sizeof(lwBoundingBoxInfo) == 92);
    static_assert(sizeof(lwBoundingSphereInfo) == 84);
    static_assert(lwTextureCoordinateCount(D3DFVF_TEX3) == 3);
    static_assert(lwTextureCoordinateCount(D3DFVF_TEX8) == 8);
    std::puts("Real engine asset types preserve the Windows x64 disk ABI");
}

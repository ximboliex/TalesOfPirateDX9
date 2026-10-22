// DXEffectFile.h: interface for the CMPEffectFile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTFILE_H__F7B73B81_55EE_11BD_AC67_0008C720ECD1__INCLUDED_)
#define AFX_DXEFFECTFILE_H__F7B73B81_55EE_11BD_AC67_0008C720ECD1__INCLUDED_

#if _MSC_VER > 1000
//#pragma once
#endif // _MSC_VER > 1000
class   MPRender;

#include "D3dx9effect.h"
#include "lwEffectPassState.h"
#include "i_effect.h"

class CMPEffectFile  
{
public:
#ifdef USE_RENDER
	CMPEffectFile(MPRender*	 pDev);
#else
	CMPEffectFile(LPDIRECT3DDEVICE8 pDev);
#endif
	CMPEffectFile();
	virtual ~CMPEffectFile();

#ifdef USE_RENDER
	void InitDev(MPRender* pDev);
#else
	void InitDev(LPDIRECT3DDEVICE8 pDev);
#endif

#ifdef USE_RENDER
	MPRender*	GetDev();
#else
	LPDIRECT3DDEVICE8	CMPEffectFile::GetDev();
#endif
	BOOL OnResetDevice();
	BOOL OnLostDevice();

	//BOOL LoadEffectFromResource(TCHAR* pszsrc);
	BOOL LoadEffectFromFile(LPCSTR pszfile);

	BOOL SetTechnique(int iIdx);

	//BOOL CreateVertexShader();
	//BOOL SetVertexShader();

	BOOL SetTexture(LPCSTR TextureValue,IDirect3DTextureX* pTexture);
	BOOL SetDword(LPCSTR DwName, DWORD dwvalue);

	BOOL Begin(DWORD  dwIsSave = 0);
	BOOL Pass(UINT ipass);
	BOOL End();

	int	 GetTechCount()						{ return _iTechNum;}
	int  GetPassCount()						{ return m_passes;}
	void free();

public:
#ifdef USE_RENDER
	MPRender*			 m_pDev;
#else
	LPDIRECT3DDEVICE8    m_pDev;
#endif

	LPD3DXEFFECT         m_pEffect;
	UINT                 m_passes = 0;

protected:
    lwEffectPassState _passState;
	int									_iTechNum;
	std::vector<D3DXHANDLE>		_vecTechniques;

	DWORD								_dwVShader;
};
inline BOOL CMPEffectFile::SetTechnique(int iIdx)
{
    if(!m_pEffect || iIdx < 0 || static_cast<size_t>(iIdx) >= _vecTechniques.size()) return FALSE;
    return SUCCEEDED(m_pEffect->SetTechnique(_vecTechniques[iIdx]));
}

//inline  BOOL CMPEffectFile::CreateVertexShader()
//{
//	LPD3DXBUFFER pCode;   //!Ö¸Áî»º³å
//	DWORD dwEffVerDecl[] =
//    {
//        D3DVSD_STREAM( 0 ),
//        D3DVSD_REG( D3DVSDE_POSITION , D3DVSDT_FLOAT3 ), // Position of first mesh
//		D3DVSD_REG( D3DVSDE_BLENDINDICES,D3DVSDT_FLOAT1),
//        D3DVSD_REG( D3DVSDE_DIFFUSE, D3DVSDT_D3DCOLOR ), // diffuse
//        D3DVSD_REG( D3DVSDE_TEXCOORD0, D3DVSDT_FLOAT2 ), // Tex coords
//        D3DVSD_END()
//    };
//
//	if(FAILED(D3DXAssembleShaderFromFile( "shader\\eff.vsh", NULL, 0, &pCode, NULL ) ) )
//	{
//		return FALSE;
//	}
//    if( FAILED(m_pDev->CreateVertexShader( dwEffVerDecl, 
//                                          (DWORD*)pCode->GetBufferPointer(),
//                                          &_dwVShader , FALSE ) ) )
//    {
//         return FALSE;
//    }
//
//    pCode->Release();
//	return TRUE;
//}

//inline BOOL CMPEffectFile::SetVertexShader()
//{
//	m_pDev->SetVertexShader(_dwVShader);
//	return  TRUE;
//}
inline BOOL CMPEffectFile::SetTexture(LPCSTR name, IDirect3DTextureX* texture)
{
    return m_pEffect && SUCCEEDED(m_pEffect->SetTexture(name, texture));
}
inline BOOL CMPEffectFile::SetDword(LPCSTR name, DWORD value)
{
    // D3DX9 removed SetDword; preserve its four-byte parameter representation.
    return m_pEffect && SUCCEEDED(m_pEffect->SetValue(name, &value, sizeof(value)));
}
inline BOOL CMPEffectFile::Begin(DWORD flags)
{
    return _passState.Begin(m_pEffect, flags, m_passes);
}
inline BOOL CMPEffectFile::Pass(UINT index = 0)
{
    return _passState.Pass(m_pEffect, index, m_passes);
}
inline BOOL CMPEffectFile::End()
{
    return _passState.End(m_pEffect);
}

#endif // !defined(AFX_DXEFFECTFILE_H__F7B73B81_55EE_11BD_AC67_0008C720ECD1__INCLUDED_)

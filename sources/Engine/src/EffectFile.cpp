// DXEffectFile.cpp: implementation of the CMPEffectFile class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
//#include "../../../proj/EffectEditer.h"
//#include <mindpower.h>
#include "GlobalInc.h"

#include "EffectFile.h"
//#include "DXUtil.h"
#include "MPRender.h"
#include <memory>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CMPEffectFile::CMPEffectFile()
{
	m_pEffect = NULL;
	m_pDev = NULL;
	_vecTechniques.clear();
	_iTechNum = 0;
	_dwVShader = 0;
}
#ifdef USE_RENDER
CMPEffectFile::CMPEffectFile(MPRender*	 pDev)
#else
CMPEffectFile::CMPEffectFile(LPDIRECT3DDEVICE8 pDev)
#endif
{
    m_pDev = pDev;
    m_pEffect   = NULL;
	_vecTechniques.clear();
	_iTechNum = 0;
	_dwVShader = 0;

}
CMPEffectFile::~CMPEffectFile()
{
	free();
}
#ifdef USE_RENDER
void CMPEffectFile::InitDev(MPRender* pDev)
#else
void CMPEffectFile::InitDev(LPDIRECT3DDEVICE8 pDev)
#endif
{
    m_pDev = pDev;
}

BOOL CMPEffectFile::LoadEffectFromFile(LPCSTR file)
{
    if(!m_pDev || !m_pDev->GetDevice() || !file) return FALSE;
    ID3DXEffect* loaded = NULL;
    HRESULT hr = D3DXCreateEffectFromFile(m_pDev->GetDevice(), file, NULL, NULL, 0, NULL, &loaded, NULL);
    const auto release = [](ID3DXEffect* effect) { if(effect) effect->Release(); };
    std::unique_ptr<ID3DXEffect, decltype(release)> effect(loaded, release);
    if(FAILED(hr) || !loaded) return FALSE;
    D3DXEFFECT_DESC description;
    if(FAILED(effect->GetDesc(&description)) || description.Techniques == 0) return FALSE;
    std::vector<D3DXHANDLE> techniques;
    techniques.reserve(description.Techniques);
    for(UINT i = 0; i < description.Techniques; ++i)
    {
        D3DXHANDLE technique = effect->GetTechnique(i);
        if(!technique) return FALSE;
        techniques.push_back(technique);
    }
    // Keep the previous effect intact if compilation/inspection failed. Numeric
    // technique indices now correspond to the file, including its first t0.
    free();
    m_pEffect = effect.release();
    _vecTechniques.swap(techniques);
    _iTechNum = static_cast<int>(_vecTechniques.size());
    return TRUE;
}

//BOOL CMPEffectFile::LoadEffectFromResource(TCHAR*  pszsrc)
//{
//    HRESULT hr;
//    HMODULE hModule = NULL;
//    HRSRC rsrc;
//    HGLOBAL hgData;
//    LPVOID pvData;
//    DWORD cbData;
//	D3DXTECHNIQUE_DESC   technique;
//
//    rsrc = FindResource( hModule, pszsrc, "EFFECT" );
//    if( rsrc != NULL )
//    {
//        cbData = SizeofResource( hModule, rsrc );
//        if( cbData > 0 )
//        {
//            hgData = LoadResource( hModule, rsrc );
//            if( hgData != NULL )
//            {
//                pvData = LockResource( hgData );
//                if( pvData != NULL )
//                {
//                    if( FAILED( hr = D3DXCreateEffect( 
//                        m_pDev, pvData, cbData, 
//						&m_pEffect,NULL) ) )
//                    {
//                        return FALSE;
//                    }
//					else
//						return TRUE;
//                }
//            }
//        }
//    }
//	if (FAILED(m_pEffect->FindNextValidTechnique(NULL, &technique)))
//	{
//		return FALSE;
//	}
//	_vecTechniques.push_back(technique);
//	_iTechNum++;
//
//	while(SUCCEEDED(m_pEffect->FindNextValidTechnique(technique.Name, &technique)))
//	{
//		_vecTechniques.push_back(technique);
//		_iTechNum++;
//	}
//
//	return FALSE;
//}


void CMPEffectFile::free()
{
    _passState.End(m_pEffect);
	SAFE_RELEASE(m_pEffect);
    m_passes = 0;
	_vecTechniques.clear();
	_iTechNum = 0;
	_dwVShader = 0;
//	if(_dwVShader)
//	{
//		m_pDev->DeleteVertexShader(_dwVShader);
//		_dwVShader = 0;
//	}
}

BOOL CMPEffectFile::OnLostDevice()
{
    _passState.End(m_pEffect);
    if(m_pEffect)
	{
		if(FAILED(m_pEffect->OnLostDevice()))
			return FALSE;
	}
	return TRUE;
}

BOOL CMPEffectFile::OnResetDevice()
{
    if(m_pEffect)
	{
		if(FAILED(m_pEffect->OnResetDevice()))
			return FALSE;
	}
	return TRUE;
}
#ifdef USE_RENDER
MPRender*	CMPEffectFile::GetDev()
#else
LPDIRECT3DDEVICE8	CMPEffectFile::GetDev()
#endif
{
	return m_pDev;
}


#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <set>
using DWORD=unsigned;using BOOL=bool;
struct RECT {int left=0,top=0,right=0,bottom=0;};struct POINT {int x,y;};
void SetRect(RECT* r,int l,int t,int x,int y){*r={l,t,x,y};}
#define ZeroMemory(a,b) std::memset(a,0,b)
#define SAFE_RELEASE(p) do{if(p){(p)->Release();(p)=nullptr;}}while(0)
constexpr int TILE_SIZE=32,SHOWRSIZE=40,D3DFMT_A4R4G4B4=0;
static int loads=0;static std::set<void*> alive;
struct MPITex {MPITex(){alive.insert(this);}void Release(){assert(alive.erase(this)==1);delete this;}};
int _access(const char*,int){return 0;}
struct Resources {void* res_mgr=nullptr;};struct Renderer {Resources resource;Resources* GetInterfaceMgr(){return &resource;}}g_Render;
int lwLoadTex(MPITex** texture,void*,const char*,int,int){++loads;*texture=new MPITex;return 0;}
struct Pack{bool IsExit(int,int){return true;}void GetTex(MPITex** t,int,int){++loads;*t=new MPITex;}};
struct Terrain {int GetWidth(){return 4096;}int GetHeight(){return 4096;}};
struct Character {int GetCurX(){return 100000;}int GetCurY(){return 100000;}};
struct Scene {Terrain terrain;static Character* GetMainCha(){static Character c;return &c;}Terrain* GetTerrain(){return &terrain;}const char* GetTerrainName(){return "garner";}}scene;
struct CGameApp {static Scene* GetCurScene(){return &scene;}};
struct App {Scene* GetCurScene(){return &scene;}}app;
App* g_pGameApp=&app;using CGameScene=Scene;
class CLargerMap {
public:
 bool _bUpdate=false;RECT _rcWnd{0,0,526,420};Scene* m_pScene=&scene;
 MPITex* _pTexDefault=new MPITex;MPITex* _pTexMask=nullptr;Pack* _pMiniPack=nullptr;
 int _TexWnum=0,_TexHnum=0,_nCenterX=0,_nCenterY=0,_nInitX=0,_nInitY=0,_sx=0,_sy=0;
 float map_scale=1,fInf=0;
 std::vector<MPITex*> _pTex;std::vector<POINT> tileKeys;std::vector<RECT> vecRect;std::vector<DWORD> vecColor;
 void Show(bool);void Update(int,int);void GetChangeNameText(char*){}
 ~CLargerMap(){Show(false);SAFE_RELEASE(_pTexDefault);}
};

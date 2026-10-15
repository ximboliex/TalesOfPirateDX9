// Deterministic game/Win32 adapters for compiling the entire production service.
// These doubles do not establish MSVC, DX9 or network integration correctness.
#include "MapPOI.h"
#include <set>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <mutex>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <cstdarg>
#define private public
#include "MapPOIService.h"
#undef private
using DWORD=unsigned long;
using HANDLE=FILE*;
#define INVALID_HANDLE_VALUE nullptr
#define GENERIC_WRITE 0
#define CREATE_ALWAYS 0
#define FILE_ATTRIBUTE_NORMAL 0
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 2
static std::uint64_t tick=1;
std::uint64_t GetTickCount64(){return tick;}
unsigned long GetLastError(){return 1;}
HANDLE CreateFileA(const char* p,int,int,void*,int,int,void*){return std::fopen(p,"wb");}
bool WriteFile(HANDLE f,const void* d,DWORD size,DWORD* written,void*){*written=std::fwrite(d,1,size,f);return *written==size;}
bool FlushFileBuffers(HANDLE f){return !std::fflush(f);}
void CloseHandle(HANDLE f){std::fclose(f);}
bool MoveFileExA(const char* a,const char* b,int){return !std::rename(a,b);}
void LG(const char*,const char*,...){}
enum {enumEVENTE_SMAP_ENTRY=1,enumEVENTE_DMAP_ENTRY=2,enumACTCONTROL_EYESHOT=1,CMD_MC_MAP_POI=625};
struct WPACKET{std::vector<std::uint8_t> data;};
WPACKET GETWPACKET(){return {};}
void WRITE_CMD(WPACKET&,int){}
void WRITE_SEQ(WPACKET& p,const char* data,std::uint16_t size){p.data.assign(data,data+size);}
namespace dbc {using uShort=std::uint16_t;}
class SubMap {public:std::string name="testcopy";short copy=1;const char* GetName(){return name.c_str();}short GetCopyNO(){return copy;}};
struct Pos{std::int32_t x=10000,y=20000;};
struct Event {unsigned type=0,id=0;unsigned GetExecType(){return type;}unsigned GetID(){return id;}const char* GetName(){return "";}};
namespace mission {enum {TRANSIT_ENTITY=4};class CEventEntity;}
class CCharacter;
class Entity {
public:
 virtual ~Entity()=default;
 unsigned long world=1;long handle=1;std::uint64_t life=1;SubMap* map=nullptr;Pos pos;Event event;bool living=true;
 const SubMap* poiOwner=nullptr;std::uint64_t poiId=0;std::string name="Point";
 virtual CCharacter* IsCharacter(){return nullptr;}virtual mission::CEventEntity* IsEvent(){return nullptr;}
 Event& GetEvent(){return event;}unsigned long GetID(){return world;}long GetHandle(){return handle;}
 std::uint64_t GetLifeGeneration(){return life;}SubMap* GetSubMap(){return map;}Pos GetPos(){return pos;}
 const char* GetName(){return name.c_str();}bool IsLiveing(){return living;}
 unsigned GetCat(){return 2492;}const SubMap* GetMapPOIOwner(){return poiOwner;}std::uint64_t GetMapPOIId(){return poiId;}
 void SetMapPOI(const SubMap* o,std::uint64_t id){poiOwner=o;poiId=id;}
};
class CCharacter:public Entity {
public:
 unsigned templateId=712;bool player=false,focus=false,eyes=true,visible=true;WPACKET last;
 CCharacter* IsCharacter()override{return this;}bool IsPlayerOwnCha(){return player;}
 bool IsPlayerFocusCha(){return focus;}unsigned GetMapPOITemplateID(){return templateId;}
 bool GetActControl(int){return eyes;}bool CanSeen(CCharacter*){return visible;}
 void ReflectINFof(CCharacter*,WPACKET p){last=p;}
};
namespace mission {class CEventEntity:public CCharacter {public:CEventEntity* IsEvent()override{return this;}int GetType(){return TRANSIT_ENTITY;}};}
struct App {
 std::vector<Entity*> entities;
 Entity* IsValidEntity(unsigned long id,long handle){for(auto* e:entities)if(e->world==id && e->handle==handle)return e;return nullptr;}
} app;
App* g_pGameApp=&app;

#!/usr/bin/env python3
"""Compile production fragments with deterministic doubles. Does not build GameServer."""
from pathlib import Path
import subprocess
import os
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'sources/Server/GameServer/src'

def read(name):
    return (SRC / name).read_bytes().decode('latin1')

def between(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]

def compile_run(name, source, folder):
    path = folder / (name + '.cpp')
    path.write_text(source)
    exe = folder / name
    subprocess.run(['g++', '-std=c++14', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(path), '-o', str(exe)], check=True)
    env = os.environ.copy()
    # LeakSanitizer cannot inspect /proc in the execution container.
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    subprocess.run([str(exe)], check=True, env=env)

COMMON = r'''
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <string>
#include <limits>
#include <functional>
#include <cmath>
using namespace std;
#define T_B
#define T_E
#define LG(...) ((void)0)
#define TRUE 1
#define FALSE 0
#define RES_STRING(...) "test"
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x << std::endl; std::abort(); } } while(0)
namespace dbc {using Long=int32_t;using uLong=uint32_t;using Short=int16_t;using uChar=unsigned char;using Char=char;}
using uLong=dbc::uLong;using Long=dbc::Long;using Char=dbc::Char;using Short=dbc::Short;
using DWORD=uint32_t;using BYTE=uint8_t;using BOOL=int;
static unsigned long long now=0;
DWORD GetTickCount(){return static_cast<DWORD>(now);}
unsigned long long GetTickCount64(){return now;}
'''
RESPAWN_ENV = r'''
enum {enumCHA_TIMEER_ENTERMAP=1,enumEXISTS_RESUMEING=2,enumEPLAYER_RELIVE_NONE=0,
 enumEXISTS_NATALITY=3,enumSWITCHMAP_DIE=1};
struct SFireUnit;
struct CCharacter;
struct SubMap {unsigned long long generation=1;unsigned long long GetRuntimeGeneration(){return generation;}
int outs=0,ins=0;bool accept=true;void GoOut(CCharacter*);int GetCopyNO(){return 0;}};
struct Entity {
 virtual ~Entity()=default;
 uint32_t id=1; int handle=1; unsigned long long generation=1; bool valid=true;
 uint32_t GetID()const{return id;} int GetHandle(){return handle;}
 unsigned long long GetLifeGeneration()const{return generation;}
 void AdvanceLifeGeneration(){++generation;}
 virtual SubMap*GetSubMap()const{return nullptr;}
 virtual CCharacter*IsCharacter(){return nullptr;}
};
struct Point {int x=0,y=0;};
struct SSwitchMapInfo {SubMap*pSrcMap;char szTarMapName[32];Point STarPos;};
struct CCharacter:Entity {
 uint32_t life=0;SubMap*map=nullptr;bool inqueue=true,player=false;int resets=0,m_chSelRelive=0;
 void RangeEffect(SFireUnit*,SubMap*,Long*);
 CCharacter*IsCharacter()override{return this;}
 bool IsPlayerCha(){return player;} uint32_t GetLifeTime(){return life;}
 SubMap*GetSubMap()const override{return map;}
 bool IsLiveing(){return valid&&map;}
 const char*GetName(){return "test";}
 std::vector<int>executed;int moves=0;std::function<void()>callback;
 void Cmd_BeginMove(Short,Point*,Char,Char=0){moves++;}
 void Cmd_BeginSkillDirect(Long skill,Entity*){executed.push_back(skill);auto cb=callback;if(cb)cb();}
 void Cmd_BeginSkillDirect2(Long skill,Long,Long,Long){executed.push_back(skill);auto cb=callback;if(cb)cb();}
void SetExistState(int){}void SetInOutMapQueue(bool v){inqueue=v;}
 void SwitchMap(SubMap*m,const char*,int,int,bool,int,int){if(m->accept){map=m;m->ins++;}}
 void ResetAIState(){resets++;}void Free(){valid=false;map=nullptr;}
};
void SubMap::GoOut(CCharacter*c){outs++;c->map=nullptr;}
struct App {
 DWORD m_dwRunCnt=1;void WorldNotice(char*){}
 std::vector<Entity*>entities;
 Entity*IsLiveingEntity(uint32_t id,int handle){auto*e=IsValidEntity(id,handle);return e&&e->GetSubMap()?e:nullptr;}
 Entity*IsValidEntity(uint32_t id,int handle){for(auto*e:entities)if(e->valid&&e->id==id&&e->handle==handle)return e;return nullptr;}
} app;
App*g_pGameApp=&app;
'''
RESPAWN_TEST = r'''
int main(){
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  now=1990;q.Add(&c,1,&si,1,5000,5000);now=6000;q.Run(6000);CHECK(m.outs==0);
  now=6990;q.Run(6990);CHECK(m.outs==1&&m.ins==0);now=11989;q.Run(11989);CHECK(m.ins==0);
  now=11990;q.Run(11990);CHECK(m.ins==1&&!c.inqueue&&c.generation==2);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;c.life=900000;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,10000);q.Run(0);CHECK(!c.valid&&!c.inqueue&&m.ins==0);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,0);c.id=99;q.Drop();CHECK(m.outs==0&&m.ins==0);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,0);c.AdvanceLifeGeneration();q.Run(0);CHECK(m.outs==0);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,0);q.Run(0);CHECK(m.outs==1&&m.ins==1&&c.valid);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,5000,5000);now=20000;q.Run(20000);CHECK(m.ins==1);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,-1);q.Run(0);CHECK(!c.valid&&m.ins==0);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,1000,1000);q.Drop();CHECK(c.valid&&c.map==&m&&m.ins==1);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;m.accept=false;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,0);q.Run(0);CHECK(!c.valid);}
 {now=0;COutMapCha q;SubMap m,other;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,0);c.map=&other;q.Run(0);CHECK(c.valid&&c.map==&other&&!c.inqueue&&m.outs==0);}
 {now=0xffffffffULL-10;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,20,20);now+=40;q.Run(static_cast<DWORD>(now));CHECK(m.ins==1);}
 {now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,-1,0);now=100000;q.Run(100000);CHECK(c.valid&&m.outs==0&&!c.inqueue);}
 {for(int i=0;i<10000;i++){now=0;COutMapCha q;SubMap m;CCharacter c;c.map=&m;c.life=10000;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};q.Add(&c,1,&si,1,0,0);q.Run(0);CHECK(!c.valid);}}
 {now=0;COutMapCha q;SubMap m;std::vector<CCharacter>cs(300);app.entities.clear();SSwitchMapInfo si{&m,"test",{}};
  for(int i=0;i<300;i++){cs[i].map=&m;cs[i].id=i+1;cs[i].handle=i+1;app.entities.push_back(&cs[i]);q.Add(&cs[i],i+1,&si,1,0,0);}q.Run(0);CHECK(m.outs+m.ins==256);for(int i=0;i<4;i++)q.Run(0);CHECK(m.outs==300&&m.ins==300);}
 {now=0;COutMapCha q;SubMap m,other;CCharacter c;c.map=&m;app.entities={&c};SSwitchMapInfo si{&m,"test",{}};
  q.Add(&c,1,&si,1,0,1000);q.Run(0);c.map=&other;now=1000;q.Run(1000);CHECK(c.valid&&c.map==&other&&m.ins==0&&!c.inqueue);}
 std::cout<<"respawn: 15 scenarios passed (including 10000 temporary lifecycles)\n";
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix='mob-runtime-') as tmp:
        folder=Path(tmp)
        header=between(read('SubMap.h'),'class COutMapCha\n','class SubMap\n')
        impl=read('SubMap.cpp')[read('SubMap.cpp').index('COutMapCha::COutMapCha'):]
        compile_run('respawn',COMMON+RESPAWN_ENV+header+impl+RESPAWN_TEST,folder)
        cases = Path(__file__).parent
        action_header = between(read('Action.h'), 'class CActionCache\n', '#endif')
        action_impl = read('Action.cpp')[read('Action.cpp').index('CActionCache::CActionCache'):]
        constants = '#define defMAX_CACHE_ACTION_PARAM_LEN 30\nenum {enumCACHEACTION_MOVE,enumCACHEACTION_SKILL,enumCACHEACTION_SKILL2};\n'
        compile_run('actions',COMMON+RESPAWN_ENV+constants+action_header+action_impl+(cases/'actions_cases.cpp.inc').read_text(),folder)
        compile_run('hate',COMMON+RESPAWN_ENV+read('HarmRec.h')+(cases/'hate_cases.cpp.inc').read_text(),folder)
        circle = between(read('Range.h'), 'class CRangeCircle :', 'class CRangeSquare :')
        base = 'class CRangeBase{protected:int _x=0,_y=0,_nRadius=0;public:void setPos(int x,int y){_x=x;_y=y;}void setRadius(int r){_nRadius=r;}virtual BOOL PointHitTest(int,int){return 0;}virtual BOOL RectHitTest(int,int,int,int){return 0;}};\n'
        compile_run('geometry',COMMON+base+circle+(cases/'range_cases.cpp.inc').read_text(),folder)
        query_header = between(read('SubMap.h'), 'class CCharacterRangeQuery\n', 'class SubMap\n')
        query_impl = between(read('SubMap.cpp'), 'CCharacter* CCharacterRangeQuery::Next()', 'void SubMap::BeginSearchInRange(')
        iterator = between(read('SubMap.cpp'), 'CCharacter* SubMap::GetNextCharacterInRange(void)', 'bool SubMap::RangeAddState(')
        compile_run('queries',COMMON+base+circle+(cases/'query_fixture.cpp.inc').read_text()+query_header+(cases/'query_map.cpp.inc').read_text()+query_impl+iterator+(cases/'query_cases.cpp.inc').read_text(),folder)
        fire = between(read('FightAble.h'), 'struct SFireUnit\n', 'struct SFightInit\n')
        timed_header = between(read('FightAble.h'), 'class\tCTimeSkillMgr\n', 'extern CTimeSkillMgr')
        timed_impl = read('FightAble.cpp')[read('FightAble.cpp').index('CTimeSkillMgr::CTimeSkillMgr'):]
        timed_env = 'using CFightAble=CCharacter;struct CSkillRecord{};struct CSkillTempData{};\n#define defSKILL_RANGE_BASEP_NUM 3\n#define THROW_EXCP(...) std::abort()\nPoint g_SSkillPoint;uint32_t g_ulCurID;Long g_lCurHandle;enum {defINVALID_CHA_ID=0,defINVALID_CHA_HANDLE=0};\n'
        compile_run('timed',COMMON+RESPAWN_ENV+timed_env+fire+timed_header+timed_impl+(cases/'timed_cases.cpp.inc').read_text(),folder)




if __name__ == '__main__': main()

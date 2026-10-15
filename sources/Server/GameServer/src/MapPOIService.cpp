#include "stdafx.h"
#include "MapPOIService.h"
#include "SubMap.h"
#include "Character.h"
#include "Item.h"
#include "Player.h"
#include "EventRecord.h"
#include "EventEntity.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <mutex>

namespace {
struct Type {map_poi::Kind kind;unsigned radius;};
struct Config {
    std::map<unsigned,Type> types;
    std::set<std::string> worlds;
    Config() {
        std::ifstream in("resource/map_poi_types.txt");std::string line;
        while(std::getline(in,line)) {
            std::istringstream row(line);unsigned id,kind,radius;
            if(row>>id>>kind>>radius && id>0 && kind>=1 && kind<=3 && radius>=100 && radius<=5000)
                types.emplace(id,Type{static_cast<map_poi::Kind>(kind),radius});
        }
        std::ifstream maps("resource/map_poi_worlds.txt");
        while(std::getline(maps,line)) {
            std::istringstream row(line);std::string name;
            if(row>>name && name.size()<=31 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==std::string::npos)
                worlds.insert(name);
        }
        LG("map_poi","Loaded %u point types and %u persistent maps\n",unsigned(types.size()),unsigned(worlds.size()));
    }
};
const Config& Settings(){static const Config config;return config;}
std::uint64_t Epoch() {
    static const std::uint64_t seed=[](){std::random_device r;return (std::uint64_t(r())<<32)^r();}();
    static std::atomic<std::uint64_t> counter{0};return seed+(++counter);
}
std::pair<unsigned long,long> Key(Entity* e){return {e->GetID(),e->GetHandle()};}
std::string Label(const char* text) {
    std::string out;
    if(text) for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p && out.size()<map_poi::MaxName;++p)
        if(*p>=32 && *p!=127)out.push_back(static_cast<char>(*p));
    return out;
}
bool Classify(Entity* e,Type& type,unsigned& templateId,bool& temporary) {
    temporary=false;
    if(e->GetEvent().GetExecType()==enumEVENTE_SMAP_ENTRY || e->GetEvent().GetExecType()==enumEVENTE_DMAP_ENTRY) {
        type={map_poi::Kind::Portal,2000};templateId=e->GetEvent().GetID();
        temporary=e->GetEvent().GetExecType()==enumEVENTE_DMAP_ENTRY;return true;
    }
    if(auto* event=e->IsEvent();event && event->GetType()==mission::TRANSIT_ENTITY) {
        type={map_poi::Kind::Portal,2000};templateId=e->GetCat();return true;
    }
    auto* cha=e->IsCharacter();if(!cha || cha->IsPlayerOwnCha())return false;
    templateId=cha->GetMapPOITemplateID();
    auto i=Settings().types.find(templateId);if(i==Settings().types.end())return false;
    type=i->second;return true;
}
}
MapPOIService::MapPOIService(SubMap* map):owner(map),epoch(Epoch()){}
MapPOIService::~MapPOIService(){Flush();}
void MapPOIService::Load() {
    if(loaded)return;
    loaded=true;
    persistent=owner->GetCopyNO()==0 && Settings().worlds.count(owner->GetName())!=0;
    if(!persistent)return;
    statePath=std::string("resource/map_poi_discoveries/")+owner->GetName()+".txt";
    std::ifstream in(statePath);std::string line;
    // Bound work on damaged/local files. No client controls these files.
    for(unsigned rows=0;rows<16384 && std::getline(in,line);++rows) {
        if(line.size()!=16 || line.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)continue;
        std::uint64_t id=0;std::istringstream row(line);row>>std::hex>>id;if(id)saved.insert(id);
    }
}
void MapPOIService::Flush() {
    if(!dirty || !persistent)return;
    std::error_code error;std::filesystem::create_directories("resource/map_poi_discoveries",error);
    if(error){LG("map_poi","Cannot create discovery directory: %s\n",error.message().c_str());return;}
    std::ostringstream out;for(auto id:saved)out<<std::hex<<std::setw(16)<<std::setfill('0')<<id<<'\n';
    const auto data=out.str(),temp=statePath+".tmp";
    HANDLE file=CreateFileA(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){LG("map_poi","Cannot open discovery file for %s (error %lu)\n",owner->GetName(),GetLastError());return;}
    DWORD written=0;
    const bool ok=WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&written,nullptr) &&
        written==data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if(ok && MoveFileExA(temp.c_str(),statePath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))dirty=false;
    else LG("map_poi","Cannot persist discoveries for %s (error %lu); will retry\n",owner->GetName(),GetLastError());
}
void MapPOIService::Unindex(const std::pair<unsigned long,long>& key) {
    auto p=buckets.find(key);if(p==buckets.end())return;
    auto cell=hidden.find(p->second);if(cell!=hidden.end()){cell->second.erase(key);if(cell->second.empty())hidden.erase(cell);}
    buckets.erase(p);
}
void MapPOIService::Register(Entity* e) {
    if(!e || e->GetSubMap()!=owner)return;
    Type type;unsigned templateId;bool temporary;
    if(!Classify(e,type,templateId,temporary))return;
    Load();
    if(registry.Size()>=map_poi::MaxMarkers && registry.CompactRemoved())epoch=Epoch();
    const auto key=Key(e);Unindex(key);
    const auto pos=e->GetPos();
    std::uint64_t id=e->GetMapPOIOwner()==owner?e->GetMapPOIId():0;
    if(!id || !registry.Find(id) || (temporary && registry.Find(id)->marker.state==map_poi::State::Removed)) {
        // Static spawn coordinates identify a point, not all entities of a type.
        std::uint32_t slot=temporary?static_cast<std::uint32_t>(++serial):0;
        do {
            id=map_poi::SpawnId(owner->GetName(),temporary?2:1,templateId,pos.x,pos.y,slot++);
            auto* old=registry.Find(id);
            if(!old || (!temporary && old->marker.state!=map_poi::State::Active &&
                old->marker.kind==type.kind && old->marker.templateId==templateId))break;
        } while(slot<map_poi::MaxMarkers+serial+1);
    }
    map_poi::Marker m{id,type.kind,map_poi::State::Active,static_cast<std::int32_t>(pos.x),static_cast<std::int32_t>(pos.y),templateId,Label(type.kind==map_poi::Kind::Portal && e->GetEvent().GetName()[0]?e->GetEvent().GetName():e->GetName())};
    if(m.name.empty())m.name=type.kind==map_poi::Kind::Portal?"Portal":"Point";
    if(registry.Find(id)) registry.Update(m);
    else if(!registry.Add(m,persistent && !temporary && saved.count(id)!=0)) {
        LG("map_poi","Point capacity/identity conflict in %s copy %d; template %u not registered\n",owner->GetName(),owner->GetCopyNO(),templateId);return;
    }
    e->SetMapPOI(owner,id);
    bindings[key]={e->GetID(),e->GetHandle(),e->GetLifeGeneration(),id,temporary};
    const auto* entry=registry.Find(id);
    if(entry && !entry->known) {
        const auto bucket=std::make_pair(int(pos.x/5000),int(pos.y/5000));
        hidden[bucket].insert(key);buckets[key]=bucket;
    }
}
void MapPOIService::Remove(Entity* e) {
    if(!e)return;
    const auto key=Key(e);Unindex(key);
    auto i=bindings.find(key);if(i==bindings.end())return;
    auto* record=registry.Find(i->second.marker);
    if(record) {
        auto m=record->marker;
        m.state=i->second.temporary?map_poi::State::Removed:map_poi::State::Inactive;
        registry.Update(m);
    }
    bindings.erase(i);
}
void MapPOIService::Observe(CCharacter* observer,Entity* target) {
    if(!observer || !target || !observer->IsPlayerFocusCha() || !observer->IsLiveing() ||
        observer->GetSubMap()!=owner || target->GetSubMap()!=owner ||
        !observer->GetActControl(enumACTCONTROL_EYESHOT))return;
    auto i=bindings.find(Key(target));if(i==bindings.end())return;
    if(i->second.life!=target->GetLifeGeneration())return;
    auto* entry=registry.Find(i->second.marker);
    if(!entry || entry->known || !target->IsLiveing())return;
    if(auto* cha=target->IsCharacter();cha && !cha->CanSeen(observer))return;
    Type type;unsigned templateId;bool temporary;
    if(!Classify(target,type,templateId,temporary))return;
    const auto a=observer->GetPos(),b=target->GetPos();
    if(!map_poi::Near(a.x,a.y,b.x,b.y,type.radius))return;
    const auto id=i->second.marker;
    if(registry.Discover(id)) {
        Unindex(Key(target));
        if(persistent && !temporary){saved.insert(id);dirty=true;}
    }
}
void MapPOIService::CheckNearby(CCharacter* observer) {
    if(!observer || !observer->IsPlayerFocusCha() || observer->GetSubMap()!=owner)return;
    const auto pos=observer->GetPos();
    std::vector<std::pair<unsigned long,long>> candidates;
    for(int y=int(pos.y/5000)-1;y<=int(pos.y/5000)+1;++y)
        for(int x=int(pos.x/5000)-1;x<=int(pos.x/5000)+1;++x) {
            auto cell=hidden.find({x,y});if(cell!=hidden.end())candidates.insert(candidates.end(),cell->second.begin(),cell->second.end());
        }
    for(const auto& key:candidates) {
        auto* e=g_pGameApp->IsValidEntity(key.first,key.second);if(e)Observe(observer,e);
    }
}
void MapPOIService::Run() {
    const auto now=GetTickCount64();if(now<nextRun)return;nextRun=now+1000;
    for(auto i=bindings.begin();i!=bindings.end();) {
        const auto pair=*i++;
        const auto& binding=pair.second;
        auto* e=g_pGameApp->IsValidEntity(binding.world,binding.handle);
        auto* record=registry.Find(binding.marker);
        if(!e || e->GetLifeGeneration()!=binding.life || e->GetSubMap()!=owner) {
            if(record) {
                auto m=record->marker;
                m.state=binding.temporary?map_poi::State::Removed:map_poi::State::Inactive;
                registry.Update(m);
            }
            Unindex(pair.first);bindings.erase(pair.first);continue;
        }
        if(!record)continue;
        auto m=record->marker;const auto pos=e->GetPos();
        m.x=pos.x;m.y=pos.y;m.state=e->IsLiveing()?map_poi::State::Active:map_poi::State::Inactive;
        registry.Update(m);
        if(!record->known) {
            const auto bucket=std::make_pair(int(pos.x/5000),int(pos.y/5000));
            auto old=buckets.find(pair.first);
            if(old==buckets.end() || old->second!=bucket) {
                Unindex(pair.first);hidden[bucket].insert(pair.first);buckets[pair.first]=bucket;
            }
        }
    }
    Flush();
}
void MapPOIService::Reset() {
    Flush();
    const auto retained=bindings;
    registry.Clear();bindings.clear();hidden.clear();buckets.clear();
    epoch=Epoch();serial=0;nextRun=0;
    // Some scripts retain NPCs/items when closing a copy. Re-seed them unknown.
    for(const auto& pair:retained) {
        const auto& b=pair.second;auto* e=g_pGameApp->IsValidEntity(b.world,b.handle);
        if(e && e->GetLifeGeneration()==b.life && e->GetSubMap()==owner) {
            e->SetMapPOI(nullptr,0);Register(e);
        }
    }
}
void MapPOIService::Send(CCharacter* observer,std::uint32_t context,std::uint64_t knownEpoch,std::uint32_t revision) {
    if(!observer || !observer->IsPlayerFocusCha() || observer->GetSubMap()!=owner)return;
    CheckNearby(observer);Run();
    map_poi::Message message;message.map=owner->GetName();message.copy=owner->GetCopyNO();
    message.context=context;message.epoch=epoch;message.revision=registry.Revision();
    message.full=knownEpoch!=epoch || revision>message.revision;
    message.markers=registry.Changes(revision,message.full);
    const auto bytes=map_poi::Encode(message);if(bytes.empty() || bytes.size()>65535)return;
    WPACKET packet=GETWPACKET();WRITE_CMD(packet,CMD_MC_MAP_POI);
    WRITE_SEQ(packet,reinterpret_cast<const char*>(bytes.data()),static_cast<dbc::uShort>(bytes.size()));
    observer->ReflectINFof(observer,packet);
}

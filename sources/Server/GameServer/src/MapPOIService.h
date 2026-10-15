#pragma once
#include "MapPOI.h"
#include <set>
class SubMap;
class Entity;
class CCharacter;
class MapPOIService {
public:
    explicit MapPOIService(SubMap* owner);
    ~MapPOIService();
    void Register(Entity* entity);
    void Remove(Entity* entity);
    void Observe(CCharacter* observer,Entity* target);
    void CheckNearby(CCharacter* observer);
    void Run();
    void Reset();
    void Send(CCharacter* observer,std::uint32_t context,std::uint64_t epoch,std::uint32_t revision);
private:
    struct Binding {unsigned long world=0;long handle=0;unsigned long long life=0;std::uint64_t marker=0;bool temporary=false;};
    SubMap* owner;
    map_poi::Registry registry;
    std::map<std::pair<unsigned long,long>,Binding> bindings;
    std::map<std::pair<int,int>,std::set<std::pair<unsigned long,long>>> hidden;
    std::map<std::pair<unsigned long,long>,std::pair<int,int>> buckets;
    std::set<std::uint64_t> saved;
    std::uint64_t epoch=0,nextRun=0,serial=0;
    bool loaded=false,persistent=false,dirty=false;
    std::string statePath;
    void Load();
    void Flush();
    void Unindex(const std::pair<unsigned long,long>& key);
};

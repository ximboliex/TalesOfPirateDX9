struct SubMap;
struct Point{int x,y;};
struct CCharacter;
struct Entity {
 uint32_t id;int handle;unsigned long long gen=1;SubMap*map=nullptr;bool valid=true;
 virtual ~Entity()=default;uint32_t GetID(){return id;}int GetHandle(){return handle;}
 unsigned long long GetLifeGeneration(){return gen;}SubMap*GetSubMap(){return map;}
 virtual CCharacter*IsCharacter(){return nullptr;}
};
struct CCharacter:Entity {
 Point pos;bool hidden=false;Point GetPos(){return pos;}bool IsHide(){return hidden;}
 CCharacter*IsCharacter()override{return this;}
};
struct App{std::vector<Entity*>entities;Entity*IsValidEntity(uint32_t id,int handle){for(auto*e:entities)if(e->valid&&e->id==id&&e->handle==handle)return e;return nullptr;}}app;
App*g_pGameApp=&app;

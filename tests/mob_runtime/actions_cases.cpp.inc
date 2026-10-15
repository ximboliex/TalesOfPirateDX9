int main() {
 SubMap m, other; CCharacter owner,target;owner.map=target.map=&m;target.id=target.handle=2;app.entities={&owner,&target};
 CActionCache cache(&owner);
 auto coord=[&](Long skill){Long level=1,x=100,y=200;cache.AddCommand(enumCACHEACTION_SKILL2);cache.PushParam(&skill,sizeof(skill));cache.PushParam(&level,sizeof(level));cache.PushParam(&x,sizeof(x));cache.PushParam(&y,sizeof(y));};
 auto entity=[&](Long skill){auto id=target.GetID();Long h=target.GetHandle();auto gen=target.GetLifeGeneration();cache.AddCommand(enumCACHEACTION_SKILL);cache.PushParam(&skill,sizeof(skill));cache.PushParam(&id,sizeof(id));cache.PushParam(&h,sizeof(h));cache.PushParam(&gen,sizeof(gen));};
 coord(11);coord(22);cache.Run();CHECK((owner.executed==std::vector<int>{11,22}));owner.executed.clear();
 coord(33);cache.Clear();cache.Run();CHECK(owner.executed.empty());
 coord(44);owner.AdvanceLifeGeneration();cache.Run();CHECK(owner.executed.empty());
 entity(55);target.AdvanceLifeGeneration();cache.Run();CHECK(owner.executed.empty());
 entity(55);target.map=&other;cache.Run();CHECK(owner.executed.empty());target.map=&m;
 entity(55);target.valid=false;cache.Run();CHECK(owner.executed.empty());target.valid=true;
 entity(66);cache.Run();CHECK((owner.executed==std::vector<int>{66}));owner.executed.clear();
 owner.callback=[&](){cache.Clear();};coord(77);coord(88);cache.Run();CHECK((owner.executed==std::vector<int>{77}));owner.callback=nullptr;owner.executed.clear();
 cache.AddCommand(enumCACHEACTION_SKILL2);Long skill=99;cache.PushParam(&skill,sizeof(skill));cache.Run();CHECK(owner.executed.empty());
 cache.AddCommand(enumCACHEACTION_MOVE);Short ping=0;Char n=127;cache.PushParam(&ping,sizeof(ping));cache.PushParam(&n,sizeof(n));cache.Run();CHECK(owner.executed.empty());
 cache.AddCommand(enumCACHEACTION_MOVE);n=2;Point path[2]{{1,2},{3,4}};cache.PushParam(&ping,sizeof(ping));cache.PushParam(&n,sizeof(n));cache.PushParam(path,sizeof(path));cache.Run();CHECK(owner.moves==1);
 owner.callback=[&](){owner.callback=nullptr;coord(111);};coord(100);cache.Run();CHECK((owner.executed==std::vector<int>{100}));cache.Run();CHECK((owner.executed==std::vector<int>{100,111}));
 std::cout<<"actions: 12 scenarios passed\n";
}

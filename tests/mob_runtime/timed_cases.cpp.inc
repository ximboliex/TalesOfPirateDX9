void CCharacter::RangeEffect(SFireUnit*fire,SubMap*,Long*){executed.push_back(fire->chSkillLevel);}
int main(){
 SubMap m,other;CCharacter source;source.map=&m;app.entities={&source};Point point{};Long range[3]{};
 auto fire=[&](){SFireUnit f{};f.pCFightSrc=&source;f.ulID=source.id;f.lSourceHandle=source.handle;f.ullSourceGeneration=source.generation;f.ullMapGeneration=m.GetRuntimeGeneration();f.chSkillLevel=5;return f;};
 {now=0;CTimeSkillMgr mgr;now=990;auto f=fire();mgr.Add(&f,100,&m,&point,range);now=1000;mgr.Run(1000);CHECK(source.executed.empty());now=2000;mgr.Run(2000);CHECK((source.executed==std::vector<int>{5}));source.executed.clear();}
 {now=0;CTimeSkillMgr mgr(0);auto f=fire();mgr.Add(&f,10,&m,&point,range);source.generation++;now=10;mgr.Run(10);CHECK(source.executed.empty());}
 {now=0;CTimeSkillMgr mgr(0);auto f=fire();mgr.Add(&f,10,&m,&point,range);source.map=&other;now=10;mgr.Run(10);CHECK(source.executed.empty());source.map=&m;}
 {now=0;CTimeSkillMgr mgr(0);auto f=fire();mgr.Add(&f,10,&m,&point,range);m.generation++;now=10;mgr.Run(10);CHECK(source.executed.empty());}
 {now=0;CTimeSkillMgr mgr(0);auto f=fire();mgr.Add(&f,10,&m,&point,range);source.valid=false;now=10;mgr.Run(10);CHECK(source.executed.empty());source.valid=true;}
 {now=0;CTimeSkillMgr mgr(0);auto f=fire();for(int i=0;i<300;i++)mgr.Add(&f,0,&m,&point,range);mgr.Run(0);CHECK(source.executed.size()==256);mgr.Run(0);CHECK(source.executed.size()==300);source.executed.clear();}
 std::cout<<"timed skills: 6 scenarios passed\n";
}

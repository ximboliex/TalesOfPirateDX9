int main(){
 SubMap map,other;CCharacter a,b,c;a.id=a.handle=1;b.id=b.handle=2;c.id=c.handle=3;a.pos={100,100};b.pos={205,100};c.pos={350,100};a.map=b.map=c.map=&map;app.entities={&a,&b,&c};Node nc{&c,nullptr},nb{&b,&nc},na{&a,nullptr};map.cells[0].m_pCChaIn=&na;map.cells[1].m_pCChaIn=&nb;
 Long base[]{100,100,0},extent[]{0,120};
 auto q=map.QueryCharactersInRange(base,extent);CHECK(q.Next()==&a);
 Long nestedBase[]{350,100,0},nestedExtent[]{0,1};auto nested=map.QueryCharactersInRange(nestedBase,nestedExtent);CHECK(nested.Next()==&c);CHECK(q.Next()==&b);CHECK(q.Next()==nullptr);
 q=map.QueryCharactersInRange(base,extent);CHECK(q.Next()==&a);++b.gen;CHECK(q.Next()==nullptr);
 q=map.QueryCharactersInRange(base,extent);++map.generation;CHECK(q.Next()==nullptr);
 q=map.QueryCharactersInRange(base,extent);CHECK(q.Next()==&a);b.map=&other;CHECK(q.Next()==nullptr);b.map=&map;
 q=map.QueryCharactersInRange(base,extent);a.valid=false;CHECK(q.Next()==&b);a.valid=true;
 b.hidden=true;q=map.QueryCharactersInRange(base,extent);CHECK(q.Next()==&a);CHECK(q.Next()==nullptr);
 q=map.QueryCharactersInRange(base,extent,true);CHECK(q.Next()==&a);CHECK(q.Next()==&b);CHECK(q.Next()==nullptr);
 std::cout<<"queries: 7 scenarios passed (nested searches, exact area, identity, instance, visibility)\n";
}

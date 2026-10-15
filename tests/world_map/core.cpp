#include "MapPOI.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <random>
using namespace map_poi;
Marker point(std::uint64_t id,Kind kind=Kind::Chest){return {id,kind,State::Active,10000,20000,712,"Chest"};}
int main(){
 Registry r;assert(r.Add(point(1,Kind::Boss)));assert(r.Add(point(2,Kind::MiniBoss)));
 assert(r.Add(point(3)));assert(r.Add(point(4,Kind::Portal)));
 assert(r.Revision()==2 && r.Changes(0,true).size()==2);
 assert(r.Discover(3));assert(!r.Discover(3));assert(r.Revision()==3);
 auto p=point(4,Kind::Portal);p.x=555;assert(r.Update(p));assert(r.Revision()==3);
 assert(r.Changes(2,false).size()==1);assert(r.Discover(4));
 p=point(3);p.state=State::Inactive;assert(r.Update(p));assert(!r.Discover(3));
 p=point(4,Kind::Portal);p.state=State::Removed;assert(r.Update(p));
 assert(r.Changes(0,true).size()==3);assert(r.Changes(4,false).back().state==State::Removed);
 assert(r.CompactRemoved());assert(r.Size()==3 && !r.Find(4));
 p=point(3,Kind::Boss);assert(!r.Update(p));assert(r.Find(3)->marker.kind==Kind::Chest);
 Registry cap;for(unsigned n=1;n<=MaxMarkers;++n)assert(cap.Add(point(n)));
 assert(!cap.Add(point(513)));assert(!cap.Add(point(1)));
 assert(SpawnId("garner",1,712,1,2)!=SpawnId("garner",1,712,1,3));
 assert(SpawnId("garner",1,712,1,2)!=SpawnId("garner",1,712,1,2,1));
 assert(SpawnId("garner",1,712,1,2)!=SpawnId("darkblue",1,712,1,2));
 assert(Near(0,0,300,400,500));assert(!Near(0,0,300,401,500));
 assert(!Near(INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX,5000));
 std::mt19937 random(42);
 for(int n=0;n<20000;++n){
  Projection t{73,51,526,420,double(random()%2000000000),double(random()%2000000000),(1+random()%8)/125.0};
  const double x=double(random()%2000000000),y=double(random()%2000000000);double sx,sy,wx,wy;
  t.Screen(x,y,sx,sy);assert(t.World(sx,sy,wx,wy));assert(std::abs(wx-x)<0.00001 && std::abs(wy-y)<0.00001);
  assert(t.Contains(74,52));assert(!t.Contains(599,471));assert(!t.Contains(74,52,7));
 }
 Projection bad;double x,y;assert(!bad.World(0,0,x,y));bad.pixelsPerCm=std::numeric_limits<double>::infinity();assert(!bad.World(0,0,x,y));
 Message m;m.map="garner";m.context=2;m.epoch=UINT64_C(0xfedcba9876543210);m.revision=17;m.copy=22;m.markers={point(UINT64_C(0x89abcdef01234567))};
 auto bytes=Encode(m);assert(!bytes.empty());assert(bytes[0]==1 && bytes[1]==6 && bytes[14]==0xfe);
 Message decoded;assert(Decode(bytes.data(),bytes.size(),decoded));assert(decoded.markers==m.markers && decoded.epoch==m.epoch && decoded.copy==22);
 Message sentinel;sentinel.map="unchanged";
 for(std::size_t n=0;n<bytes.size();++n){assert(!Decode(bytes.data(),n,sentinel));assert(sentinel.map=="unchanged");}
 auto reject=[&](std::vector<std::uint8_t> b){assert(!Decode(b.data(),b.size(),sentinel));assert(sentinel.map=="unchanged");};
 auto b=bytes;b[0]=2;reject(b);b=bytes;b.push_back(0);reject(b);
 // Header: version/text/copy/context/epoch/revision/full/count; marker starts at 29.
 const std::size_t start=29;
 assert(bytes[start]==0x89);
 for(auto change:std::vector<std::pair<std::size_t,unsigned>>{{start+8,0},{start+8,5},{start+9,0},{start+9,4},{start+22,49},{26,2},{27,3},{start+10,255},{2,0}}){b=bytes;b[change.first]=std::uint8_t(change.second);reject(b);}
 m.markers.push_back(m.markers.front());b=Encode(m);reject(b);m.markers.pop_back();
 m.markers[0].name=std::string(49,'a');assert(Encode(m).empty());m.markers[0]=point(1);
 ClientStore client;client.Reset("garner");m.context=client.context;m.copy=0;m.revision=1;m.epoch=100;
 assert(client.Apply(m));assert(client.points.size()==1);
 auto late=m;late.context--;assert(!client.Apply(late));late=m;late.map="darkblue";assert(!client.Apply(late));
 late=m;late.full=false;late.epoch=99;assert(!client.Apply(late));late=m;late.revision=0;assert(!client.Apply(late));
 late=m;late.copy=1;late.full=false;assert(!client.Apply(late));
 late=m;late.full=false;late.revision=2;late.markers[0].state=State::Removed;assert(client.Apply(late));assert(client.points.empty());
 m.epoch=101;m.revision=0;assert(client.Apply(m));assert(client.points.size()==1);
 client.Reset("garner");assert(client.points.empty());assert(!client.Apply(m));
 // Decoder fuzzing: random/truncated frames must never publish partial output.
 for(int n=0;n<20000;++n){auto fuzz=bytes;for(int j=0;j<1+n%6;++j)fuzz[random()%fuzz.size()]=std::uint8_t(random());Message out;out.map="sentinel";if(!Decode(fuzz.data(),fuzz.size(),out))assert(out.map=="sentinel");}
 std::cout<<"PASS: registry, secrecy, limits, projection (20000), codec truncation/fuzz (20000), stale contexts\n";
}

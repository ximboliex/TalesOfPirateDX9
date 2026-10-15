map_poi::Message receive(MapPOIService& service,CCharacter& player,std::uint64_t epoch=0,std::uint32_t revision=0){
 service.Send(&player,2,epoch,revision);map_poi::Message m;
 assert(map_poi::Decode(player.last.data.data(),player.last.data.size(),m));return m;
}
int main(){
 SubMap map;MapPOIService s(&map);
 CCharacter player;player.map=&map;player.player=player.focus=true;player.world=100;
 CCharacter chest;chest.map=&map;chest.name="Secret Chest";
 CCharacter chest2;chest2.map=&map;chest2.world=2;chest2.pos.x+=5000;
 CCharacter boss;boss.map=&map;boss.world=3;boss.templateId=679;
 CCharacter mini;mini.map=&map;mini.world=4;mini.templateId=999;
 app.entities={&player,&chest,&chest2,&boss,&mini};
 s.Register(&chest);s.Register(&chest2);s.Register(&boss);s.Register(&mini);
 assert(chest.poiId && chest.poiId!=chest2.poiId);
 assert(s.registry.Changes(0,true).size()==2);
 player.pos.x=50000;assert(receive(s,player).markers.size()==2);
 player.pos=chest.pos;s.CheckNearby(&player);
 assert(s.registry.Find(chest.poiId)->known && !s.registry.Find(chest2.poiId)->known);
 const auto revision=s.registry.Revision();s.CheckNearby(&player);assert(s.registry.Revision()==revision);
 CCharacter other=player;other.world=101;assert(receive(s,other).markers.size()==3);
 player.eyes=false;player.pos=chest2.pos;s.CheckNearby(&player);assert(!s.registry.Find(chest2.poiId)->known);
 player.eyes=true;chest2.visible=false;s.CheckNearby(&player);assert(!s.registry.Find(chest2.poiId)->known);
 chest2.visible=true;chest2.living=false;s.CheckNearby(&player);assert(!s.registry.Find(chest2.poiId)->known);
 chest2.living=true;player.living=false;s.CheckNearby(&player);assert(!s.registry.Find(chest2.poiId)->known);
 player.living=true;chest2.life++;s.Observe(&player,&chest2);assert(!s.registry.Find(chest2.poiId)->known);chest2.life--;
 // Spatial index follows a hidden moving target across bucket boundaries.
 chest2.pos.x=100000;tick+=1001;s.Run();player.pos=chest2.pos;s.CheckNearby(&player);assert(s.registry.Find(chest2.poiId)->known);
 // A corpse update and a respawn preserve the point identity and discovery.
 s.Remove(&chest);const auto id=chest.poiId;s.Register(&chest);assert(chest.poiId==id && s.registry.Find(id)->known);
 const auto epoch=s.epoch;s.Reset();assert(s.epoch!=epoch && !s.registry.Find(chest.poiId)->known);
 assert(s.registry.Changes(0,true).size()==2);
 // Transit NPCs are portals even without an item event.
 mission::CEventEntity transit;transit.map=&map;transit.world=6;app.entities.push_back(&transit);s.Register(&transit);
 assert(s.registry.Find(transit.poiId)->marker.kind==map_poi::Kind::Portal);
 // Random portal openings do not inherit discovery, even at identical coordinates.
 Entity portal;portal.map=&map;portal.world=7;portal.event.type=enumEVENTE_DMAP_ENTRY;portal.event.id=33;app.entities.push_back(&portal);
 s.Register(&portal);player.pos=portal.pos;s.Observe(&player,&portal);const auto old=portal.poiId;assert(s.registry.Find(old)->known);
 s.Remove(&portal);++portal.life;s.Register(&portal);
 assert(portal.poiId!=old && !s.registry.Find(portal.poiId)->known);
 // More than 512 successive openings: reclaim tombstones and force a full snapshot.
 const auto oldEpoch=s.epoch;
 for(int n=0;n<1200;++n){s.Remove(&portal);portal.SetMapPOI(nullptr,0);++portal.life;s.Register(&portal);assert(s.registry.Find(portal.poiId));}
 assert(s.epoch!=oldEpoch && s.registry.Size()<=map_poi::MaxMarkers);
 auto full=receive(s,player,oldEpoch,s.registry.Revision());assert(full.full);
 auto delta=receive(s,player,full.epoch,full.revision);assert(!delta.full && delta.markers.empty());
 // Separate instances never share the in-memory registry.
 SubMap second;MapPOIService isolated(&second);CCharacter c;c.map=&second;c.world=8;app.entities.push_back(&c);isolated.Register(&c);assert(!isolated.registry.Find(c.poiId)->known);
 player.map=&second;isolated.Observe(&player,&chest);assert(!isolated.registry.Find(c.poiId)->known);
 // Generation invalidation cannot leave a permanently live marker or hidden bucket.
 const auto stale=c.poiId;++c.life;tick+=1001;isolated.Run();
 assert(isolated.registry.Find(stale)->marker.state==map_poi::State::Inactive);
 assert(isolated.bindings.empty() && isolated.buckets.empty());
 // Copy 0 permanent maps persist only static discoveries, including restart.
 SubMap world;world.name="garner";world.copy=0;CCharacter treasure;treasure.map=&world;treasure.world=9;app.entities.push_back(&treasure);player.map=&world;player.pos=treasure.pos;
 {MapPOIService permanent(&world);permanent.Register(&treasure);permanent.Observe(&player,&treasure);assert(permanent.dirty);tick+=1001;permanent.Run();assert(!permanent.dirty);}
 treasure.SetMapPOI(nullptr,0);
 {MapPOIService restored(&world);restored.Register(&treasure);assert(restored.registry.Find(treasure.poiId)->known);}
 std::cout<<"PASS: production service with adapters: discovery, visibility, generations, shared copies, persistence, 1200 portal openings\n";
}

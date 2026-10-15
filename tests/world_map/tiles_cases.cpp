int main(){
 {CLargerMap map;map.Show(true);assert(loads>0);const int first=loads;
  for(int n=0;n<100;++n)map.Update(100000+n,100000+n);
  assert(loads==first); // No file loads while staying within the same terrain tiles.
  map.Show(false);assert(alive.size()==1);
  map.Show(true);assert(loads>first && alive.size()>1); // Reopen reloads released textures.
  map.map_scale=8;map.Update(100000,100000);assert(map._pTex.size()<50);
  map.map_scale=1;map.Update(100000,100000);assert(map._pTex.size()>200);
  map.Update(-100,-100);assert(map._nCenterX==0 && map._nCenterY==0);
  map.Update(999999,999999);assert(map._nCenterX==409599 && map._nCenterY==409599);
  map.Show(false);
 }
 assert(alive.empty());std::cout<<"PASS: production tile update/show: cached pan, reopen, zoom, map bounds, balanced releases\n";
}

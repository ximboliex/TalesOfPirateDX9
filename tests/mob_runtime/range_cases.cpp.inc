int main(){
 CRangeCircle c;c.setPos(100,100);c.setRadius(120);CHECK(c.RectHitTest(200,0,399,199));CHECK(c.PointHitTest(205,100));CHECK(!c.PointHitTest(221,100));
 CHECK(c.RectHitTest(220,100,230,110));CHECK(!c.RectHitTest(221,100,230,110));CHECK(!c.RectHitTest(3,3,2,2));
 c.setPos(INT32_MIN,INT32_MIN);c.setRadius(INT32_MAX);CHECK(!c.PointHitTest(INT32_MAX,INT32_MAX));
 c.setPos(0,0);c.setRadius(INT32_MAX);CHECK(c.PointHitTest(INT32_MAX,0));CHECK(!c.PointHitTest(INT32_MAX,INT32_MAX));
 for(int r=0;r<=20;r++)for(int x=-30;x<=30;x++)for(int y=-30;y<=30;y++){c.setPos(0,0);c.setRadius(r);CHECK(bool(c.PointHitTest(x,y))==(int64_t(x)*x+int64_t(y)*y<=int64_t(r)*r));}
 std::cout<<"geometry: edge cases and 78141 point comparisons passed\n";
}

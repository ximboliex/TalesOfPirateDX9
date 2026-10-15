BOOL g_bLogHarmRec=FALSE;
int main(){
 SubMap m;CCharacter a;a.map=&m;app.entities={&a};CHateMgr h;h.ClearHarmRec();
 h.AddHarm(&a,40000,1);CHECK(h.GetHarmRec(0)->sHarm==40000);h.AddHarm(&a,100000,1);CHECK(h.GetHarmRec(0)->sHarm==140000);
 h.ClearHarmRec();h.AddHate(&a,10,1);h.AddHate(&a,-20,1);CHECK(h.GetHarmRec(0)->sHate==0);
 h.ClearHarmRec();h.AddHarm(&a,-1,1);CHECK(!h.GetCurTarget());
 h.AddHarm(&a,MAX_HATE_VALUE,1);h.AddHarm(&a,40000,1);CHECK(h.GetHarmRec(0)->sHarm==MAX_HATE_VALUE&&h.GetHarmRec(0)->sHate==MAX_HATE_VALUE);
 h.AddHate(&a,std::numeric_limits<long long>::min(),1);CHECK(h.GetHarmRec(0)->sHate==0);
 h.AddHate(&a,std::numeric_limits<long long>::max(),1);CHECK(h.GetHarmRec(0)->sHate==MAX_HATE_VALUE);
 SHarmRec b,c;b.sHate=c.sHate=10;CHECK(CompareHarm(&b,&c)==0&&CompareHarm(&c,&b)==0);
 CHECK(h.GetHarmRec(-1)==nullptr&&h.GetHarmRec(MAX_HARM_REC)==nullptr);
 h.ClearHarmRec();h.AddHarm(&a,10,1);a.AdvanceLifeGeneration();CHECK(!h.GetCurTarget());
 h.ClearHarmRec();h.AddHarm(&a,10,1);SubMap other;CCharacter self;self.map=&other;now=3000;h.UpdateHarmRec(&self);CHECK(!h.GetCurTarget());
 std::cout<<"aggro: 10 scenarios passed\n";
}

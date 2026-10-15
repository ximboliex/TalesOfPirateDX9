struct Node{CCharacter*m_pCCha;Node*m_pCNext;};
struct Cell{Node*m_pCChaIn=nullptr;};
struct SubMap{
 unsigned long long generation=1;unsigned long long GetRuntimeGeneration(){return generation;}
 CRangeCircle circle;CRangeBase*m_pCBaseRange=nullptr;int m_sRangeCurMgrUnit=-1,m_sRangeMgrUnitNum=0;
 bool m_bIncludeHideEnti=false;Node*m_pRangeCurEntiNode=nullptr;int m_lRangeMgrUnit[2][2];Cell cells[2];Cell*m_pCStateCell[1][2]{{&cells[0],&cells[1]}};
 void BeginSearchInRange(Long*base,Long*extent,bool hidden){circle.setPos(base[0],base[1]);circle.setRadius(extent[1]);m_pCBaseRange=&circle;m_bIncludeHideEnti=hidden;m_sRangeCurMgrUnit=-1;m_sRangeMgrUnitNum=0;m_pRangeCurEntiNode=nullptr;for(int x=0;x<2;x++)if(circle.RectHitTest(x*200,0,x*200+199,199)){m_lRangeMgrUnit[m_sRangeMgrUnitNum][0]=x;m_lRangeMgrUnit[m_sRangeMgrUnitNum++][1]=0;}}
 CCharacter*GetNextCharacterInRange();CCharacterRangeQuery QueryCharactersInRange(Long*,Long*,bool=false);
};

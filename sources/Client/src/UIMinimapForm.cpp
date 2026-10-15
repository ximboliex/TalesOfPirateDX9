#include "StdAfx.h"
#include "uiminimapform.h"
#include "uiForm.h"
#include "uicompent.h"
#include "uilabel.h"
#include "gameapp.h"
#include "SmallMap.h"
#include "uiformmgr.h"
#include "uimemo.h"
#include "ui3dcompent.h"
#include "scene.h"
#include "uiEdit.h"
#include "uiCombo.h"
#include "uiboothform.h"
#include "worldscene.h"
#include "PacketCmd.h"
#include "character.h"
#include "EffectObj.h"
#include "MapSet.h"
#include "UIStoreForm.h"
#include "UIDoublePwdForm.h"
#include "UIMisLogForm.h"//add by alfred.shi 20080722
#include "UIstartform.h"//add by alfred.shi 20080711

#include "AreaRecord.h"//add by sunny.sun 20080903
#include "MonsterSet.h" //add by mdr
#include <shellapi.h>
#include "UIRender.h"
#include "UIList.h"
#include <cmath>
#include "ProCirculate.h"
#include "uiBoatForm.h"
using namespace std;
using namespace GUI;

#define CHECK_FAILED(Status) ((Status) != 0)

//---------------------------------------------------------------------------
// class CTradeMgr
//---------------------------------------------------------------------------

CForm*		CMiniMapMgr::frmRadar = 0;
CEdit*		CMiniMapMgr::edtX = 0;
CEdit*		CMiniMapMgr::edtY = 0;
CCombo*		CMiniMapMgr::cboAddr = 0;

//---------------------------------------------------------------------------
bool CMiniMapMgr::Init()
{

	frmMinimap = _FindForm("frmMinimap");
	if( !frmMinimap ) return false;
	frmMinimap->evtEntrustMouseEvent = _MiniFormMouseEvent;

	MinimatRect = dynamic_cast<CCompent*>(frmMinimap->Find( "imgMinimapRect" ));
	if( !MinimatRect ) return Error(g_oLangRec.GetString(45), frmMinimap->GetName(), "imgMinimapRect");

	char szName[32] = { 0 };
	for( int i=0; i<MAP_POS_MAX; i++ )
	{
		sprintf( szName, "labMapPos%d", i );

		labMapPosRand[i] = dynamic_cast<CLabelEx*>(frmMinimap->Find( szName )); //��ͼ���� 
		if( !labMapPosRand[i] ) return Error(g_oLangRec.GetString(45), frmMinimap->GetName(), szName);
	}
	//labMapPos = dynamic_cast<CLabelEx*>(frmMinimap->Find( "labMapPos" )); //��ͼ���� 
	//if( !labMapPos ) return Error("msgui.clu����<%s>���Ҳ����ؼ�<%s>", frmMinimap->GetName(), "labMapPos");
	labMapPos = labMapPosRand[0];
	
	labMapName = dynamic_cast<CLabelEx*>(frmMinimap->Find( "labMapName" ));   //��ͼ����
	if( !labMapName ) return Error(g_oLangRec.GetString(45), frmMinimap->GetName(), "labMapName");

	//server timer
	labClock = dynamic_cast<CLabelEx*>(frmMinimap->Find("labClock"));
	if (!labClock) return Error(g_oLangRec.GetString(45), frmMinimap->GetName(), "labClock");
		
	// ���ͼ
	frmBigmap =  _FindForm("frmBigmap");
	if( !frmBigmap ) return false;
	frmBigmap->Refresh();
	frmBigmap->evtShow = _evtShowbigmap;
	frmBigmap->evtHide = _evtHidebigmap;

	d3dBigmap = dynamic_cast<C3DCompent*>(frmBigmap->Find( "d3dCompent" ));
	if( !d3dBigmap )  return Error(g_oLangRec.GetString(45), frmBigmap->GetName(), "d3dCompent");
	d3dBigmap->SetRenderEvent( _RenderBigmapEvent );


	{ // Zoom buttons
		constexpr auto ZOOM_STEP{ 1.0f };
		if (auto btnZoomIn = dynamic_cast<CTextButton*>(frmBigmap->Find("btnZoomIn")); btnZoomIn)
		{
			btnZoomIn->evtMouseClick = [](CGuiData* pSender, int x, int y, DWORD key) {
				if (auto scene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene()); scene)
				{
					if (auto map = scene->GetLargerMap(); map)
					{
						map->SetScale(map->GetScale() + ZOOM_STEP);
					}
				}
			};
		}

		if (auto btnZoomOut = dynamic_cast<CTextButton*>(frmBigmap->Find("btnZoomOut")); btnZoomOut)
		{
			btnZoomOut->evtMouseClick = [](CGuiData* pSender, int x, int y, DWORD key) {
				if (auto scene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene()); scene)
				{
					if (auto map = scene->GetLargerMap(); map)
					{
						map->SetScale(map->GetScale() - ZOOM_STEP);
					}
				}
			};
		}
	}

	if (auto checkFollow = dynamic_cast<CCheckBox*>(frmBigmap->Find("checkFollow")); checkFollow)
	{
		checkFollow->evtCheckChange = [](CGuiData* pSender) {
			if (auto scene = CGameApp::GetCurScene(); scene)
			{
				if (auto map = scene->GetLargerMap(); map)
				{
					map->SetFollow(static_cast<CCheckBox*>(pSender)->GetIsChecked());
				}
			}
		};
	}

	//�״�
	frmRadar = _FindForm("frmSearch");
	if( !frmRadar ) return false;
	frmRadar->evtEntrustMouseEvent = _RadarFormEvent;
	frmRadar->evtBeforeShow = _RadarFormBeforeShowEvent;

	edtX = dynamic_cast<CEdit *>(frmRadar->Find("edtLeft"));
	if( !edtX ) return Error(g_oLangRec.GetString(719), frmRadar->GetName(), "edtLeft");
	edtX->SetIsDigit(true);
	edtX->evtEnter = _evtRadarEnter;

	edtY = dynamic_cast<CEdit *>(frmRadar->Find("edtRight"));
	if( !edtY ) return Error(g_oLangRec.GetString(719), frmRadar->GetName(), "edtRight");
	edtY->SetIsDigit(true);
	edtY->evtEnter = _evtRadarEnter;

	cboAddr = dynamic_cast<CCombo *>(frmRadar->Find("cboMap"));
	if (!cboAddr) return Error(g_oLangRec.GetString(719), frmRadar->GetName(), "cboMap");

	btnPosSearch  = dynamic_cast<CTextButton*>(frmMinimap->Find("btnPosSearch"));
	if(!btnPosSearch) return false;
	btnPosSearch->evtMouseClick = _evtShowNPCList;

	chkID  =( CCheckBox *)frmRadar->Find( "chkID" );
	chkID->SetIsChecked(true);
	//----------------------------Daily buff Start------------------------------------//
	//daily buff form 
	if (DailyBuffFrm = _FindForm("DailyBuffFrm"); !DailyBuffFrm)
		return Error(g_oLangRec.GetString(45), "DailyBuffFrm", "DailyBuffFrm");
	if (DailyBufInfo = dynamic_cast<CLabelEx*>(DailyBuffFrm->Find("DailyBufInfo")); !DailyBufInfo) return Error(g_oLangRec.GetString(45), DailyBuffFrm->GetName(), "DailyBufInfo");

	if (DBufImageType = dynamic_cast<CImage*>(DailyBuffFrm->Find("DBufImageType")); !DBufImageType)return Error(g_oLangRec.GetString(45), DailyBuffFrm->GetName(), "DBufImageType");

	//------------------------Daily buff end ----------------------------------------//
    // The first component paints the frame; later controls remain above it.
    if(auto background=dynamic_cast<C3DCompent*>(frmBigmap->Find("d3dMapBackdrop"));background)
        background->SetRenderEvent([](C3DCompent* component,int,int) {
            const int x=component->GetX(),y=component->GetY();
            GetRender().FillFrame(x,y,x+component->GetWidth(),y+component->GetHeight(),0xf2192433);
            GetRender().FillFrame(x+10,y+48,x+188,y+component->GetHeight()-12,0xff111b29);
            GetRender().LineThinFrame(x,y,x+component->GetWidth(),y+component->GetHeight(),0xff697d98);
            for(const auto& button:{RECT{544,17,610,39},RECT{622,17,646,39},RECT{652,17,676,39},RECT{700,17,722,39}})
                GetRender().FillFrame(x+button.left,y+button.top,x+button.right,y+button.bottom,0xff344963);
        });
    markerList=dynamic_cast<CList*>(frmBigmap->Find("lstMapPOI"));
    if(markerList) {
        markerList->SetFontColor(0xffd8e2ef);markerList->SetSelectColor(0xffffd178);
        markerList->SetRowHeight(20);markerList->SetMargin(3,3,12,3);
        markerList->evtSelectChange=[](CGuiData*) {
            auto& ui=g_stUIMap;
            if(!ui.markerList || !ui.markerList->GetItems()->GetSelect())return;
            const int index=ui.markerList->GetItems()->GetSelect()->GetIndex();
            if(index<0 || std::size_t(index)>=ui.markerRows.size())return;
            ui.selectedMarker=ui.markerRows[index];
            auto point=ui.mapMarkers.points.find(ui.selectedMarker);
            auto* scene=CGameApp::GetCurScene();
            if(point!=ui.mapMarkers.points.end() && scene && scene->GetLargerMap()) {
                scene->GetLargerMap()->SetFollow(false);
                scene->GetLargerMap()->Update(point->second.x,point->second.y);
            }
        };
    }
    const char* filterNames[]={"chkBossPOI","chkMiniPOI","chkChestPOI","chkPortalPOI"};
    for(int i=0;i<4;++i) {
        markerFilters[i]=dynamic_cast<CCheckBox*>(frmBigmap->Find(filterNames[i]));
        if(markerFilters[i])markerFilters[i]->SetIsChecked(true);
    }
    markerName=dynamic_cast<CLabelEx*>(frmBigmap->Find("labPOIName"));
    markerState=dynamic_cast<CLabelEx*>(frmBigmap->Find("labPOIState"));
    if(auto center=dynamic_cast<CTextButton*>(frmBigmap->Find("btnPOICenter"));center)
        center->evtMouseClick=[](CGuiData*,int,int,DWORD) {
            auto* scene=CGameApp::GetCurScene();
            if(scene && scene->GetMainCha() && scene->GetLargerMap()) {
                auto* map=scene->GetLargerMap();map->SetFollow(true);
                map->Update(scene->GetMainCha()->GetCurX(),scene->GetMainCha()->GetCurY());
            }
        };
	return true;
}


void CMiniMapMgr::SetupDailyBuffForm(const char* img, const char* labelInfo)
{
	//update label
	DailyBufInfo->SetCaption(labelInfo);
	//update image
	if (const auto imgP = DBufImageType->GetImage(); imgP) {
		DBufImageType->GetImage()->UnLoadImage();//unload old image
		const auto imgPath = std::format("texture/ui/new/DailyBuff/{}.png", img);
		//LoadImage( const char * file, int w, int h, int frame=0, int tx=0, int ty=0, float scalex=0.0, float scaley=0.0 );
		imgP->LoadImage(imgPath.c_str(), imgP->GetWidth(), imgP->GetHeight(), 0, 0, 0,0.9f,0.9f);
		//imgP->LoadAllImage(imgPath.c_str(), imgP->GetWidth(), imgP->GetHeight());
	}
	//after everything show the form 
	DailyBuffFrm->Show();

}
void CMiniMapMgr::End()
{
}

void CMiniMapMgr::FrameMove(DWORD dwTime)
{
    auto* scene=CGameApp::GetCurScene();
    if(scene && scene->GetMainCha()) {
        const std::string name=scene->GetTerrainName();
        if(mapMarkers.map!=name) {mapMarkers.Reset(name);selectedMarker=0;nextMarkerPoll=0;}
        if(nextMarkerPoll==0 || static_cast<LONG>(dwTime-nextMarkerPoll)>=0) {
            CS_MapPOI(mapMarkers.context,mapMarkers.epoch,mapMarkers.revision);
            nextMarkerPoll=dwTime+2000;
        }
    }
    RefreshMarkerList();
	static CTimeWork time(1000);
	if (time.IsTimeOut(dwTime))
	{
		SetClockStringClientSide();
	}
}

void CMiniMapMgr::_evtShowbigmap(CGuiData *pSender)
{
	CWorldScene* pWorldScene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene());
	if (!pWorldScene) return;

    if (auto* map=pWorldScene->GetLargerMap();map) map->Show(true);
    g_stUIMap.nextMarkerPoll=0;

	// Э�鷢�ͻ���ʱ������ʾ
	//if( CGameApp::GetCurScene() && CGameApp::GetCurScene()->GetLargerMap() )
	//{
	//	CGameApp::GetCurScene()->GetLargerMap()->Show( true );
	//}
}

void CMiniMapMgr::_evtHidebigmap(CGuiData *pSender)
{
	CWorldScene* pScene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene());
	if( !pScene ) return;
	
	if( CGameApp::GetCurScene() && CGameApp::GetCurScene()->GetLargerMap() )
	{
		CGameApp::GetCurScene()->GetLargerMap()->Show( false );
	}
}

void CMiniMapMgr::_RenderBigmapEvent(C3DCompent *pSender, int x, int y)
{
	if( CGameApp::GetCurScene() && CGameApp::GetCurScene()->GetLargerMap() )
	{
        auto* map=CGameApp::GetCurScene()->GetLargerMap();
        const RECT bounds{pSender->GetX(),pSender->GetY(),pSender->GetX2(),pSender->GetY2()};
        map->SyncViewport(bounds);map->Render();
	}

    if( g_stUIMap.GetBigmapForm() && g_stUIMap.GetBigmapRect())
    {
        g_stUIMap._RenderBigMapHint();
	}
}

void CMiniMapMgr::_evtRadarEnter(CGuiData *pSender)
{
	ShowRadar();
}


void CMiniMapMgr::RefreshChaPos( int x, int y )
{
	static CTimeWork time(1000);
	if( time.IsTimeOut( CGameApp::GetCurTick() ) )
	{
		int n = rand() % 9;
		labMapPos->SetIsShow( false );

		labMapPos = labMapPosRand[n];
		labMapPos->SetIsShow( true );
	}

	static char buf[32] = { 0 };
	sprintf( buf, "%d,%d", x/100, y/100 );
	labMapPos->SetCaption(buf);
}

void CMiniMapMgr::RefreshMapName( const char*name )
{
	if ( labMapName ) labMapName->SetCaption(name);
}

void CMiniMapMgr::SetClockStringClientSide()
{
	std::time_t rawtime = std::time(nullptr);
	struct std::tm* ptm;
	ptm = gmtime(&rawtime);
	static char buf[26];
	const int UTC = (+8);
	ptm->tm_hour = (ptm->tm_hour + UTC) % 24;
	std::strftime(buf, sizeof(buf), "%T", ptm);
	labClock->SetCaption(buf);
	tServerTime = mktime(ptm);
}

void CMiniMapMgr::_MiniFormMouseEvent(CCompent *pSender, int nMsgType, int x, int y, DWORD dwKey)
{
	string name = pSender->GetName();

	if (name == "btnOpen")
	{
		//�򿪻�رմ��ͼ
		if (!g_stUIMap.frmBigmap->GetIsShow())
			g_stUIMap.frmBigmap->Show();
		else
			g_stUIMap.frmBigmap->Hide();
	}
	else if (name == "btnSearch")
	{

			frmRadar->SetIsShow( !frmRadar->GetIsShow() );
	}
	else if (name == "btnStall")
	{
		g_stUIBooth.SearchAllStalls();
			//frmRadar->SetIsShow( !frmRadar->GetIsShow() );
	}
	else if (name == "btnRank")
	{
		CProCirculateCS* proCir = (CProCirculateCS *)g_NetIF->GetProCir();
		proCir->OpenRankings();
	}
	else if(name == "btnOpenTempBag")
	{
		// ����ʱ����
		CUIInterface::MainChaMove();
		//g_stUIStore.ShowTempKitbag();
	}
	else if(name == "btnOpenStore")
	{
		g_stUIStore.OpenStoreAsk();
	}
	else if (name == "btnDailyBuff") //daily buff form 
	{
		//if form opend close it 
		if (g_stUIMap.DailyBuffFrm->GetIsShow()) {
			g_stUIMap.DailyBuffFrm->Hide();
		}
		else {
			//as server have 3 min cooldown we show temp old data when recall function
			if (static DWORD time = 0; CGameApp::GetCurTick() > time) {
				//to add timer and request here 
				CS_RequestDailyBuffInfo();
				time = CGameApp::GetCurTick() + 180000; // 3 min cooldown //adjust here to be more if needed 
			}
			else {
				g_stUIMap.DailyBuffFrm->Show();
			}

		}
	}
	else if(name == "btnteam")
	{
		CS_VolunteerOpen(10);
	}

	//if( name=="btnClose" || name == "btnNo" )
	//{
	//	CGameApp::GetCurScene()->ShowMinimap( false ) ;
	//	pSender->GetForm()->Close();
	//	return;

	//}
	//float fCurPos = CGameApp::GetCurScene()->GetSmallMap()->GetDist();			
	//if (name =="btnSmall")
	//{
	//	fCurPos +=5.0f;
	//	if (fCurPos >=40.0f ) fCurPos = 40.0f ;
	//	CGameApp::GetCurScene()->GetSmallMap()->SetDist( fCurPos );			
	//}
	//else if (name == "btnBig" )
	//{
	//	fCurPos -= 5.0f;
	//	if ( fCurPos <= 20.0f ) fCurPos = 20.0f ;
	//	CGameApp::GetCurScene()->GetSmallMap()->SetDist( fCurPos );
	//}
	//else if (name == "btnHelp" )
	//{
	//	CForm* frmHelp = CFormMgr::s_Mgr.Find( "frmHelp" );
	//	if( frmHelp )
	//	{
	//		CMemo* memCtrl = dynamic_cast<CMemo*>(frmHelp->Find( "memCtrl" ));
	//		if( memCtrl )
	//		{
	//			memCtrl->Init();
	//			memCtrl->SetCaption( "��������Online������ָ��_��    �ߣ������������ס���ſɳ�������_ս    �����ڳ���Թ��ﵥ�������ս��_NPC�Ի�������������NPC�ɽ��жԻ�_�ӽ�ת������ס�Ҽ���ˮƽ��ת���������ֿ���_    ����Զ��˫���Ҽ��ص�Ĭ�ϵ��ӽ�_��    �ӣ���������ҵ��Ҽ���ѡ��������ӣ�_    ��Ӻ�ֻ�жӳ���������������Ҽ���_��    �죺Alt+F���������·���ť�ɴ�_     ������������ͷ�񣬻���ֶԻ���_��    �ף���������ҵ���Ҽ���ѡ�����뽻��_     ��Է�ͬ��󣬵�����Ҽ佻�׽���" );
	//			memCtrl->ProcessCaption();
	//		}
	//		frmHelp->Refresh();
	//		frmHelp->Show();
	//	}
	//}
	return;
}

void CMiniMapMgr::_RadarFormEvent(CCompent *pSender, int nMsgType, int x, int y, DWORD dwKey)
{
	string name = pSender->GetName();

	if (name == "btnYes")
	{
		ShowRadar();
	}
}

void CMiniMapMgr::_RadarFormBeforeShowEvent(CForm* pForm, bool& IsShow)
{
	if( !CGameApp::GetCurScene() ) 
		return;

	IsShow = true;

	//�õ����Ŀǰ���ڵĵ�ͼ����
	CMapInfo* pMapInfo = GetMapInfo(CGameApp::GetCurScene()->GetInitParam()->strMapFile.c_str());
	if (!pMapInfo)
		cboAddr->GetList()->GetItems()->Select(0);
	else
	{
		int maxItemNum = cboAddr->GetList()->GetItems()->GetCount();
		CItemRow* pItemRow(0);
		int i(0);
		for (; i<maxItemNum; i++)
		{
			CItemRow* pItemRow = cboAddr->GetList()->GetItems()->GetItem(i);
			if (pItemRow)
			{
				if (strcmp(pMapInfo->szName, pItemRow->GetBegin()->GetString()) == 0)
					break;
			}

		}
		if (i < maxItemNum)
		{
			cboAddr->GetList()->GetItems()->Select(i);
		}
		else
		{
			cboAddr->GetList()->GetItems()->Select(0);
		}
	}

	edtX->SetCaption("");
	edtY->SetCaption("");
	edtX->SetActive(edtX);

	if (frmRadar->GetIsShow())
	{
		frmRadar->Close();
	}
	if (CNavigationBar::g_cNaviBar.IsShow())
	{
		CNavigationBar::g_cNaviBar.Show(false);
	}
}

int CMiniMapMgr::CheckCoordinateEdit(const char* input)
{
	if (!(*input))
		return -1;

	while(*input)
	{
		if (0x30 <= (*input) && (*input) <= 0x39)	//�ж�������
		{
			input ++;
		}
		else
		{
			return -1;
		}
	}
	return 0;
}

void CMiniMapMgr::ClearRadar()
{
	edtX->SetCaption("");
	edtY->SetCaption("");
	cboAddr->GetList()->GetItems()->Select(0);
}
void CMiniMapMgr::ShowRadar(const char * szX, const char * szY)
{
	if (CHECK_FAILED(CheckCoordinateEdit(szX)))
	{
		g_pGameApp->MsgBox(g_oLangRec.GetString(720));
		edtX->SetCaption("");
		edtX->SetActive(edtX);
		return;
	}
	if (CHECK_FAILED(CheckCoordinateEdit(szY)))
	{
		g_pGameApp->MsgBox(g_oLangRec.GetString(720));
		edtY->SetCaption("");
		edtY->SetActive(edtY);
		return;
	}

	int x = atoi(szX), y = atoi(szY);
	D3DXVECTOR3 target((float)x, (float)y, 0);

	const char* _strName = g_pGameApp->GetCurScene()->GetTerrainName();
	if(strcmp(_strName,"garner") == 0)
		_strName = g_oLangRec.GetString(56);
	else if(strcmp(_strName,"magicsea") == 0)
		_strName = g_oLangRec.GetString(58);
	else if(strcmp(_strName,"darkblue") == 0)
		_strName = g_oLangRec.GetString(59);
	else if( strcmp( _strName, "winterland" ) == 0 )
		_strName = "Winter Isle Archipelago";
	else if( strcmp( _strName, "jialebi" ) == 0 )
		_strName = "Pirate\'s Base";


	CWorldScene* pScene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene());
	if( !pScene ) return;
	const char* curMap = pScene->GetCurMapInfo()->szName;
	
	if ((strcmp(curMap, _strName) == 0) && (!pScene->GetMainCha()->IsBoat()))
	{
		if(g_stUIStart.chkID->GetIsChecked())
		{
			g_cFindPathEx.Reset();
			g_cFindPathEx.ClearDestDirection();
			g_cFindPathEx.SetDestDirection((int)pScene->GetMainCha()->GetPos().x,(int)pScene->GetMainCha()->GetPos().y,x,y);
			g_cFindPathEx.SetTarget((int)pScene->GetMainCha()->GetPos().x,(int)pScene->GetMainCha()->GetPos().y,x,y);
		}
	}
	else
	{
		if(pScene->GetMainCha()->IsBoat())
		{
			g_stUIBox.ShowMsgBox(nullptr, "No routing on sea.");
		}
		else
		{			
			g_stUIBox.ShowMsgBox(nullptr,  "Not on the same map.");	
		}
	}
	CNavigationBar::g_cNaviBar.SetTarget((char*)_strName, target);
	CNavigationBar::g_cNaviBar.Show(true);
}
void CMiniMapMgr::ShowRadar()
{
	const char * szX = edtX->GetCaption();
	const char * szY = edtY->GetCaption();
	if (CHECK_FAILED(CheckCoordinateEdit(szX)))
	{
		g_pGameApp->MsgBox(g_oLangRec.GetString(720));
		edtX->SetCaption("");
		edtX->SetActive(edtX);
		return;
	}
	if (CHECK_FAILED(CheckCoordinateEdit(szY)))
	{
		g_pGameApp->MsgBox(g_oLangRec.GetString(720));
		edtY->SetCaption("");
		edtY->SetActive(edtY);
		return;
	}

	edtX->SetActive(edtX);
	int x = atoi(szX), y = atoi(szY);
	D3DXVECTOR3 target((float)x, (float)y, 0);

	const char* szAddress = cboAddr->GetText();

	CWorldScene* pScene = dynamic_cast<CWorldScene*>(CGameApp::GetCurScene());
	if( !pScene ) return;
	const char* curMap = pScene->GetCurMapInfo()->szName;

	if((strcmp(curMap,szAddress) == 0) && (!pScene->GetMainCha()->IsBoat()))
	{
		if(g_stUIMap.chkID->GetIsChecked())
		{
			g_cFindPathEx.Reset();
			g_cFindPathEx.ClearDestDirection();
			g_cFindPathEx.SetDestDirection((int)pScene->GetMainCha()->GetPos().x,(int)pScene->GetMainCha()->GetPos().y,x,y);
			g_cFindPathEx.SetTarget((int)pScene->GetMainCha()->GetPos().x,(int)pScene->GetMainCha()->GetPos().y,x,y);
		}
	}
	else
	{
		if(pScene->GetMainCha()->IsBoat())
		{
			g_stUIBox.ShowMsgBox(nullptr, "No routing on sea.");
		}
		else
		{			
			g_stUIBox.ShowMsgBox(nullptr, "Not on the same map.");	
		}
	}
	CNavigationBar::g_cNaviBar.SetTarget((char*)szAddress, target);
	CNavigationBar::g_cNaviBar.Show(true);
	frmRadar->Hide();
}

void CMiniMapMgr::SwitchMap()
{
    mapMarkers.Reset("");selectedMarker=0;nextMarkerPoll=0;
	frmBigmap->Close();
}

bool CMiniMapMgr::IsShowBigmap()
{
	return frmBigmap->GetIsShow();
}

void CMiniMapMgr::CloseRadar()
{
	if(frmRadar && frmRadar->GetIsShow())
	{
		this->ClearRadar();
		frmRadar->Close();
	}

	if (CNavigationBar::g_cNaviBar.IsShow())
	{
		CNavigationBar::g_cNaviBar.Show(false);
	}

}



bool CMiniMapMgr::IsPKSilver()
{
	return false; // Disables blind CA.
	if(0 == strcmp(GetMapName(), g_oLangRec.GetString(900))) // �Ҷ������� //"������"))
		return true;

	return false;
}

bool CMiniMapMgr::IsGuildWar()
{
	if(0 == strcmp(GetMapName(), g_oLangRec.GetString(934)) || 0 == strcmp(GetMapName(), g_oLangRec.GetString(934)))	// "ʥս" "Сʥս"
		return true;

	return false;
}
//Add by sunny.sun 20080904
//Begin
void CMiniMapMgr::_RenderBigMapHint(void)
{
    auto* scene=CGameApp::GetCurScene();if(!scene || !scene->GetLargerMap() || !scene->GetTerrain())return;
    const auto projection=scene->GetLargerMap()->GetProjection();
    const int sx=g_pGameApp->GetMouseX(),sy=g_pGameApp->GetMouseY();
    if(!projection.Contains(sx,sy))return;
    double x,y;if(!projection.World(sx,sy,x,y))return;
    if(x<0 || y<0 || x>=scene->GetTerrain()->GetWidth()*100 || y>=scene->GetTerrain()->GetHeight()*100)return;
    mouse_map_coordinate={LONG(x/100),LONG(y/100)};
}

void CMiniMapMgr::RefreshMarkerList()
{
    unsigned filters=0;
    for(int i=0;i<4;++i)if(!markerFilters[i] || markerFilters[i]->GetIsChecked())filters|=1u<<i;
    if(markerList && (listEpoch!=mapMarkers.epoch || listRevision!=mapMarkers.revision ||
       listContext!=mapMarkers.context || listFilters!=filters)) {
        markerList->Clear();markerRows.clear();
        for(const auto& pair:mapMarkers.points) {
            const auto& point=pair.second;
            if(!(filters & (1u<<(int(point.kind)-1))))continue;
            const auto name=point.name.substr(0,20);
            markerList->Add(name.c_str());markerRows.push_back(point.id);
        }
        markerList->Refresh();
        listEpoch=mapMarkers.epoch;listRevision=mapMarkers.revision;listContext=mapMarkers.context;listFilters=filters;
    }
    auto selected=mapMarkers.points.find(selectedMarker);
    if(selected==mapMarkers.points.end()) {
        if(markerName)markerName->SetCaption("Selecciona un punto");
        if(markerState)markerState->SetCaption("Solo puntos conocidos");
    } else {
        const auto& p=selected->second;
        if(markerName)markerName->SetCaption(p.name.substr(0,23).c_str());
        char text[96];std::snprintf(text,sizeof(text),"%s  %d,%d",p.state==map_poi::State::Active?"Disponible":"Inactivo",p.x/100,p.y/100);
        if(markerState)markerState->SetCaption(text);
    }
}

void CMiniMapMgr::RenderMapMarkers(const map_poi::Projection& projection,bool minimap)
{
    struct Vertex {float x,y,z,w;DWORD color;};
    std::vector<Vertex> triangles;triangles.reserve(mapMarkers.points.size()*18);
    auto triangle=[&](double ax,double ay,double bx,double by,double cx,double cy,DWORD color) {
        for(const auto& p:{std::make_pair(ax,ay),std::make_pair(bx,by),std::make_pair(cx,cy)})
            triangles.push_back({float(p.first),float(p.second),0.9f,1.0f,color});
    };
    auto quad=[&](double l,double t,double r,double b,DWORD color) {
        triangle(l,t,r,t,r,b,color);triangle(l,t,r,b,l,b,color);
    };
    const int mx=g_pGameApp->GetMouseX(),my=g_pGameApp->GetMouseY();
    const map_poi::Marker* hovered=nullptr;double closest=100;
    for(const auto& pair:mapMarkers.points) {
        const auto& p=pair.second;const int kind=int(p.kind);
        if(minimap && !map_poi::Public(p.kind))continue;
        if(kind<1 || kind>4 || (markerFilters[kind-1] && !markerFilters[kind-1]->GetIsChecked()))continue;
        double sx,sy;projection.Screen(p.x,p.y,sx,sy);
        const double size=minimap?5:7;
        if(!projection.Contains(sx,sy,size+2))continue;
        DWORD color=p.state!=map_poi::State::Active?0xff8490a0:
            p.kind==map_poi::Kind::Boss?0xffff5c61:p.kind==map_poi::Kind::MiniBoss?0xffffac4a:
            p.kind==map_poi::Kind::Chest?0xffffd56b:0xffb69aff;
        if(!minimap && p.id==selectedMarker)quad(sx-size-2,sy-size-2,sx+size+2,sy+size+2,0xfff1f6ff);
        quad(sx-size-1,sy-size-1,sx+size+1,sy+size+1,0xff172131);
        if(p.kind==map_poi::Kind::Boss) {
            triangle(sx,sy-size,sx+size,sy,sx,sy+size,color);
            triangle(sx,sy-size,sx,sy+size,sx-size,sy,color);
        } else if(p.kind==map_poi::Kind::MiniBoss)triangle(sx,sy-size,sx+size,sy+size,sx-size,sy+size,color);
        else if(p.kind==map_poi::Kind::Chest) {
            quad(sx-size,sy-size+2,sx+size,sy+size-1,color);
            quad(sx-1,sy-size+2,sx+1,sy+size-1,0xff805a23);
        } else {
            constexpr double pi=3.141592653589793;
            for(int i=0;i<12;++i) {
                const double a=i*pi/6,b=(i+1)*pi/6;
                triangle(sx+std::cos(a)*size,sy+std::sin(a)*size,sx+std::cos(b)*size,sy+std::sin(b)*size,
                    sx+std::cos(a)*(size-2),sy+std::sin(a)*(size-2),color);
                triangle(sx+std::cos(b)*size,sy+std::sin(b)*size,sx+std::cos(b)*(size-2),sy+std::sin(b)*(size-2),
                    sx+std::cos(a)*(size-2),sy+std::sin(a)*(size-2),color);
            }
        }
        const double d=(mx-sx)*(mx-sx)+(my-sy)*(my-sy);
        if(!minimap && d<closest){closest=d;hovered=&p;}
    }
    if(!triangles.empty()) {
        g_Render.SetTexture(0,nullptr);g_Render.SetVertexShader(nullptr);
        g_Render.SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        g_Render.SetRenderState(D3DRS_ZENABLE,FALSE);g_Render.SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        g_Render.SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        g_Render.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        g_Render.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        g_Render.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
        g_Render.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
        g_Render.GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(triangles.size()/3),triangles.data(),sizeof(Vertex));
        g_Render.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
        g_Render.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        g_Render.SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        g_Render.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        g_Render.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        g_Render.SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    }
    if(hovered) {
        if(g_pGameApp->GetMouseKey() & M_LDown)selectedMarker=hovered->id;
        char hint[128];std::snprintf(hint,sizeof(hint),"%s [%d,%d] - %s",hovered->name.c_str(),hovered->x/100,hovered->y/100,
            hovered->state==map_poi::State::Active?"Disponible":"Inactivo");
        if(auto* font=g_pGameApp->GetFont();font) {
            const int x=int(projection.left)+12,y=int(projection.top+projection.height)-22;
            font->DrawTextShadow(hint,x,y,x+1,y+1,0xff0c1220,0xffffffff);
        }
    }
}

//=============================================================================
// FileName: Action.cpp
// Creater: ZhangXuedong
// Date: 2004.10.08
// Comment: CAction class
//=============================================================================
#include "stdafx.h"
#include "Action.h"
#include "GameCommon.h"
#include "MoveAble.h"
#include "TryUtil.h"
#include "Character.h"
#include <algorithm>
#include "GameApp.h"

_DBC_USING;

CAction::CAction(Entity *pCEntity)
{T_B
	m_pCEntity = pCEntity;
	memset(m_SAction, 0, sizeof(m_SAction));
	m_sActionNum = 0;
	m_sCurAction = -1;
T_E}

bool CAction::Add(dbc::Short sActionType, void *pActionData)
{T_B
	if (m_sCurAction != -1 || m_sActionNum >= defMAX_ACTION_NUM)
	{
		return false;
	}

	m_SAction[m_sActionNum].sType = sActionType;
	switch (m_SAction[m_sActionNum].sType)
	{
	case	enumACTION_MOVE:
		{
			memcpy(&m_SMoveInit, pActionData, sizeof(m_SMoveInit));
			m_SAction[m_sActionNum].pInit = &m_SMoveInit;
			m_sActionNum ++;

			break;
		}
	case	enumACTION_SKILL:
		{
			memcpy(&m_SFightInit, pActionData, sizeof(m_SFightInit));
			m_SAction[m_sActionNum].pInit = &m_SFightInit;
			m_sActionNum ++;

			break;
		}
	default:
		{
			break;
		}
	}

	return true;
T_E}

//=============================================================================
// sActionType 刚完成的动作类型.0，表示首次执行动作队列
// sActionState 行动执行的结果
//=============================================================================
bool CAction::DoNext(Short sActionType, Short sActionState)
{T_B
	if (m_sActionNum == 0 || m_pCEntity == NULL)
	{
		return false;
	}
	if (m_sCurAction >= m_sActionNum - 1)
	{
		m_sCurAction = -1;
		m_sActionNum = 0;
		return false;
	}

	switch (m_SAction[m_sCurAction + 1].sType)
	{
	case	enumACTION_MOVE:
		{
			CMoveAble *pCMoveAble = m_pCEntity->IsMoveAble();
			if (pCMoveAble)
			{
				m_sCurAction ++;

				if (pCMoveAble->DesireMoveBegin((CMoveAble::SMoveInit *)m_SAction[m_sCurAction].pInit) == false)
				{
					m_sCurAction = -1;
					m_sActionNum = 0;
				}
			}

			break;
		}
	case	enumACTION_SKILL:
		{
			if (sActionType == enumACTION_MOVE && sActionState & enumMSTATE_INRANGE)
			{
				CFightAble *pCFightAble = m_pCEntity->IsFightAble();
				if (pCFightAble)
				{
					m_sCurAction ++;

					if (pCFightAble->DesireFightBegin((SFightInit *)m_SAction[m_sCurAction].pInit) == false)
					{
						m_sCurAction = -1;
						m_sActionNum = 0;
					}
				}
			}
			else
				Interrupt();

			break;
		}
	default:
		{
			return false;

			break;
		}
	}

	return true;
T_E}

void CAction::End()
{T_B
	if (m_sActionNum == 0 || m_sCurAction == -1 || m_pCEntity == NULL)
	{
		return;
	}

	switch (m_SAction[m_sCurAction].sType)
	{
	case	enumACTION_MOVE:
		{
			CMoveAble *pCMoveAble = m_pCEntity->IsMoveAble();
			if (pCMoveAble)
			{
				pCMoveAble->DesireMoveStop();
			}

			break;
		}
	case	enumACTION_SKILL:
		{
			CFightAble *pCFightAble = m_pCEntity->IsFightAble();
			if (pCFightAble)
			{
				pCFightAble->DesireFightEnd();
			}

			break;
		}
	default:
		{
			break;
		}
	}

	m_sCurAction = -1;
	m_sActionNum = 0;

	return;
T_E}

void CAction::Interrupt()
{T_B
	m_sCurAction = -1;
	m_sActionNum = 0;

	return;
T_E}

bool CAction::Has(Short sActionType, void *pActionData)
{
	//if (m_sActionNum == 0 || m_pCEntity == NULL)
	//	return false;
	//if (m_sCurAction >= m_sActionNum - 1)
	//	return false;

	//for (Short i = m_sCurAction + 1; i < m_sActionNum; i++)
	//{
	//	if (m_SAction[i].sType == sActionType)
	//	{
	//	}
	//}

	return false;
}

//=============================================================================
CActionCache::CActionCache(CCharacter *pCOwn)
{
	m_pCOwn = pCOwn;
	m_pSExecQueue = NULL;
	m_pSFreeQueue = NULL;
}

CActionCache::~CActionCache()
{
	SAction	*pSCarrier;

	pSCarrier = m_pSExecQueue;
	while (pSCarrier)
	{
		m_pSExecQueue = pSCarrier->pSNext;
		delete pSCarrier;
		pSCarrier = m_pSExecQueue;
	}

	pSCarrier = m_pSFreeQueue;
	while (pSCarrier)
	{
		m_pSFreeQueue = pSCarrier->pSNext;
		delete pSCarrier;
		pSCarrier = m_pSFreeQueue;
	}
}

void CActionCache::Clear()
{
    ++m_ullCancelVersion;
    while (m_pSExecQueue)
    {
        auto* item = m_pSExecQueue;
        m_pSExecQueue = item->pSNext;
        item->pSNext = m_pSFreeQueue;
        m_pSFreeQueue = item;
    }
    m_pSTail = m_pSBuilding = nullptr;
    m_uPending = 0;
}

void CActionCache::AddCommand(Short command)
{
    auto* item = m_pSFreeQueue;
    if (item) m_pSFreeQueue = item->pSNext;
    else item = new SAction;
    item->sCommand = command;
    item->chParamPos = 0;
    item->ullOwnerGeneration = m_pCOwn->GetLifeGeneration();
    item->pSNext = nullptr;
    if (m_pSTail) m_pSTail->pSNext = item;
    else m_pSExecQueue = item;
    m_pSTail = m_pSBuilding = item;
    ++m_uPending;
}

void CActionCache::PushParam(void* param, Char size)
{
    if (m_pSBuilding && param && size > 0 &&
        static_cast<unsigned int>(m_pSBuilding->chParamPos) + size <= defMAX_CACHE_ACTION_PARAM_LEN)
    {
        memcpy(m_pSBuilding->szParam + m_pSBuilding->chParamPos, param, size);
        m_pSBuilding->chParamPos += size;
    }
}

void CActionCache::Run()
{
    const auto version = m_ullCancelVersion;
    unsigned int budget = (std::min)(m_uPending, 256u);
    while (m_pSExecQueue && budget-- && version == m_ullCancelVersion)
    {
        auto* item = m_pSExecQueue;
        m_pSExecQueue = item->pSNext;
        if (!m_pSExecQueue) m_pSTail = nullptr;
        if (m_pSBuilding == item) m_pSBuilding = nullptr;
        --m_uPending;
        if (m_pCOwn->IsLiveing() && item->ullOwnerGeneration == m_pCOwn->GetLifeGeneration())
            ExecAction(item);
        item->pSNext = m_pSFreeQueue;
        m_pSFreeQueue = item;
    }
}

namespace {
template<class T>
bool ReadCachedParam(const CActionCache::SAction* action, unsigned int& offset, T& value)
{
    if (offset + sizeof(T) > static_cast<unsigned int>(action->chParamPos)) return false;
    memcpy(&value, action->szParam + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}
}

void CActionCache::ExecAction(SAction* action)
{
    unsigned int offset = 0;
    Long skill = 0;
    switch (action->sCommand)
    {
    case enumCACHEACTION_MOVE:
    {
        Short ping = 0;
        Char count = 0;
        Point path[2];
        if (!ReadCachedParam(action, offset, ping) || !ReadCachedParam(action, offset, count) ||
            count != 2 || !ReadCachedParam(action, offset, path)) return;
        if (offset == static_cast<unsigned int>(action->chParamPos))
            m_pCOwn->Cmd_BeginMove(ping, path, count);
        else
        {
            Char stop = 0;
            if (ReadCachedParam(action, offset, stop) && offset == static_cast<unsigned int>(action->chParamPos))
                m_pCOwn->Cmd_BeginMove(ping, path, count, stop);
        }
        break;
    }
    case enumCACHEACTION_SKILL:
    {
        dbc::uLong id = 0;
        Long handle = 0;
        unsigned long long generation = 0;
        if (!ReadCachedParam(action, offset, skill) || !ReadCachedParam(action, offset, id) ||
            !ReadCachedParam(action, offset, handle) || !ReadCachedParam(action, offset, generation) ||
            offset != static_cast<unsigned int>(action->chParamPos)) return;
        auto* target = g_pGameApp->IsLiveingEntity(id, handle);
        if (target && target->GetLifeGeneration() == generation &&
            target->GetSubMap() == m_pCOwn->GetSubMap())
            m_pCOwn->Cmd_BeginSkillDirect(skill, target);
        break;
    }
    case enumCACHEACTION_SKILL2:
    {
        Long level = 0, x = 0, y = 0;
        if (ReadCachedParam(action, offset, skill) && ReadCachedParam(action, offset, level) &&
            ReadCachedParam(action, offset, x) && ReadCachedParam(action, offset, y) &&
            offset == static_cast<unsigned int>(action->chParamPos))
            m_pCOwn->Cmd_BeginSkillDirect2(skill, level, x, y);
        break;
    }
    }
}

/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "door.h"
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/zesc.h>
#include <game/server/entities/character.h>

CDoor::CDoor(CGameWorld* pGameWorld, int Index, int Time, vec2 Pos) : CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, 0)
{
	m_Index = Index;
	m_Time = -1;
	if (Time > 0)
		m_Time = Time * Server()->TickSpeed();
	m_State = DOOR_CLOSED;
	if (Index > 32)
		m_State = DOOR_OPEN;
	if (Index == -1)
		m_State = DOOR_ZCLOSED;

	GameWorld()->InsertEntity(this);
}

void CDoor::Reset()
{
	if (m_Index == -1)
		GameServer()->m_World.DestroyEntity(this);
}

void CDoor::Tick()
{
	if (m_Time > 0)
	{
		m_Time--;
		if (!m_Time)
			GameServer()->m_World.DestroyEntity(this);
	}
	if(m_Index > -1)
		if(GameServer()->m_pController->GetGameType()=="zESC")
			m_State = GameServer()->zESCController()->DoorState(m_Index);
	if (m_State == DOOR_OPEN || m_State == DOOR_ZCLOSING || m_State == DOOR_REOPENED || !m_Time
		//|| !g_Config.m_SvDoors
		)
		return;
	CCharacter* apCloseCCharacters[MAX_CLIENTS];
	int Num = GameServer()->m_World.FindEntities(m_Pos, 8.0f, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter* pTarget = apCloseCCharacters[i];
		if(
			!pTarget->IsAlive() ||
			pTarget->GetPlayer()->GetTeam() == TEAM_SPECTATORS ||
			GameServer()->Collision()->IntersectLine(m_Pos, pTarget->m_Pos, NULL, NULL) ||
			(m_State == DOOR_ZCLOSED && pTarget->GetPlayer()->GetTeam() == TEAM_BLUE)
			)
			continue;
		pTarget->m_HittingDoor = true;
		pTarget->m_PushDirection = normalize(apCloseCCharacters[i]->m_OldPos - m_Pos);
		apCloseCCharacters[i]->m_Ninja.m_CurrentMoveTime = 0;
	}
}

void CDoor::Snap(int SnappingClient)
{
	if (m_State == DOOR_OPEN || m_State == DOOR_ZCLOSING || m_State == DOOR_REOPENED || !m_Time ||
		//!g_Config.m_SvDoors ||
		NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if (!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	if (m_State == DOOR_ZCLOSED)
		pP->m_Type = PICKUP_HEALTH;
	else
		pP->m_Type = PICKUP_ARMOR;
}

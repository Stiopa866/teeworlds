#include <generated/server_data.h>
#include <game/server/gamecontext.h>

#include "character.h"
#include "harpoon.h"

CHarpoon::CHarpoon(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Owner)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Vel = Direction*50;
	m_Grounded = false;
	m_SpawnTick = GameServer()->Server()->Tick();
	GameWorld()->InsertEntity(this);
}

void CHarpoon::Tick()
{
	if (!m_Grounded)
	{
		//Gravity, friction effect
		m_Vel.y += GameServer()->Tuning()->m_HarpoonCurvature;
		if (m_Vel.x > 0)
		{
			m_Vel.x -= 0.2f;
		}
		else if (m_Vel.x < 0)
		{
			m_Vel.x += 0.2f;
		}

		//Move
		vec2 NewPos = m_Pos;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveHarpoonBox(&NewPos, &m_Vel, vec2(28.0f, 28.0f), 1, &m_Grounded);

		m_Pos = NewPos;
	}
}

void CHarpoon::TickPaused()
{
	m_SpawnTick++;
}

void CHarpoon::Reset()
{
	GameWorld()->RemoveEntity(this);
}

void CHarpoon::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos))
		return;

	CNetObj_Harpoon* pHarpoon = static_cast<CNetObj_Harpoon*>(Server()->SnapNewItem(NETOBJTYPE_HARPOON, GetID(), sizeof(CNetObj_Harpoon)));
	if (pHarpoon)
		FillInfo(pHarpoon);
}

void CHarpoon::FillInfo(CNetObj_Harpoon* pHarpoon)
{
	pHarpoon->m_X = round_to_int(m_Pos.x);
	pHarpoon->m_Y = round_to_int(m_Pos.y);
	pHarpoon->m_Dir_X = round_to_int(m_Vel.x * 100.0f);
	pHarpoon->m_Dir_Y = round_to_int(m_Vel.y * 100.0f);
	pHarpoon->m_SpawnTick = m_SpawnTick;
}
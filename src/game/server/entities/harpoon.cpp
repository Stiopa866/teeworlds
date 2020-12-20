#include <generated/server_data.h>
#include <game/server/gamecontext.h>

#include "character.h"
#include "harpoon.h"

CHarpoon::CHarpoon(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Owner, CCharacter* This)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Pos = Pos;
	m_pOwnerChar = This;
	m_Owner = Owner;
	m_DeathTick = -1;
	m_Vel = Direction*GameServer()->Tuning()->m_HarpoonInitialSpeed;
	m_Grounded = HARPOON_FLYING;
	m_SpawnTick = GameServer()->Server()->Tick();
	GameWorld()->InsertEntity(this);
}

void CHarpoon::Tick()
{
	if (!m_pOwnerChar && m_DeathTick == -1) //begin countdown to death
	{
		m_DeathTick = Server()->TickSpeed() * GameServer()->Tuning()->m_HarpoonLifetimewithoutchar * 60;
	}
	else if (m_DeathTick > 0)
	{
		m_DeathTick--;
		if (!m_DeathTick)
		{
			RemoveHarpoon();
		}
	}
	//char aBuf[64];
	//str_format(aBuf, sizeof(aBuf), "Harpoon Status %d", m_Grounded);
	//GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	if (!m_Grounded)
	{
		//Gravity, friction effect
		m_Vel.y += GameServer()->Tuning()->m_HarpoonCurvature;
		if (m_Vel.x > 0)
		{
			m_Vel.x -= GameServer()->Tuning()->m_HarpoonSpeedLoss;
		}
		else if (m_Vel.x < 0)
		{
			m_Vel.x += GameServer()->Tuning()->m_HarpoonSpeedLoss;
		}

		//Move
		vec2 NewPos = m_Pos;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveHarpoonBox(&NewPos, &m_Vel, vec2(28.0f, 28.0f), GameServer()->Tuning()->m_HarpoonElasticity, &m_Grounded);
		CCharacter* TargetChr = GameWorld()->IntersectCharacter(m_Pos, NewPos, 6.0f, NewPos, m_pOwnerChar);
		if (TargetChr)
		{
			m_pVictim = TargetChr;
			NewPos = TargetChr->GetPos();
			m_Grounded = HARPOON_IN_CHARACTER;
			TargetChr->TakeDamage(m_Vel , m_Vel * -1, 4, m_Owner, 7);
			m_Pos = TargetChr->GetPos();
		}
		else m_Pos = NewPos;
	}
	if (m_Grounded == HARPOON_RETRACTING)
	{
		vec2 OwnerPos = m_pOwnerChar->GetPos();
		if (m_Pos.x > OwnerPos.x)
		{
			m_Pos.x = SaturatedAdd(OwnerPos.x, m_Pos.x, m_Pos.x, -GameServer()->Tuning()->m_HarpoonReturnSpeed);
		}
		else if (m_Pos.x < OwnerPos.x)
		{
			m_Pos.x = SaturatedAdd(m_Pos.x, OwnerPos.x, m_Pos.x, GameServer()->Tuning()->m_HarpoonReturnSpeed * 1.0f);
		}
		if (m_Pos.y > OwnerPos.y)
		{
			m_Pos.y = SaturatedAdd(OwnerPos.y, m_Pos.y, m_Pos.y, -GameServer()->Tuning()->m_HarpoonReturnSpeed);
		}
		else if (m_Pos.y < OwnerPos.y)
		{
			m_Pos.y = SaturatedAdd(m_Pos.y, OwnerPos.y, m_Pos.y, GameServer()->Tuning()->m_HarpoonReturnSpeed * 1.0f);
		}
		if (m_Pos.y == OwnerPos.y && m_Pos.x == OwnerPos.x)
		{
			RemoveHarpoon();
		}
	}
	if (m_pVictim)
	{
		m_Pos = m_pVictim->GetPos();
	}
}

void CHarpoon::Drag()
{
	if (GameServer()->Tuning()->m_HarpoonScaledWithDistance)
	{
		if (m_Grounded == HARPOON_IN_CHARACTER)
		{
			vec2 Distance = m_pOwnerChar->GetPos() - m_pVictim->GetPos();
			Distance /= 200;
			if (GameServer()->Tuning()->m_HarpoonClampPull)
			{
				Distance.x = clamp(Distance.x, -GameServer()->Tuning()->m_HarpoonClampPull, GameServer()->Tuning()->m_HarpoonClampPull * 1.0f);
				Distance.y = clamp(Distance.y, -GameServer()->Tuning()->m_HarpoonClampPull, GameServer()->Tuning()->m_HarpoonClampPull * 1.0f);
			}
			m_pVictim->HarpoonDrag(Distance);
		}
		else if (m_Grounded == HARPOON_IN_GROUND)
		{
			vec2 Distance = m_Pos - m_pOwnerChar->GetPos();
			Distance /= 200;
			if (GameServer()->Tuning()->m_HarpoonClampPull)
			{
				Distance.x = clamp(Distance.x, -GameServer()->Tuning()->m_HarpoonClampPull, GameServer()->Tuning()->m_HarpoonClampPull * 1.0f);
				Distance.y = clamp(Distance.y, -GameServer()->Tuning()->m_HarpoonClampPull, GameServer()->Tuning()->m_HarpoonClampPull * 1.0f);
			}
			m_pOwnerChar->HarpoonDrag(Distance);
		}
	}
	else
	{
		if (m_Grounded == HARPOON_IN_CHARACTER)
		{
			vec2 Distance;
			Distance.x = GameServer()->Tuning()->m_HarpoonPullValue * sign(m_pOwnerChar->GetPos().x - m_pVictim->GetPos().x);
			Distance.y = GameServer()->Tuning()->m_HarpoonPullValue * sign(m_pOwnerChar->GetPos().y - m_pVictim->GetPos().y);

			m_pVictim->HarpoonDrag(Distance);
		}
		else if (m_Grounded == HARPOON_IN_GROUND)
		{
			vec2 Distance;
			Distance.x = GameServer()->Tuning()->m_HarpoonPullValue * sign(m_Pos.x - m_pOwnerChar->GetPos().x);
			Distance.y = GameServer()->Tuning()->m_HarpoonPullValue * sign(m_Pos.y - m_pOwnerChar->GetPos().y);
			m_pOwnerChar->HarpoonDrag(Distance);
		}
	}
}

void CHarpoon::RemoveHarpoon()
{
	if(m_pOwnerChar)
		m_pOwnerChar->DeallocateHarpoon();
	if (m_pVictim)
		m_pVictim->DeallocateVictimHarpoon();
	GameWorld()->DestroyEntity(this);
}

void CHarpoon::DeallocateOwner()
{
	m_pOwnerChar = 0x0;
	if (m_Grounded == HARPOON_RETRACTING)
	{
		m_Grounded == HARPOON_FLYING;
	}
}
void CHarpoon::DeallocateVictim()
{
	m_pVictim = 0x0;
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
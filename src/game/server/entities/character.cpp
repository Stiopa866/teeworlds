/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/mapitems.h>

#include "../gamemodes/zesc.h"

#include "character.h"
#include "laser.h"
#include "projectile.h"
#include "door.h"

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ProximityRadius = ms_PhysSize;
	m_Health = 0;
	m_Armor = 0;
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_ActiveWeapon = WEAPON_GUN;
	if(pPlayer->GetTeam() == TEAM_RED)
		m_ActiveWeapon = WEAPON_HAMMER;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	m_LastSpeedup = -1;

	GameServer()->m_pController->OnCharacterSpawn(this);

	return true;
}

void CCharacter::Destroy()
{
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon || m_pPlayer->GetTeam() == TEAM_RED)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x+m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x-m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	return false;
}


void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	if ((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = m_LastWeapon;

		SetWeapon(m_ActiveWeapon);
		return;
	}

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = m_ProximityRadius * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameServer()->m_World.FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;

	switch(m_ActiveWeapon)
	{
	case WEAPON_HAMMER:
		{
			// reset objects Hit
			m_NumObjectsHit = 0;
			GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);
			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			if(m_pPlayer->GetTeam() == TEAM_RED)
			{
				int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*1.5f, (CEntity**)apEnts, 
					MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

				for (int i = 0; i < Num; ++i)
				{
					CCharacter *pTarget = apEnts[i];

					if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL) || pTarget->m_pPlayer->GetTeam() == TEAM_RED)
						continue;

					// set his velocity to fast upward (for now)
					if(length(pTarget->m_Pos-ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos-normalize(pTarget->m_Pos-ProjStartPos)*m_ProximityRadius*1.5f);
					else
						GameServer()->CreateHammerHit(ProjStartPos);

					GameServer()->m_apPlayers[pTarget->m_pPlayer->GetCID()]->SetZomb(m_pPlayer->GetCID());

					// a nice sound
					GameServer()->CreateSound(pTarget->m_Pos, SOUND_PLAYER_PAIN_LONG);

					Hits++;
				}
			}
			else if((m_Item == HITEM_HAMMER) && m_Armor)
			{
				CDoor *pDoor = new CDoor(&GameServer()->m_World, -1, g_Config.m_SvHdoorReopenTime);
				pDoor->m_Pos = m_Pos;
				m_Armor--;
			}
			else
			{
				int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*0.65f, (CEntity**)apEnts, 
					MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

				for (int i = 0; i < Num; ++i)
				{
					CCharacter *pTarget = apEnts[i];

					if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
						continue;

					// set his velocity to fast upward (for now)
					if(length(pTarget->m_Pos-ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos-normalize(pTarget->m_Pos-ProjStartPos)*m_ProximityRadius*0.65f);
					else
						GameServer()->CreateHammerHit(ProjStartPos);

					vec2 Dir;
					if (length(pTarget->m_Pos - m_Pos) > 0.0f)
						Dir = normalize(pTarget->m_Pos - m_Pos);
					else
						Dir = vec2(0.f, -1.f);

					if(pTarget->m_pPlayer->GetTeam() == TEAM_RED)
						pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -0.5f)) * 35.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), m_ActiveWeapon);
					else
						pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), m_ActiveWeapon);

					Hits++;
				}
			}

			// if we Hit anything, we have to wait for the reload
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed()/3;

		} break;

		case WEAPON_GUN:
		{
			CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GUN,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime),
				1, 0, 20, -1, WEAPON_GUN);

			// pack the Projectile and send it to the client Directly
			CNetObj_Projectile p;
			pProj->FillInfo(&p);

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(1);
			for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
				Msg.AddInt(((int *)&p)[i]);

			Server()->SendMsg(&Msg, 0, m_pPlayer->GetCID());

			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			int ShotSpread = 2;

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread*2+1);

			for(int i = -ShotSpread; i <= ShotSpread; ++i)
			{
				float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
				float a = GetAngle(Direction);
				a += Spreading[i+2];
				float v = 1-(absolute(i)/(float)ShotSpread);
				float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
				CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_SHOTGUN,
					m_pPlayer->GetCID(),
					ProjStartPos,
					vec2(cosf(a), sinf(a))*Speed,
					(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
					1, 0, 15, -1, WEAPON_SHOTGUN);

				// pack the Projectile and send it to the client Directly
				CNetObj_Projectile p;
				pProj->FillInfo(&p);

				for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
					Msg.AddInt(((int *)&p)[i]);
			}

			Server()->SendMsg(&Msg, 0,m_pPlayer->GetCID());

			GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GRENADE,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
				1, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

			// pack the Projectile and send it to the client Directly
			CNetObj_Projectile p;
			pProj->FillInfo(&p);

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(1);
			for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
				Msg.AddInt(((int *)&p)[i]);
			Server()->SendMsg(&Msg, 0, m_pPlayer->GetCID());

			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
		} break;

		case WEAPON_RIFLE:
		{
			new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID());
			GameServer()->CreateSound(m_Pos, SOUND_RIFLE_FIRE);
		} break;

		case WEAPON_NINJA:
		{
			// reset Hit objects
			m_NumObjectsHit = 0;

			m_Ninja.m_ActivationDir = Direction;
			m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
			m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

			GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE);
		} break;

	}

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0 && !g_Config.m_SvInfiniteAmmo) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
		m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / 1000;
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	// ammo regen
	int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Ammoregentime;
	if(m_Item == HITEM_GUN)
		AmmoRegenTime = AmmoRegenTime / 3;
	if(AmmoRegenTime)
	{
		// If equipped and not active, regen ammo?
		if (m_ReloadTimer <= 0)
		{
			if (m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

			if ((Server()->Tick() - m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_aWeapons[m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_ActiveWeapon].m_Ammo + 1, 10);
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}

	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if(m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// or are not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	if(m_pPlayer->m_ForceBalanced)
	{
		char Buf[128];
		str_format(Buf, sizeof(Buf), "You were moved to %s due to team balancing", GameServer()->m_pController->GetTeamName(m_pPlayer->GetTeam()));
		GameServer()->SendBroadcast(Buf, m_pPlayer->GetCID());

		m_pPlayer->m_ForceBalanced = false;
	}

	// save jumping state
	int Jumped = m_Core.m_Jumped;

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	CGameControllerZESC *pzESC = GameServer()->zESCController();

	if(g_Config.m_SvRegen > 0 && (Server()->Tick()%g_Config.m_SvRegen) == 0)
	{
		if(m_Health < 10)
			m_Health++;
		else if(m_Armor < 10)
			m_Armor++;
	}

	// tile index
	int TileIndex = GameServer()->Collision()->GetIndex(m_PrevPos, m_Pos);

	int z = GameServer()->Collision()->IsHoldpoint(m_Pos);
	int ct = -1;
	if(z != -1 && m_Alive)
		pzESC->OnHoldpoint(z);

	z = GameServer()->Collision()->IsZStop(m_Pos);
	if(z != -1 && m_pPlayer && m_pPlayer->GetTeam() == TEAM_BLUE && m_Alive)
		pzESC->OnZStop(z);

	z = GameServer()->Collision()->IsTrigger(m_Pos, m_pPlayer->GetTeam());
	if(z != -1 && m_Alive)
		GameServer()->m_pController->OnTrigger(z);

	z = GameServer()->Collision()->IsCustomTeleport(m_Pos);
	if(z != -1 && m_Alive)
		ct = GameServer()->m_pController->OnCustomTeleporter(z, m_pPlayer->GetTeam());
	if(ct != -1)
	{
		// check double jump
		if(Jumped&3 && m_Core.m_Jumped != Jumped)
			m_Core.m_Jumped = Jumped;

		m_Core.m_HookedPlayer = -1;
		m_Core.m_HookState = HOOK_RETRACTED;
		m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_Core.m_HookState = HOOK_RETRACTED;
		m_Core.m_Pos = pzESC->m_pTeleporter[ct];
		m_Core.m_HookPos = m_Core.m_Pos;
		//Resetting velocity to prevent exploit
		if(g_Config.m_SvTeleportVelReset)
			m_Core.m_Vel = vec2(0,0);
		if(g_Config.m_SvStrip)
		{
			m_ActiveWeapon = WEAPON_GUN;
			m_LastWeapon = WEAPON_HAMMER;
			m_aWeapons[WEAPON_HAMMER].m_Got = true;
			m_aWeapons[WEAPON_GUN].m_Got = true;
			for(int i = 2; i < NUM_WEAPONS; i++)
				m_aWeapons[i].m_Got = false;
		}
	}
	if(GameServer()->Collision()->IsWeaponStrip(m_Pos))
	{
		if(m_ActiveWeapon != WEAPON_HAMMER && m_ActiveWeapon != WEAPON_GUN)
		{
			m_ActiveWeapon = WEAPON_GUN;
			m_LastWeapon = WEAPON_HAMMER;
		}
		for(int i = 2; i < NUM_WEAPONS; i++)
			m_aWeapons[i].m_Got = false;
		m_Ninja.m_CurrentMoveTime = 0;
	}

	if(m_ActiveWeapon == WEAPON_NINJA && GameServer()->Collision()->IsKatanaStrip(m_Pos))
	{
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = m_LastWeapon;
		if(m_ActiveWeapon == WEAPON_NINJA)
			m_ActiveWeapon = WEAPON_GUN;

		SetWeapon(m_ActiveWeapon);
		m_Ninja.m_CurrentMoveTime = 0;
	}

	else if(TileIndex != -1 && GameServer()->Collision()->GetCollision(TileIndex) == TILE_STOPL)
	{
		if(m_Core.m_Vel.x > 0)
		{
			if((int)GameServer()->Collision()->GetPos(TileIndex).x < (int)m_Core.m_Pos.x)
				m_Core.m_Pos.x = m_PrevPos.x;
			m_Core.m_Vel.x = 0;
		}
	}
	else if(TileIndex != -1 && GameServer()->Collision()->GetCollision(TileIndex) == TILE_STOPR)
	{
		if(m_Core.m_Vel.x < 0)
		{
			if((int)GameServer()->Collision()->GetPos(TileIndex).x > (int)m_Core.m_Pos.x)
				m_Core.m_Pos.x = m_PrevPos.x;
			m_Core.m_Vel.x = 0;
		}
	}
	else if(TileIndex != -1 && GameServer()->Collision()->GetCollision(TileIndex) == TILE_STOPB)
	{
		if(m_Core.m_Vel.y < 0)
		{
			if((int)GameServer()->Collision()->GetPos(TileIndex).y > (int)m_Core.m_Pos.y)
				m_Core.m_Pos.y = m_PrevPos.y;
			m_Core.m_Vel.y = 0;
		}
	}
	else if(TileIndex != -1 && GameServer()->Collision()->GetCollision(TileIndex) == TILE_STOPT)
	{
		if(m_Core.m_Vel.y > 0)
		{
			if((int)GameServer()->Collision()->GetPos(TileIndex).y < (int)m_Core.m_Pos.y)
				m_Core.m_Pos.y = m_PrevPos.y;
			if(Jumped&3 && m_Core.m_Jumped != Jumped) // check double jump
				m_Core.m_Jumped = Jumped;
			m_Core.m_Vel.y = 0;
		}
	}

	// handle speedup tiles
	int CurrentSpeedup = GameServer()->Collision()->IsSpeedup(TileIndex);
	bool SpeedupTouch = false;
	if(m_LastSpeedup != CurrentSpeedup && CurrentSpeedup > -1)
	{
		vec2 Direction;
		int Force;
		GameServer()->Collision()->GetSpeedup(TileIndex, &Direction, &Force);

		m_Core.m_Vel += Direction*Force;

		SpeedupTouch = true;
	}

	m_LastSpeedup = CurrentSpeedup;

	// handle teleporter
	z = GameServer()->Collision()->IsTeleport(TileIndex);
	if(g_Config.m_SvTeleport && z)
	{
		// check double jump
		if(Jumped&3 && m_Core.m_Jumped != Jumped)
			m_Core.m_Jumped = Jumped;

		m_Core.m_HookedPlayer = -1;
		m_Core.m_HookState = HOOK_RETRACTED;
		m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_Core.m_HookState = HOOK_RETRACTED;
		m_Core.m_Pos = pzESC->m_pTeleporter[z-1];
		m_Core.m_HookPos = m_Core.m_Pos;
		//Resetting velocity to prevent exploit
		if(g_Config.m_SvTeleportVelReset)
			m_Core.m_Vel = vec2(0,0);
		if(g_Config.m_SvStrip)
		{
			m_ActiveWeapon = WEAPON_GUN;
			m_LastWeapon = WEAPON_HAMMER;
			m_aWeapons[WEAPON_HAMMER].m_Got = true;
			m_aWeapons[WEAPON_GUN].m_Got = true;
			for(int i = 2; i < NUM_WEAPONS; i++)
				m_aWeapons[i].m_Got = false;
		}
	}

	if(m_FreezeTick)
	{
		m_FreezeTick--;
		m_Core.m_Vel = vec2(0.f, 0.f);
		m_Core.m_Pos = m_PrevPos;
	}
	if(m_BurnTick)
	{
		m_BurnTick--;
		m_Core.m_Vel *= 0.9;
		if(!(m_BurnTick%20))
			GameServer()->CreateExplosion(m_Core.m_Pos, GetPlayer()->GetCID(), WEAPON_GRENADE, true);
	}

	// set Position just in case it was changed
	m_Pos = m_Core.m_Pos;

	// handle death-tiles and leaving gamelayer
	if(!SpeedupTouch &&
		(GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameLayerClipped(m_Pos)))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	// handle Weapons
	HandleWeapons();

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;

	if(pzESC->m_NukeLaunched && !GameServer()->Collision()->IsBunker(m_Pos))
		m_pPlayer->Nuke();
	return;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.Move();
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	int Events = m_Core.m_TriggeredEvents;
	int Mask = CmaskAllExceptOne(m_pPlayer->GetCID());

	if(Events&COREEVENT_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Mask);

	if(Events&COREEVENT_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, CmaskAll());
	if(Events&COREEVENT_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Mask);
	if(Events&COREEVENT_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Mask);


	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	// we got to wait 0.5 secs before respawning
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());

	if(m_HookedItem)
		m_HookedItem->Reset();
	m_Item = 0;
	m_HookedItem = 0;
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	if(m_pPlayer->GetTeam() == TEAM_BLUE && (Weapon == WEAPON_GRENADE || Weapon == WEAPON_HAMMER))
		m_Core.m_Vel += Force;
	if(Weapon == WEAPON_GRENADE && From == m_pPlayer->GetCID() && m_pPlayer->GetTeam() == TEAM_BLUE && m_Item == HITEM_GRENADE)
		m_Core.m_Vel += Force; // Bigger rocketjump for upgraded grenade :3

	if(m_pPlayer->GetTeam() == TEAM_RED)
	{
		vec2 AddVel = vec2(0, 0);
		if(Weapon == WEAPON_HAMMER)
			m_Item ? AddVel = Force*0.7f : AddVel = Force;
		else if(Weapon == WEAPON_GUN)
		{
			if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item != HITEM_GUN)
				m_Item ? AddVel = Force*0.7f : AddVel = Force;
			else if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item == HITEM_GUN)
				m_Item ? AddVel = Force : AddVel = Force*1.35f;
		}
		else if(Weapon == WEAPON_SHOTGUN)
		{
			if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item != HITEM_SHOTGUN)
				m_Item ? AddVel = Force*0.7f : AddVel = Force;
			else if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item == HITEM_SHOTGUN)
				m_Item ? AddVel = Force : AddVel = Force*1.3f;
		}
		else if(Weapon == WEAPON_GRENADE)
		{
			if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item != HITEM_GRENADE)
			{
				m_Item ? AddVel = Force : AddVel = Force*2.0f;
				m_Item ? m_BurnTick = Server()->TickSpeed()*1.5f : m_BurnTick = Server()->TickSpeed()*2.0f;
			}
			else if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item == HITEM_GRENADE)
			{
				m_Item ? AddVel = Force*2 : AddVel = Force*3.0f;
				m_Item ? m_BurnTick = Server()->TickSpeed()*2.0f : m_BurnTick = Server()->TickSpeed()*3.0f;
			}
		}
		else if(Weapon == WEAPON_RIFLE)
		{
			if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item != HITEM_RIFLE)
				m_Item ? m_FreezeTick = Server()->TickSpeed()*1.0f : m_FreezeTick = Server()->TickSpeed()*1.5f;
			else if(GameServer()->m_apPlayers[From] && GameServer()->m_apPlayers[From]->GetCharacter() && GameServer()->m_apPlayers[From]->GetCharacter()->m_Item == HITEM_RIFLE)
				m_Item ? m_FreezeTick = Server()->TickSpeed()*1.5f : m_FreezeTick = Server()->TickSpeed()*2.0f;
		}
		if(m_BurnTick)
			AddVel *= 2.5f;
		m_Core.m_Vel += AddVel;
	}

	return false;
}

void CCharacter::SetZomb()
{
	m_ActiveWeapon = WEAPON_HAMMER;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	if(m_Item && (m_Item != HITEM_GUN || m_Item != HITEM_GRENADE))
	{
		GameServer()->SendBroadcast("Your human item transfered into a zombie item!", m_pPlayer->GetCID());
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);
		SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
		m_Item = ZITEM_HAMMER;
	}
	else
		m_Item = 0;
	
	if(m_HookedItem)
		m_HookedItem->Reset();
	m_HookedItem = 0;
	m_Armor = 0;
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;

	pCharacter->m_Weapon = m_ActiveWeapon;
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;
}

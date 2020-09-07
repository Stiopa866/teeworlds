/* copyright (c) 2007 rajh, teleporter */
/* copyright (c) 2011 BotoX, zombie escape mod */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <stdio.h>
#include <string.h>
#include <game/server/entities/flag.h>
#include <game/server/entities/door.h>
#include "zesc.h"

CGameControllerZESC::CGameControllerZESC(class CGameContext* pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "zESC";
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
	m_apFlags[TEAM_RED] = 0;
	m_apFlags[TEAM_BLUE] = 0;
	m_RoundStarted = false;
	m_NukeTick = 0;
	m_NukeLaunched = false;
	m_LevelEarned = false;
	for (int i = 0; i < 48; i++)
	{
		if (i < 32)
			m_Door[i].m_State = DOOR_CLOSED;
		else
			m_Door[i].m_State = DOOR_OPEN;
		m_Door[i].m_Tick = 120;
		m_Door[i].m_OpenTime = 10;
		m_Door[i].m_CloseTime = 3;
		m_Door[i].m_ReopenTime = 10;
	}
	
}

/*CGameControllerZESC::~CGameControllerZESC()
{
	delete[] m_pTeleporter;
}*/
void CGameControllerZESC::StartRound()
{
	ResetDoors();
	IGameController::StartRound();
	

}
void CGameControllerZESC::InitTeleporter()
{
	/*int ArraySize = 0;
	if (GameServer()->Collision()->Layers()->TeleLayer())
	{
		for (int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width * GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
		{
			// get the array size
			if (GameServer()->Collision()->m_pTele[i].m_Number > ArraySize)
				ArraySize = GameServer()->Collision()->m_pTele[i].m_Number;
		}
	}

	if (!ArraySize)
	{
		m_pTeleporter = 0x0;
		return;
	}

	m_pTeleporter = new vec2[ArraySize];
	mem_zero(m_pTeleporter, ArraySize * sizeof(vec2));

	// assign the values
	for (int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width * GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
	{
		if (GameServer()->Collision()->m_pTele[i].m_Number > 0 && GameServer()->Collision()->m_pTele[i].m_Type == TILE_TELEOUT)
			m_pTeleporter[GameServer()->Collision()->m_pTele[i].m_Number - 1] = vec2(i % GameServer()->Collision()->Layers()->TeleLayer()->m_Width * 32 + 16, i / GameServer()->Collision()->Layers()->TeleLayer()->m_Width * 32 + 16);
	}*/
}

/*bool CGameControllerZESC::OnEntity(int Index, vec2 Pos)
{
	/*if (IGameController::OnEntity(Index, Pos))
		return true;

	int Team = -1;
	if (Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if (Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if (Team == -1 || m_apFlags[Team])
		return false;

	if (Team == TEAM_BLUE)
		m_apFlags[TEAM_RED] = 0;

	CFlag* F = new CFlag(&GameServer()->m_World, Team, Pos);
	F->m_Pos = Pos;	
	m_apFlags[Team] = F;
	GameServer()->m_World.InsertEntity(F);
	return true;
}*/

void CGameControllerZESC::Tick()
{
	IGameController::Tick();
	//|| GameServer()->m_World.m_Paused

	if (!IsGameRunning())
		return;

	if (m_RoundEndTick)
	{
		m_RoundEndTick--;
		if (!m_RoundEndTick)
			EndRound();
	}

	DoWincheckMatch();

	for (int i = 0; i < 48; i++)
	{
		if (m_Door[i].m_Tick > 0)
		{
			if (m_Door[i].m_State == DOOR_CLOSED)
			{
				if (m_Door[i].m_Tick == Server()->Tick() + 5 * Server()->TickSpeed())
				{
					char aBuf[128];
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "even here");
					str_format(aBuf, sizeof(aBuf), "(All) Door %d opening in 5 seconds.", i + 1);
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
				}
				else if (m_Door[i].m_Tick <= Server()->Tick())
				{
					SetDoorState(i, DOOR_OPEN);
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "(All) Door %d is open. Run!", i + 1);
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
					m_Door[i].m_Tick = 0;
				}
			}
			else if (m_Door[i].m_Tick <= Server()->Tick())
			{
				if (m_Door[i].m_State == DOOR_ZCLOSING && i < 32)
				{
					SetDoorState(i, DOOR_ZCLOSED);
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "(Zombies) Door %d closed. Reopening in %d seconds.", i + 1, GetDoorTime(i));
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
					m_Door[i].m_Tick = Server()->Tick() + Server()->TickSpeed() * GetDoorTime(i);
				}
				else if (m_Door[i].m_State == DOOR_ZCLOSED && i < 32)
				{
					SetDoorState(i, DOOR_REOPENED);
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "(Zombies) Door %d is open. Run!", i + 1);
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
					m_Door[i].m_Tick = 0;
				}
				else if (m_Door[i].m_State == DOOR_ZCLOSING && i >= 32)
				{
					SetDoorState(i, DOOR_ZCLOSED);
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "(Zombies) ZDoor %d closed. Reopening in %d seconds.", i - 31, GetDoorTime(i));
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
					m_Door[i].m_Tick = Server()->Tick() + Server()->TickSpeed() * GetDoorTime(i);
				}
				else if (m_Door[i].m_State == DOOR_ZCLOSED && i >= 32)
				{
					SetDoorState(i, DOOR_REOPENED);
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "(Zombies) ZDoor %d is open. Run!", i - 31);
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
					m_Door[i].m_Tick = 0;
				}
			}
		}
	}

	/*if (m_NukeTick)
	{
		m_NukeTick--;
		char bBuf[128];
		str_format(bBuf, sizeof(bBuf), "Tango down in %.2f seconds!\n      Stay in the bunker!!!", m_NukeTick / (float)Server()->TickSpeed());
		GameServer()->SendBroadcast(bBuf, -1);
		if (!m_NukeTick)
		{
			GameServer()->SendBroadcast("Stay in the bunker!!!", -1);
			m_NukeLaunched = true;
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetCharacter() && !GameServer()->m_apPlayers[i]->GetCharacter()->m_InBunker)
					GameServer()->m_apPlayers[i]->Nuke();
			}
			//CheckZomb();
		}
	}*/

	// Flag
	/*if (m_apFlags[TEAM_BLUE])
	{
		CFlag* F = m_apFlags[TEAM_BLUE];

		CCharacter* apCloseCCharacters[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(F->m_Pos, CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; i++)
		{
			if (!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->m_Pos, apCloseCCharacters[i]->m_Pos, NULL, NULL))
				continue;

			if (apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_BLUE) // Humans Win :D
			{
				//GameServer()->SendBroadcast("Humans win!", -1);
				apCloseCCharacters[i]->GetPlayer()->m_Score += 10;
				EndRound();
			}
			else if (apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_RED) // Zombies win :(
			{
				//GameServer()->SendBroadcast("Zombies took over the World!", -1);
				EndRound();
			}
		}
	}
	else if (m_apFlags[TEAM_RED])
	{
		CFlag* F = m_apFlags[TEAM_RED];

		// flag hits death-tile or left the game layer, reset it
		if (!F->m_AtStand && (GameServer()->Collision()->GetCollisionAt(F->m_Pos.x, F->m_Pos.y) & CCollision::COLFLAG_DEATH || F->GameLayerClipped(F->m_Pos)))
		{
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
			GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
			F->Reset();
		}
		if (!F->GetCarrier() && !F->m_AtStand)
		{
			if (Server()->Tick() > F->m_DropTick + Server()->TickSpeed() * 30)
			{
				GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
				F->Reset();
			}
			else
			{
				F->m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
				GameServer()->Collision()->MoveBox(&F->m_Pos, &F->m_Vel, vec2(F->ms_PhysSize, F->ms_PhysSize), 0.5f);
			}
		}

		// update flag position
		if (F->GetCarrier())
			F->m_Pos = F->GetCarrier()->m_Core.m_Pos;
		else
		{
			CCharacter* apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(F->m_Pos, CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < Num; i++)
			{
				if (!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->m_Pos, apCloseCCharacters[i]->m_Pos, NULL, NULL))
					continue;

				// take the flag
				if (F->m_AtStand)
					F->m_GrabTick = Server()->Tick();

				F->m_AtStand = 0;
				F->m_pCarryingCharacter = apCloseCCharacters[i];
				m_NukeTick = Server()->TickSpeed() * g_Config.m_SvNukeTime;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s'",
					F->m_pCarryingCharacter->GetPlayer()->GetCID(),
					Server()->ClientName(F->m_pCarryingCharacter->GetPlayer()->GetCID()));
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

				for (int c = 0; c < MAX_CLIENTS; c++)
				{
					if (!GameServer()->m_apPlayers[c])
						continue;

					if (GameServer()->m_apPlayers[c]->GetTeam() == F->m_pCarryingCharacter->GetPlayer()->GetTeam())
						GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, GameServer()->m_apPlayers[c]->GetCID());
					else
						GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, GameServer()->m_apPlayers[c]->GetCID());
				}
				break;
			}
		}
	}*/
}

void CGameControllerZESC::ResetDoors()
{
	for (int i = 0; i < 48; i++)
	{
		if (i < 32)
		{
			m_Door[i].m_State = DOOR_CLOSED;
			m_Door[i].m_OpenTime = (i+1) * GameServer()->Server()->TickSpeed()*5;
		}
		else
			m_Door[i].m_State = DOOR_OPEN;
		m_Door[i].m_Tick =0;
	}
	char aBuf2[IO_MAX_PATH_LENGTH];
	str_format(aBuf2, sizeof(aBuf2), "%s.cfg", GameServer()->Config()->m_SvMap);
	GameServer()->Console()->ExecuteFile(aBuf2);
	for (int i = 0; i < 48; i++)
	{
		m_Door[i].m_Tick = m_Door[i].m_OpenTime;
	}
}

int CGameControllerZESC::DoorState(int Index)
{
	if (m_Door[Index].m_State!=NULL)
		return m_Door[Index].m_State;
	return DOOR_CLOSED;
}

void CGameControllerZESC::SetDoorState(int Index, int State)
{
	
	m_Door[Index].m_State = State;
}

bool CGameControllerZESC::NukeLaunched()
{
	if (m_NukeLaunched || m_NukeTick)
		return true;
	return false;
}

int CGameControllerZESC::GetDoorTime(int Index)
{
	if (m_Door[Index].m_State == DOOR_CLOSED)
		return m_Door[Index].m_OpenTime;
	else if (m_Door[Index].m_State == DOOR_ZCLOSING || m_Door[Index].m_State == DOOR_OPEN)
		return m_Door[Index].m_CloseTime;
	else if (m_Door[Index].m_State == DOOR_ZCLOSED)
		return m_Door[Index].m_ReopenTime;
	return 0;
}
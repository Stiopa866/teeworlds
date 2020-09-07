#ifndef GAME_SERVER_GAMEMODES_ZESC_H
#define GAME_SERVER_GAMEMODES_ZESC_H

#include <game/server/gamecontroller.h>
#include <game/server/gamecontext.h>
#include "game/server/entities/door.h"
class CGameControllerZESC : public IGameController
{
public:
	CGameControllerZESC(class CGameContext* pGameServer);
	//~CGameControllerZESC();
	
	struct Door
	{
		int m_State;
		int64 m_Tick;
		int m_OpenTime;
		int m_CloseTime;
		int m_ReopenTime;
	};
	Door m_Door[48];
	
	vec2* m_pTeleporter;
	class CFlag* m_apFlags[2];

	void InitTeleporter();
	
	bool m_RoundStarted;
	bool m_LevelEarned;
	int m_RoundEndTick;
	int m_NukeTick;
	bool m_NukeLaunched;

	virtual void ResetDoors();
	virtual int DoorState(int Index);
	virtual void SetDoorState(int Index, int State);
	virtual bool NukeLaunched();
	virtual int GetDoorTime(int Index);
	virtual void Tick();
	virtual void StartRound();
	//virtual void Snap();
	//virtual int OnCharacterDeath(class CCharacter* pVictim, class CPlayer* pKiller, int Weapon);
	//virtual bool OnEntity(int Index, vec2 Pos);
	/*virtual void DoTeamScoreWincheck();
	virtual void OnHoldpoint(int Index);
	virtual void OnZHoldpoint(int Index);
	virtual void OnZStop(int Index);
	virtual bool ZombStarted();
	virtual void StartZomb(bool Value);
	virtual void CheckZomb();
	virtual void OnEndRound();
	virtual int NumPlayers();
	virtual int NumZombs();
	virtual int NumHumans();
	virtual void Reset();
	
	*/
};
#endif
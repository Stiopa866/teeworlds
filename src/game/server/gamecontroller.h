/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <base/tl/array.h>
#include <engine/shared/protocol.h> // MAX_CLIENTS

enum
{
	TRIGGER_ONCE=0,
	TRIGGER_PLAYERONCE,
	TRIGGER_MULTI,
};

#include <generated/protocol.h>

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	vec2 m_aaSpawnPoints[64];
	int m_aNumSpawnPoints;
	vec2 m_aZSpawn;

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	// activity
	void DoActivityCheck();
	bool GetPlayersReadyState(int WithoutID = -1);
	void SetPlayersReadyState(bool ReadyState);
	void CheckReadyStates(int WithoutID = -1);

	// balancing
	enum
	{
		TBALANCE_CHECK=-2,
		TBALANCE_OK,
	};
	int m_aTeamSize[NUM_TEAMS];
	int m_UnbalancedTick;

	virtual bool CanBeMovedOnBalance(int ClientID) const;
	void CheckTeamBalance();
	void DoTeamBalance();

	// game
	enum EGameState
	{
		// internal game states
		IGS_WARMUP_GAME,		// warmup started by game because there're not enough players (infinite)
		IGS_WARMUP_USER,		// warmup started by user action via rcon or new match (infinite or timer)

		IGS_START_COUNTDOWN,	// start countown to unpause the game or start match/round (tick timer)

		IGS_GAME_PAUSED,		// game paused (infinite or tick timer)
		IGS_GAME_RUNNING,		// game running (infinite)
		
		IGS_END_MATCH,			// match is over (tick timer)
		IGS_END_ROUND,			// round is over (tick timer)
 	};
	EGameState m_GameState;
	int m_GameStateTimer;

	virtual bool DoWincheckMatch();		// returns true when the match is over
	virtual void DoWincheckRound() {};
	bool HasEnoughPlayers() const { return (IsTeamplay() && m_aTeamSize[TEAM_RED] > 0 && m_aTeamSize[TEAM_BLUE] > 0) || (!IsTeamplay() && m_aTeamSize[TEAM_RED] > 1); }
	void ResetGame();
	void SetGameState(EGameState GameState, int Timer=0);
	void StartMatch();
	void StartRound();

	// map
	char m_aMapWish[128];
	
	void CycleMap();

	// spawn
	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100,100);
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};
	vec2 m_aaSpawnPoints[3][64];
	int m_aNumSpawnPoints[3];
	
	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const;
	void EvaluateSpawnType(CSpawnEval *pEval, int Type) const;

	// team
	int ClampTeam(int Team) const;

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }

	// game
	int m_GameStartTick;
	int m_MatchCount;
	int m_RoundCount;
	int m_SuddenDeath;
	int m_aTeamscore[NUM_TEAMS];

	void EndMatch() { SetGameState(IGS_END_MATCH, TIMER_END); }
	void EndRound() { SetGameState(IGS_END_ROUND, TIMER_END/2); }


	// info
	int m_GameFlags;
	int m_UnbalancedTick;
	bool m_ForceBalanced;

	int m_LastZomb;
	int m_LastZomb2;

public:
	class CTimedEvent
	{
	public:
		float m_Time;
		int64 m_Tick;
		char *m_pAction;

		CTimedEvent() { m_pAction = 0; }
		~CTimedEvent()
		{
			if(m_pAction)
				delete[] m_pAction;
		}
		CTimedEvent(float Time, int64 Tick, const char *pAction)
		{
			m_Time = Time;
			m_Tick = Tick;
			// Saving memory ftw! xD
			int StrLen = str_length(pAction)+1;
			m_pAction = new char[StrLen];
			str_copy(m_pAction, pAction, StrLen);
		}
		const CTimedEvent &operator =(const CTimedEvent &Orig)
		{
			m_Time = Orig.m_Time;
			m_Tick = Orig.m_Tick;
			int StrLen = str_length(Orig.m_pAction)+1;
			m_pAction = new char[StrLen];
			str_copy(m_pAction, Orig.m_pAction, StrLen);
			return *this;
		}
	};
	array<CTimedEvent> m_lTimedEvents;

	struct CTriggeredEvent
	{
		bool m_State;
		int m_Type;
		bool m_aPlayerState[MAX_CLIENTS];
		char m_aAction[512];
		void Reset(bool Hard)
		{
			m_State = false;
			for(int i = 0; i < MAX_CLIENTS; i++)
				m_aPlayerState[i] = false;
			if(Hard)
			{
				m_Type = TRIGGER_ONCE;
				m_aAction[0] = '\0';
			}
		}
	};
	CTriggeredEvent *m_apTriggeredEvents[256];

	struct CCustomTeleport
	{
		int m_Teleport;
		int m_Team;
		void Reset()
		{
			m_Teleport = -1;
			m_Team = -1;
		}
	};
	CCustomTeleport *m_apCustomTeleport[256];

	char m_aaOnTeamWinEvent[3][512];

	const char *m_pGameType;
	struct CGameInfo
	{
		int m_MatchCurrent;
		int m_MatchNum;
		int m_ScoreLimit;
		int m_TimeLimit;
	} m_GameInfo;

	void UpdateGameInfo(int ClientID);

public:
	IGameController(class CGameContext *pGameServer);
	virtual ~IGameController();

	int m_aTeamscore[2];
	int m_ZombWarmup;
	int m_SuddenDeath;
	int m_RoundStartTick;

	void StartRound();
	void EndRound();
	void ChangeMap(const char *pToMap);

	bool IsFriendlyFire(int ClientID1, int ClientID2);

	bool IsForceBalanced();

	int ParseExec(char *pCommand, int Size);
	bool RegisterTimedEvent(float Time, const char *pCommand);
	void ResetEvents();
	bool RegisterTriggeredEvent(int ID, int Type, const char *pCommand);
	void OnTrigger(int ID, int TriggeredBy);
	bool RegisterCustomTeleport(int ID, int ToX, int Team);
	int OnCustomTeleporter(int ID, int Team);

	/*
		Function: on_CCharacter_death
			Called when a CCharacter in the world dies.

		Arguments:
			victim - The CCharacter that died.
			killer - The player that killed it.
			weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: on_CCharacter_spawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			chr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr);

	virtual void OnFlagReturn(class CFlag *pFlag);

	/*
		Function: on_entity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			pos - Where the entity is located in the world.

		Returns:
			bool?
	*/
	virtual bool OnEntity(int Index, vec2 Pos);

	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer);
	void OnPlayerInfoChange(class CPlayer *pPlayer);
	void OnPlayerReadyChange(class CPlayer *pPlayer);

	void OnReset();

	// game
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	void DoPause(int Seconds) { SetGameState(IGS_GAME_PAUSED, Seconds); }
	void DoWarmup(int Seconds)
	{
		if(m_GameState==IGS_WARMUP_GAME)
			SetGameState(IGS_WARMUP_GAME, 0);
		else
			SetGameState(IGS_WARMUP_USER, Seconds);
	}
	void SwapTeamscore();

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	// info
	void CheckGameInfo();
	bool IsFriendlyFire(int ClientID1, int ClientID2) const;
	bool IsGamePaused() const { return m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_START_COUNTDOWN; }
	bool IsGameRunning() const { return m_GameState == IGS_GAME_RUNNING; }
	bool IsPlayerReadyMode() const;
	bool IsTeamChangeAllowed() const;
	bool IsTeamplay() const { return m_GameFlags&GAMEFLAG_TEAMS; }
	
	const char *GetGameType() const { return m_pGameType; }
	
	// map
	void ChangeMap(const char *pToMap);

	//
	virtual bool CanSpawn(int Team, vec2 *pPos);
	virtual bool ZombieSpawn(vec2 *pOutPos);
	//spawn
	bool CanSpawn(int Team, vec2 *pPos) const;
	bool GetStartRespawnState() const;

	// team
	bool CanJoinTeam(int Team, int NotThisID) const;
	bool CanChangeTeam(CPlayer *pPplayer, int JoinTeam) const;

	*/
	virtual const char *GetTeamName(int Team);
	virtual int GetAutoTeam(int NotThisID);
	virtual bool CanJoinTeam(int Team, int NotThisID);
	int ClampTeam(int Team);
	void ZombWarmup(int W);
	void RandomZomb(int Mode);

	virtual void PostReset();
};

#endif

#ifndef GAME_SERVER_ENTITIES_HARPOON_H
#define GAME_SERVER_ENTITIES_HARPOON_H

#include <game/server/entity.h>

enum
{
	HARPOON_FLYING,
	HARPOON_RETRACTING,
	HARPOON_IN_GROUND,
	HARPOON_IN_CHARACTER,
};

class CHarpoon : public CEntity
{
public:
	CHarpoon(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Owner, CCharacter* This);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
	void Drag();
	void RemoveHarpoon();
	void DeallocateOwner();
	void DeallocateVictim();
	void FillInfo(CNetObj_Harpoon* pHarpoon);
	int m_Grounded;
protected:
	//bool HitCharacter(vec2 From, vec2 To);
	//void DoBounce();

private:
	//vec2 m_From;
	//vec2 m_Dir;
	//float m_Energy;
	//int m_Bounces;
	//int m_EvalTick;
	CCharacter* m_pOwnerChar;
	CCharacter* m_pVictim;
	int m_SpawnTick;
	int m_DeathTick=-1;
	vec2 m_Vel;
	int m_Owner;
};

#endif

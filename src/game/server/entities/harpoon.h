#ifndef GAME_SERVER_ENTITIES_HARPOON_H
#define GAME_SERVER_ENTITIES_HARPOON_H

#include <game/server/entity.h>

class CHarpoon : public CEntity
{
public:
	CHarpoon(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Owner);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
	void FillInfo(CNetObj_Harpoon* pHarpoon);
protected:
	//bool HitCharacter(vec2 From, vec2 To);
	//void DoBounce();

private:
	//vec2 m_From;
	//vec2 m_Dir;
	//float m_Energy;
	//int m_Bounces;
	//int m_EvalTick;
	bool m_Grounded;
	CCharacter* Owner;
	int m_SpawnTick;
	vec2 m_Vel;
	int m_Owner;
};

#endif

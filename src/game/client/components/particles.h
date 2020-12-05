/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PARTICLES_H
#define GAME_CLIENT_COMPONENTS_PARTICLES_H
#include <base/vmath.h>
#include <game/client/component.h>

// particles

enum ParticleFlags
{
	PFLAG_NONE = 0,
	PFLAG_DESTROY_IN_AIR = 1, // Particle is destroyed in air
	PFLAG_DESTROY_IN_WATER = 2, // Particle is destroyed in water tile
	PFLAG_DESTROY_ON_IMPACT = 4, //Particle is destroyed on impact with a solid tile
	PFLAG_DESTROY_IN_ANIM_WATER = 8, //Particle is destroyed in animwater water (under surface)
};

struct CParticle
{
	void SetDefault()
	{
		m_Vel = vec2(0,0);
		m_LifeSpan = 0;
		m_StartSize = 32;
		m_EndSize = 32;
		m_Rot = 0;
		m_Rotspeed = 0;
		m_Gravity = 0;
		m_Friction = 0;
		m_Flags = PFLAG_NONE;
		m_FlowAffected = 1.0f;
		m_Color = vec4(1,1,1,1);
		m_Water = false;
		m_BubbleStage = 0;
		m_RotationByVel = false;
	}

	vec2 m_Pos;
	vec2 m_Vel;

	int m_Spr;

	float m_FlowAffected;

	float m_LifeSpan;

	float m_StartSize;
	float m_EndSize;

	float m_Rot;
	float m_Rotspeed;

	float m_Gravity;
	float m_Friction;

	vec4 m_Color;
	
	bool m_RotationByVel;
	bool m_Water;
	int m_Flags;
	int m_BubbleStage;
	// set by the particle system
	float m_Life;
	int m_PrevPart;
	int m_NextPart;
};

class CParticles : public CComponent
{
	friend class CGameClient;
public:
	enum
	{
		GROUP_PROJECTILE_TRAIL=0,
		GROUP_EXPLOSIONS,
		GROUP_GENERAL,
		NUM_GROUPS
	};

	CParticles();

	void Add(int Group, CParticle *pPart);

	virtual void OnReset();
	virtual void OnRender();

private:

	enum
	{
		MAX_PARTICLES=1024*8,
	};

	CParticle m_aParticles[MAX_PARTICLES];
	int m_FirstFree;
	int m_aFirstPart[NUM_GROUPS];

	void RenderGroup(int Group);
	void Update(float TimePassed);
	void RemoveParticle(int Group, int Entry);

	template<int TGROUP>
	class CRenderGroup : public CComponent
	{
	public:
		CParticles *m_pParts;
		virtual void OnRender() { m_pParts->RenderGroup(TGROUP); }
	};

	CRenderGroup<GROUP_PROJECTILE_TRAIL> m_RenderTrail;
	CRenderGroup<GROUP_EXPLOSIONS> m_RenderExplosions;
	CRenderGroup<GROUP_GENERAL> m_RenderGeneral;
};
#endif

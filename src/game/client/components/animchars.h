#ifndef GAME_CLIENT_COMPONENTS_ANIMCHARS_H
#define GAME_CLIENT_COMPONENTS_ANIMCHARS_H
#include <game/client/component.h>

enum {
	ANIMCHARLIFETIME = 2500,
	ANIMCHARPOSITIONLOSS = 64,
	ANIMCHARROTATIONLOSS = 3
};
class CAnimChars : public CComponent
{
	struct AnimatedChar {
		bool m_Exists;
		vec2 m_Pos;
		float m_Rotation;
		//vec2 m_Size;
		//vec4 m_Properties;
		CTeeRenderInfo m_TeeRenderInfo;
		int m_Lifetime;
	} m_AnimatedChars[16];
public:
	//CAnimChars();
	void Add(vec2 Pos, CTeeRenderInfo RenderInfo, float Angle);

	//virtual void Tick();
	virtual void OnRender();
	float GetEffectsSpeed();
};


#endif
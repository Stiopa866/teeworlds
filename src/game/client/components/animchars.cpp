#include <engine/graphics.h>
#include <engine/demo.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/ui.h>
#include <game/client/render.h>

#include <game/client/components/flow.h>
#include <game/client/components/animchars.h>

void CAnimChars::OnRender()
{
	for (int i = 0; i < 15;i++)
	{
		if (m_AnimatedChars[i].m_Exists)
		{
			float Fraction = (m_AnimatedChars[i].m_Lifetime * 1.0f / ANIMCHARLIFETIME);
			vec2 CurrentPos = m_AnimatedChars[i].m_Pos;
			CurrentPos.y += 64-(Fraction * ANIMCHARPOSITIONLOSS);
			char aBuf[64];
			vec2 Direction = direction(1 + (1 - Fraction) * ANIMCHARROTATIONLOSS / 10);
			str_format(aBuf, sizeof(aBuf), "%f, %f, %f",  1 + (1 - Fraction) * ANIMCHARROTATIONLOSS / 10, Direction.x, Direction.y);
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "debug", aBuf);//CurrentPos
		//	RenderTools()->RenderTee(CAnimState::GetIdle(), &m_AnimatedChars[i].m_TeeRenderInfo, 1, vec2(0, 0), );
			RenderTools()->RenderWaterTee(CAnimState::GetIdle(), &m_AnimatedChars[i].m_TeeRenderInfo, 1 + (1 - Fraction) * ANIMCHARROTATIONLOSS / 10, CurrentPos , m_AnimatedChars[i].m_Rotation, Fraction);
			//Graphics()->BlendNormal();
			//Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			//Graphics()->QuadsBegin();
			//RenderTools()->SelectSprite(SPRITE_PICKUP_ARMOR);
			//RenderTools()->DrawSprite(m_AnimatedChars[i].m_Pos.x, m_AnimatedChars[i].m_Pos.y, m_AnimatedChars[i].m_TeeRenderInfo.m_Size);
		//	Graphics()->QuadsEnd();
			m_AnimatedChars[i].m_Lifetime--;
			if (!m_AnimatedChars[i].m_Lifetime)
			{
				m_AnimatedChars[i].m_Exists = false;
			}
			//m_AnimatedChars[i].m_Pos.x -= 0.02f;
			
		}
	}
}

void CAnimChars::Add(vec2 Pos, CTeeRenderInfo RenderInfo, float Angle)
{
	for (int i = 0; i < 15; i++)
	{
		if (!m_AnimatedChars[i].m_Exists)
		{
			m_AnimatedChars[i].m_Lifetime = ANIMCHARLIFETIME;
			m_AnimatedChars[i].m_Exists = true;
			m_AnimatedChars[i].m_Pos = Pos;
			m_AnimatedChars[i].m_Rotation = Angle;
			m_AnimatedChars[i].m_TeeRenderInfo = RenderInfo;
			return;
		}
	}
}

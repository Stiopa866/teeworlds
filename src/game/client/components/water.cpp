#include "water.h"

#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>
#include <generated/client_data.h>
#include <engine/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/effects.h>
#include <engine/shared/config.h>

enum {
	DESIRED_HEIGHT = 16
};

void CWater::Init()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Init Called");
	m_pLayers = Layers();
	if (!m_pLayers->WaterLayer())
		return;
	int NumOfSurfaces = 0;
	int Length;

	m_pWaterTiles = static_cast<CTile*>(m_pLayers->Map()->GetData(m_pLayers->WaterLayer()->m_Data));
	int m_Width = m_pLayers->WaterLayer()->m_Width;
	int m_Height = m_pLayers->WaterLayer()->m_Height;
	
	
	//Algorithm to fetch the amount of 'Surface' Areas
	for (int i = m_Width; i < m_Width * m_Height; i++)
	{
		if (m_pWaterTiles[i].m_Index==1&&!m_pWaterTiles[i-m_Width].m_Index==1)
		{
			Length = AmountOfSurfaceTiles(i, m_Width);
			NumOfSurfaces++;
			i += Length;
		}
	}
	m_NumOfSurfaces = NumOfSurfaces;
	m_aWaterSurfaces = new CWaterSurface * [NumOfSurfaces];
	NumOfSurfaces = 0;
	for (int i = m_Width; i < m_Width * m_Height; i++)
	{
		if (m_pWaterTiles[i].m_Index == 1 && !m_pWaterTiles[i - m_Width].m_Index == 1)
		{
			Length = AmountOfSurfaceTiles(i, m_Width);
			m_aWaterSurfaces[NumOfSurfaces] = new CWaterSurface(i % m_Width, i / m_Width, Length);
			NumOfSurfaces++;
			i += Length;
		}
	}
}

void CWater::OnReset()
{
	if (Client()->State() == IClient::STATE_OFFLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK || !Config()->m_GfxAnimateWater)
	{
		m_pLayers = 0x0;
		m_pWaterTiles = 0x0;
		for (int i = 0; i < m_NumOfSurfaces; i++)
		{
			m_aWaterSurfaces[i]->Remove();
		}
		m_NumOfSurfaces = 0;

		delete[] m_aWaterSurfaces;
		m_aWaterSurfaces = 0;
	}
}

int CWater::AmountOfSurfaceTiles(int Coord, int End)
{
	if (!(m_pWaterTiles[Coord].m_Index == 1 &!m_pWaterTiles[Coord - End].m_Index == 1))
		return 0;
	//Check if we are not at the end of the x coordinate
	if (!((Coord+1) % End))
		return 1;
	//If we are not, we should check another tile
	else
		return (1 + AmountOfSurfaceTiles(Coord + 1, End));
}

void CWater::Render()
{
	m_pLayers = Layers();
	if (!m_pLayers->WaterLayer())
		return;
	Tick();
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WATER].m_Id);
	Graphics()->QuadsBegin();

	int a = m_NumOfSurfaces ? m_NumOfSurfaces : 0;
	Graphics()->SetColor(0.5, 0.5, 0.5, 0.5);
	for (int i = 0;i < a;i++)
	{
		float XBasePos = m_aWaterSurfaces[i]->m_Coordinates.m_X * 32.0f;
		float YBasePos = (float)m_aWaterSurfaces[i]->m_Coordinates.m_Y * 32.0f;
		RenderTools()->SelectSprite(SPRITE_WATER30);
		for (int j = 0; j < m_aWaterSurfaces[i]->m_AmountOfVertex - 1; j++)
		{
			for (int k = j + 1; k < m_aWaterSurfaces[i]->m_AmountOfVertex - 1; k++)
			{
				if (m_aWaterSurfaces[i]->m_aVertex[j]->m_Height == m_aWaterSurfaces[i]->m_aVertex[k]->m_Height  && k+1!= m_aWaterSurfaces[i]->m_AmountOfVertex - 1)
				{
					continue;
				}
				else
				{
					if (!(j == k - 1))
					{
						WaterFreeform(XBasePos, YBasePos, j, k - 1, 4, m_aWaterSurfaces[i]);
					}
					j += k - 1 - j;
					break;
				}
			}
			WaterFreeform(XBasePos, YBasePos, j+1, j, 4, m_aWaterSurfaces[i]);
		}
		//WaterFreeform(XBasePos, YBasePos, m_aWaterSurfaces[i]->m_AmountOfVertex - 1, m_aWaterSurfaces[i]->m_AmountOfVertex -2, 4, m_aWaterSurfaces[i]);
	}
	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}
void CWater::WaterFreeform(float X, float Y, int A, int B, float Size, CWaterSurface* Surface)
{
	IGraphics::CFreeformItem Item(
		X + A * Size, //bottom left corner
		Y + 32.0f,
		X + B * Size, //bottom right corner
		Y + 32.0f, 
		X + A * 4, //top left corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[A]->m_Height - 16.0f) * Surface->m_Scale,
		X + B * 4, //top right corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[B]->m_Height - 16.0f) * Surface->m_Scale

	);
	IGraphics::CFreeformItem Item2(
		X + A * Size, //bottom left corner
		Y + 32.0f,
		X + B * Size, //bottom right corner
		Y + 32.0f,
		X + A * 4, //top left corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[A]->m_Height - 16.0f) * Surface->m_Scale,
		X + B * 4, //top right corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[B]->m_Height - 16.0f) * Surface->m_Scale

	);
	IGraphics::CFreeformItem Item3(
		X + A * Size, //bottom left corner
		Y + 32.0f,
		X + B * Size, //bottom right corner
		Y + 32.0f,
		X + A * 4, //top left corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[A]->m_Height - 16.0f) * Surface->m_Scale,
		X + B * 4, //top right corner
		Y + 32.0f - 16.0f - (Surface->m_aVertex[B]->m_Height - 16.0f) * Surface->m_Scale

	);
	Graphics()->QuadsDrawFreeform(&Item, 1);
	//Graphics()->QuadsDrawFreeform(&Item2, 1);
	//Graphics()->QuadsDrawFreeform(&Item3, 1);
}

void CWater::HitWater(float x, float y, float Force)
{
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "HitWater");
	CWaterSurface* Target;
	if(FindSurface(&Target, x, y))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "HitWater2");
		Target->HitWater(x, y, Force);
		if (absolute(Force) >= 5)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Droplet Spawn2");
			m_pClient->m_pEffects->Droplet(vec2(x, y), vec2(0, Force));
		}
	}
}

bool CWater::FindSurface(CWaterSurface** Pointer, float x, float y)
{
	for (int i = 0; i < m_NumOfSurfaces;i++)
	{
		if (y < m_aWaterSurfaces[i]->m_Coordinates.m_Y * 32.0f + 32.0f)
		{
			if (y > m_aWaterSurfaces[i]->m_Coordinates.m_Y * 32.0f)
			{
				if (x > m_aWaterSurfaces[i]->m_Coordinates.m_X * 32.0f && x < (m_aWaterSurfaces[i]->m_Coordinates.m_X + m_aWaterSurfaces[i]->m_Length) * 32.0f)
				{
					*Pointer = m_aWaterSurfaces[i];

					return true;
				}
				else
					continue;
			}
			else
				return false; //the explanation is a bit complicated
		}
		else
			continue;
	}
	return false;
}

bool CWater::IsUnderWater(vec2 Pos)
{
	CWaterSurface* Target;
	if (FindSurface(&Target, Pos.x, Pos.y))
	{
		return Target->IsUnderWater(Pos);
	}
	else
	{
		return m_pClient->Collision()->TestBox(Pos, vec2(1.0f, 1.0f), 8);
	}
	//return false;
}

void CWater::Tick()
{
	if (!Config()->m_GfxAnimateWater)
	{
		OnReset();
		return;
	}
	for (int i = 0; i < m_NumOfSurfaces;i++)
	{
		m_aWaterSurfaces[i]->Tick();
	}
}
CWaterSurface::CWaterSurface(int X, int Y, int Length)
{
	m_Coordinates.m_X = X;
	m_Coordinates.m_Y = Y;
	m_Length = Length;
	m_AmountOfVertex = Length * 8+1;
	m_aVertex = new CVertex * [m_AmountOfVertex];
	for (int i = 0; i < m_AmountOfVertex; i++)
	{
		m_aVertex[i] = new CVertex(16.0f);
	}
}

void CWaterSurface::HitWater(float x, float y, float Force)
{
	int RealX = (int)(x - m_Coordinates.m_X * 32.0f);
	RealX /= 4;
	m_aVertex[RealX]->m_Velo += Force;
}

float CWaterSurface::PositionOfVertex(float Height, bool ToScale)
{
	float Scale = ToScale ? m_Scale : 1.0f;
	return m_Coordinates.m_Y * 32.0f + 32.0f - 16.0f - (Height - 16.0f) * Scale;
}

bool CWaterSurface::IsUnderWater(vec2 Pos)
{

	int RealX = (int)(Pos.x - m_Coordinates.m_X * 32.0f);
	RealX /= 4;
	if (PositionOfVertex(m_aVertex[RealX]->m_Height) < Pos.y)
	{
		//char aBuf[128];
		//str_format(aBuf, sizeof(aBuf), "%f %f", PositionOfVertex(m_aVertex[RealX]->m_Height, Pos.y));
		//Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		if (RealX + 1 == m_AmountOfVertex)
		{
			return true;
		}
		else if (PositionOfVertex(m_aVertex[RealX+1]->m_Height) < Pos.y)
		{
			return true;
		}
	}
	return false;
}

void CWaterSurface::Tick()
{
	for (int i = 0; i < m_AmountOfVertex; i++)
	{
		m_aVertex[i]->m_Height += m_aVertex[i]->m_Velo;
		m_aVertex[i]->m_Velo += (m_aVertex[i]->m_Height - DESIRED_HEIGHT * 1.0f) * -0.005f - 0.005f* m_aVertex[i]->m_Velo;
	}

	float Spread = 0.1;

	//makes the waves spread
	float* LeftDelta = new float[m_AmountOfVertex];
	float* RightDelta = new float[m_AmountOfVertex];
	//float LeftDelta[1000];
	//float RightDelta[1000];
	for (int j = 0; j < 8; j++)
	{
		for (int i = 0; i < m_AmountOfVertex; i++)
		{
			if (i > 0)
			{
				LeftDelta[i] = Spread * (m_aVertex[i]->m_Height - m_aVertex[i - 1]->m_Height);
				m_aVertex[i - 1]->m_Velo += LeftDelta[i];
			}
			if (i < m_AmountOfVertex - 1)
			{
				RightDelta[i] = Spread * (m_aVertex[i]->m_Height - m_aVertex[i + 1]->m_Height);
				m_aVertex[i + 1]->m_Velo += RightDelta[i];
			}
		}
		for (int i = 0; i < m_AmountOfVertex; i++)
		{
			if (i > 0)
				m_aVertex[i - 1]->m_Height += LeftDelta[i];
			if (i < m_AmountOfVertex - 1)
				m_aVertex[i + 1]->m_Height += RightDelta[i];

		}
	}
	float MaxHeight = 0;
	float Scale = 1;
	for (int i = 0; i < m_AmountOfVertex; i++)
	{
		if (absolute(m_aVertex[i]->m_Height-16) > MaxHeight)
			MaxHeight = absolute(m_aVertex[i]->m_Height-16.0f);
	}
	if (MaxHeight > 16.0f)
	{
		Scale = 16.0f / MaxHeight;
	}
	m_Scale = Scale;
}

CVertex::CVertex(float Height)
{
	m_Height = Height;
	m_Velo = 0;
}

void CWaterSurface::Remove()
{
	delete[] m_aVertex;
}

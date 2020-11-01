/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

class CCollision
{
	class CTile* m_pTiles;
	
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	class CWater* m_pWater;

	bool IsTile(int x, int y, int Flag=COLFLAG_SOLID) const;
	bool IsAirTile(int x, int y, int Flag = COLFLAG_SOLID) const;
	int GetTile(int x, int y) const;
public:
	class CTile* m_pCollisionTiles;
	enum
	{
		COLFLAG_SOLID = 1,
		COLFLAG_DEATH = 2,
		COLFLAG_NOHOOK = 4,
		COLFLAG_WATER = 8,
	};

	CCollision();
	void Init(class CLayers* pLayers, class CWater* pWater=0x0);
	//void InitializeWater();
	bool CheckPoint(float x, float y, int Flag=COLFLAG_SOLID) const
	{
		int Tx = round_to_int(x);
		int Ty = round_to_int(y);
		if (Flag == 8)
		{
			if (IsTile(Tx, Ty - 32, 8)) //water above carry on
			{
			}
			else
			{ //boi u done goofed
				if (Ty % 32 < 16)
				{
					return false;
				}
			}
		}
		return IsTile(Tx, Ty, Flag);
	}
	bool CheckWaterPoint(float x, float y, int Flag = COLFLAG_SOLID) const { return IsAirTile(round_to_int(x), round_to_int(y), Flag); }
	bool CheckPoint(vec2 Pos, int Flag=COLFLAG_SOLID) const { return CheckPoint(Pos.x, Pos.y, Flag); }
	bool CheckWaterPoint(vec2 Pos, int Flag = COLFLAG_SOLID) const { return CheckWaterPoint(Pos.x, Pos.y, Flag); }
	int GetCollisionAt(float x, float y) const { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWaterCollisionAt(float x, float y, int Flag) const { return IsAirTile(round_to_int(x), round_to_int(y), Flag); }
	int GetWidth() const { return m_Width; };
	int GetHeight() const { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	int IntersectLineWithWater(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int Flag) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void Diffract(vec2* pInoutPos, vec2* pInoutVel, float Elasticity, int* pBounces, int Flag) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool *pDeath=0) const;
	void MoveWaterBox(vec2* pInoutPos, vec2* pInoutVel, vec2 Size, float Elasticity, bool* pDeath = 0, float Severity = 0.95) const;
	bool TestBox(vec2 Pos, vec2 Size, int Flag=COLFLAG_SOLID) const;
	bool TestWaterBox(vec2 Pos, vec2 Size, int Flag = COLFLAG_SOLID) const;
};

#endif

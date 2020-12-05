#ifndef WATER_COMPONENT_H
#define WATER_COMPONENT_H
#include <game/client/component.h>

class CWater : public CComponent
{
public:
	CWater() {};
	
	class CLayers* m_pLayers;
	class CWaterSurface** m_aWaterSurfaces;
	class CTile* m_pWaterTiles;
	
	virtual void OnMapLoad() { Init(); }
	void Render();
	virtual void OnReset();
	void Init();
	void Tick();

	int AmountOfSurfaceTiles(int Coord, int End);
	void HitWater(float x, float y, float Force);
	void WaterFreeform(float X, float Y, int A, int B, float Size, CWaterSurface* Surface);
	bool IsUnderWater(vec2 Pos);
	bool FindSurface(CWaterSurface** Pointer, float x, float y);

	int m_NumOfSurfaces;
};


class CWaterSurface
{
public:
	CWaterSurface(int X, int Y, int Length);

	struct Coordinates
	{
		int m_X;
		int m_Y;
	} m_Coordinates;

	class CVertex** m_aVertex;
	int m_AmountOfVertex;
	int m_Length;
	float m_Scale;

	bool IsUnderWater(vec2 Pos);
	void Remove();
	void Tick();
	void HitWater(float x, float y, float Force);
	float PositionOfVertex(float Height, bool ToScale = true);
};

class CVertex
{
public:
	CVertex(float Height);
	float m_Velo;
	float m_Height;
};
#endif
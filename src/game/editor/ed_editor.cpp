// copyright (c) 2007 magnus auvinen, see licence.txt for more info

#include <base/system.h>
#include <base/tl/sorted_array.h>
#include <base/tl/string.h>

#include <engine/shared/datafile.h>
#include <engine/shared/config.h>
#include <engine/shared/engine.h>
#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/storage.h>

#include <game/client/ui.h>
#include <game/gamecore.h>
#include <game/client/render.h>

#include "ed_editor.h"
#include <game/client/lineinput.h>

#include <game/localization.h>

int CEditor::ms_CheckerTexture;
int CEditor::ms_BackgroundTexture;
int CEditor::ms_CursorTexture;
int CEditor::ms_EntitiesTexture;
const void* CEditor::ms_pUiGotContext;

enum
{
	BUTTON_CONTEXT=1,
};

CEditorImage::~CEditorImage()
{
	m_pEditor->Graphics()->UnloadTexture(m_TexId);
}

CLayerGroup::CLayerGroup()
{
	m_pName = "";
	m_Visible = true;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	Clear();
}

void CLayerGroup::Convert(CUIRect *pRect)
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints)
{
	m_pMap->m_pEditor->RenderTools()->MapscreenToWorld(
		m_pMap->m_pEditor->m_WorldOffsetX, m_pMap->m_pEditor->m_WorldOffsetY,
		m_ParallaxX/100.0f, m_ParallaxY/100.0f,
		m_OffsetX, m_OffsetY,
		m_pMap->m_pEditor->Graphics()->ScreenAspect(), m_pMap->m_pEditor->m_WorldZoom, pPoints);

	pPoints[0] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[1] += m_pMap->m_pEditor->m_EditorOffsetY;
	pPoints[2] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[3] += m_pMap->m_pEditor->m_EditorOffsetY;
}

void CLayerGroup::MapScreen()
{
	float aPoints[4];
	Mapping(aPoints);
	m_pMap->m_pEditor->Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();
	IGraphics *pGraphics = m_pMap->m_pEditor->Graphics();

	if(m_UseClipping)
	{
		float aPoints[4];
		m_pMap->m_pGameGroup->Mapping(aPoints);
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2]-aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3]-aPoints[1]);
		float x1 = ((m_ClipX+m_ClipW) - aPoints[0]) / (aPoints[2]-aPoints[0]);
		float y1 = ((m_ClipY+m_ClipH) - aPoints[1]) / (aPoints[3]-aPoints[1]);

		pGraphics->ClipEnable((int)(x0*pGraphics->ScreenWidth()), (int)(y0*pGraphics->ScreenHeight()),
			(int)((x1-x0)*pGraphics->ScreenWidth()), (int)((y1-y0)*pGraphics->ScreenHeight()));
	}

	for(int i = 0; i < m_lLayers.size(); i++)
	{
		if(m_lLayers[i]->m_Visible && m_lLayers[i] != m_pMap->m_pGameLayer)
		{
			if(m_pMap->m_pEditor->m_ShowDetail || !(m_lLayers[i]->m_Flags&LAYERFLAG_DETAIL))
				m_lLayers[i]->Render();
		}
	}

	pGraphics->ClipDisable();
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= m_lLayers.size()) return;
	delete m_lLayers[Index];
	m_lLayers.remove_index(Index);
}

void CLayerGroup::GetSize(float *w, float *h)
{
	*w = 0; *h = 0;
	for(int i = 0; i < m_lLayers.size(); i++)
	{
		float lw, lh;
		m_lLayers[i]->GetSize(&lw, &lh);
		*w = max(*w, lw);
		*h = max(*h, lh);
	}
}


int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= m_lLayers.size()) return Index0;
	if(Index1 < 0 || Index1 >= m_lLayers.size()) return Index0;
	if(Index0 == Index1) return Index0;
	swap(m_lLayers[Index0], m_lLayers[Index1]);
	return Index1;
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	int tw = m_Width/16; // tilesizes
	int th = m_Height/16;
	if ( tw == th )
    {
		unsigned char *pPixelData = (unsigned char *)m_pData;

		int TileId = 0;
		for(int ty = 0; ty < 16; ty++)
			for(int tx = 0; tx < 16; tx++, TileId++)
			{
				bool Opaque = true;
				for(int x = 0; x < tw; x++)
					for(int y = 0; y < th; y++)
					{
						int p = (ty*tw+y)*m_Width + tx*tw+x;
						if(pPixelData[p*4+3] < 250)
						{
							Opaque = false;
							break;
						}
					}

				if(Opaque)
					m_aTileFlags[TileId] |= TILEFLAG_OPAQUE;
			}
	}

}

/********************************************************
 OTHER
*********************************************************/

// copied from gc_menu.cpp, should be more generalized
//extern int ui_do_edit_box(void *id, const CUIRect *rect, char *str, int str_size, float font_size, bool hidden=false);

int CEditor::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, bool Hidden)
{
    int Inside = UI()->MouseInside(pRect);
	int ReturnValue = 0;
	static int s_AtIndex = 0;

	if(UI()->LastActiveItem() == pID)
	{
		int Len = str_length(pStr);

		if(Inside && UI()->MouseButton(0))
		{
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for (int i = 1; i <= Len; i++)
			{
				if (TextRender()->TextWidth(0, FontSize, pStr, i) + 10 > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if (i == Len)
					s_AtIndex = Len;
			}
		}

		for(int i = 0; i < Input()->NumEvents(); i++)
		{
			Len = str_length(pStr);
			CLineInput::Manipulate(Input()->GetEvent(i), pStr, StrSize, &Len, &s_AtIndex);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if (UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, vec4(1,1,1,0.5f), CUI::CORNER_ALL, 3.0f);
	Textbox.VMargin(3.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars)-1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);
	
	//TODO: make it blink
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		Textbox = *pRect;
		Textbox.VSplitLeft(2.0f, 0, &Textbox);
		Textbox.x += w*UI()->Scale();
		Textbox.y -= FontSize/10.f;
		
		UI()->DoLabel(&Textbox, "|", FontSize*1.1f, -1);
	}

	return ReturnValue;
}

vec4 CEditor::ButtonColorMul(const void *pId)
{
	if(UI()->ActiveItem() == pId)
		return vec4(1,1,1,0.5f);
	else if(UI()->HotItem() == pId)
		return vec4(1,1,1,1.5f);
	return vec4(1,1,1,1);
}

float CEditor::UiDoScrollbarV(const void *pId, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float s_OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h-Handle.h)*Current;

	// logic
    float Ret = Current;
    int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pId)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->y;
		float Max = pRect->h-Handle.h;
		float Cur = UI()->MouseY()-s_OffsetY;
		Ret = (Cur-Min)/Max;
		if(Ret < 0.0f) Ret = 0.0f;
		if(Ret > 1.0f) Ret = 1.0f;
	}
	else if(UI()->HotItem() == pId)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pId);
			s_OffsetY = UI()->MouseY()-Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pId);

	// render
	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, vec4(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.w = Rail.x-Slider.x;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_L, 2.5f);
	Slider.x = Rail.x+Rail.w;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_R, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f)*ButtonColorMul(pId), CUI::CORNER_ALL, 2.5f);

    return Ret;
}

vec4 CEditor::GetButtonColor(const void *pId, int Checked)
{
	if(Checked < 0)
		return vec4(0,0,0,0.5f);

	if(Checked > 0)
	{
		if(UI()->HotItem() == pId)
			return vec4(1,0,0,0.75f);
		return vec4(1,0,0,0.5f);
	}

	if(UI()->HotItem() == pId)
		return vec4(1,1,1,0.75f);
	return vec4(1,1,1,0.5f);
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags&BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
		if(m_pTooltip)
			m_pTooltip = pToolTip;
	}

	if(UI()->HotItem() == pID && pToolTip)
		m_pTooltip = (const char *)pToolTip;

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);

	// Draw here
	//return UI()->DoButton(id, text, checked, r, draw_func, 0);
}


int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);
    CUIRect NewRect = *pRect;
    NewRect.y += NewRect.h/2.0f-7.0f;
    UI()->DoLabel(&NewRect, pText, 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID)
		RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	UI()->DoLabel(&t, pText, 10, -1, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	CUIRect r = *pRect;
    RenderTools()->DrawUIRect(&r, vec4(0.5f, 0.5f, 0.5f, 1.0f), CUI::CORNER_T, 3.0f);

	r = *pRect;
	r.VMargin(5.0f, &r);
	UI()->DoLabel(&r, pText, 10, -1, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID || Checked)
		RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	UI()->DoLabel(&t, pText, 10, -1, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, 0, 0);
}

int CEditor::DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_T, 5.0f);
    CUIRect NewRect = *pRect;
    NewRect.y += NewRect.h/2.0f-7.0f;
    UI()->DoLabel(&NewRect, pText, 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), Corners, 3.0f);
    CUIRect NewRect = *pRect;
    NewRect.y += NewRect.h/2.0f-7.0f;
    UI()->DoLabel(&NewRect, pText, 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_R, 3.0f);
	UI()->DoLabel(pRect, pText?pText:"+", 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_L, 3.0f);
	UI()->DoLabel(pRect, pText?pText:"-", 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

void CEditor::RenderBackground(CUIRect View, int Texture, float Size, float Brightness)
{
	Graphics()->TextureSet(Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0,0, View.w/Size, View.h/Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

int CEditor::UiDoValueSelector(void *pId, CUIRect *r, const char *pLabel, int Current, int Min, int Max, float Scale)
{
    // logic
    static float s_Value;
    int Ret = 0;
    int Inside = UI()->MouseInside(r);

	if(UI()->ActiveItem() == pId)
	{
		if(!UI()->MouseButton(0))
		{
			if(Inside)
				Ret = 1;
			m_LockMouse = false;
			UI()->SetActiveItem(0);
		}
		else
		{
			if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
				s_Value += m_MouseDeltaX*0.05f;
			else
				s_Value += m_MouseDeltaX;

			if(absolute(s_Value) > Scale)
			{
				int Count = (int)(s_Value/Scale);
				s_Value = fmod(s_Value, Scale);
				Current += Count;
				if(Current < Min)
					Current = Min;
				if(Current > Max)
					Current = Max;
			}
		}
	}
	else if(UI()->HotItem() == pId)
	{
		if(UI()->MouseButton(0))
		{
			m_LockMouse = true;
			s_Value = 0;
			UI()->SetActiveItem(pId);
		}
	}

	if(Inside)
		UI()->SetHotItem(pId);

	// render
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf),"%s %d", pLabel, Current);
	RenderTools()->DrawUIRect(r, GetButtonColor(pId, 0), CUI::CORNER_ALL, 5.0f);
    r->y += r->h/2.0f-7.0f;
    UI()->DoLabel(r, aBuf, 10, 0, -1);
    
	return Current;
}

CLayerGroup *CEditor::GetSelectedGroup()
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < m_Map.m_lGroups.size())
		return m_Map.m_lGroups[m_SelectedGroup];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayer(int Index)
{
	CLayerGroup *pGroup = GetSelectedGroup();
	if(!pGroup)
		return 0x0;

	if(m_SelectedLayer >= 0 && m_SelectedLayer < m_Map.m_lGroups[m_SelectedGroup]->m_lLayers.size())
		return pGroup->m_lLayers[m_SelectedLayer];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayerType(int Index, int Type)
{
	CLayer *p = GetSelectedLayer(Index);
	if(p && p->m_Type == Type)
		return p;
	return 0x0;
}

CQuad *CEditor::GetSelectedQuad()
{
	CLayerQuads *ql = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
	if(!ql)
		return 0;
	if(m_SelectedQuad >= 0 && m_SelectedQuad < ql->m_lQuads.size())
		return &ql->m_lQuads[m_SelectedQuad];
	return 0;
}

static void CallbackOpenMap(const char *pFileName, void *pUser)
{
	CEditor *pEditor = (CEditor*)pUser;
	if(pEditor->Load(pFileName))
	{
		str_copy(pEditor->m_aFileName, pFileName, 512);
		pEditor->SortImages();
	}
}
static void CallbackAppendMap(const char *pFileName, void *pUser)
{
	CEditor *pEditor = (CEditor*)pUser;
	if(pEditor->Append(pFileName))
		pEditor->m_aFileName[0] = 0;
	else
		pEditor->SortImages();
}
static void CallbackSaveMap(const char *pFileName, void *pUser)
{
	char aBuf[1024];
	const int Length = str_length(pFileName);
	// add map extension
	if(Length <= 4 || pFileName[Length-4] != '.' || str_comp_nocase(pFileName+Length-3, "map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	CEditor *pEditor = static_cast<CEditor*>(pUser);
	if(pEditor->Save(pFileName))
		str_copy(pEditor->m_aFileName, pFileName, sizeof(pEditor->m_aFileName));
}

void CEditor::DoToolbar(CUIRect ToolBar)
{
	CUIRect TB_Top, TB_Bottom;
	CUIRect Button;
	
	ToolBar.HSplitTop(ToolBar.h/2.0f, &TB_Top, &TB_Bottom);
	
    TB_Top.HSplitBottom(2.5f, &TB_Top, 0);
    TB_Bottom.HSplitTop(2.5f, 0, &TB_Bottom);

	// ctrl+o to open
	if(Input()->KeyDown('o') && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
		InvokeFileDialog(IStorage::TYPE_ALL, Localize("Open map"), Localize("Open"), "maps/", "", CallbackOpenMap, this);

	// ctrl+s to save
	if(Input()->KeyDown('s') && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
	{
		if(m_aFileName[0])	
			CallbackSaveMap(m_aFileName, this);
		else
			InvokeFileDialog(IStorage::TYPE_SAVE, Localize("Save map"), Localize("Save"), "maps/", "", CallbackSaveMap, this);
	}

	// detail button
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_HqButton = 0;
	if(DoButton_Editor(&s_HqButton, Localize("HD"), m_ShowDetail, &Button, 0, Localize("[ctrl+h] Toggle High Detail")) ||
		(Input()->KeyDown('h') && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL))))
	{
		m_ShowDetail = !m_ShowDetail;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// animation button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_AnimateButton = 0;
	if(DoButton_Editor(&s_AnimateButton, Localize("Anim"), m_Animate, &Button, 0, ("[ctrl+m] Toggle animation")) ||
		(Input()->KeyDown('m') && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL))))
	{
		m_AnimateStart = time_get();
		m_Animate = !m_Animate;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// proof button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_ProofButton = 0;
	if(DoButton_Editor(&s_ProofButton, Localize("Proof"), m_ProofBorders, &Button, 0, Localize("[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.")) ||
		(Input()->KeyDown('p') && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL))))
	{
		m_ProofBorders = !m_ProofBorders;
	}

	TB_Top.VSplitLeft(15.0f, 0, &TB_Top);

	// zoom group
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomOutButton = 0;
	if(DoButton_Ex(&s_ZoomOutButton, Localize("ZO"), 0, &Button, 0, Localize("[NumPad-] Zoom out"), CUI::CORNER_L) || Input()->KeyDown(KEY_KP_MINUS))
		m_ZoomLevel += 50;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomNormalButton = 0;
	if(DoButton_Ex(&s_ZoomNormalButton, "1:1", 0, &Button, 0, Localize("[NumPad*] Zoom to normal and remove editor offset"), 0) || Input()->KeyDown(KEY_KP_MULTIPLY))
	{
		m_EditorOffsetX = 0;
		m_EditorOffsetY = 0;
		m_ZoomLevel = 100;
	}

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomInButton = 0;
	if(DoButton_Ex(&s_ZoomInButton, Localize("ZI"), 0, &Button, 0, Localize("[NumPad+] Zoom in"), CUI::CORNER_R) || Input()->KeyDown(KEY_KP_PLUS))
		m_ZoomLevel -= 50;

	TB_Top.VSplitLeft(10.0f, 0, &TB_Top);

	// animation speed
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimFasterButton = 0;
	if(DoButton_Ex(&s_AnimFasterButton, "A+", 0, &Button, 0, Localize("Increase animation speed"), CUI::CORNER_L))
		m_AnimateSpeed += 0.5f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimNormalButton = 0;
	if(DoButton_Ex(&s_AnimNormalButton, "1", 0, &Button, 0, Localize("Normal animation speed"), 0))
		m_AnimateSpeed = 1.0f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimSlowerButton = 0;
	if(DoButton_Ex(&s_AnimSlowerButton, "A-", 0, &Button, 0, Localize("Decrease animation speed"), CUI::CORNER_R))
	{
		if(m_AnimateSpeed > 0.5f)
			m_AnimateSpeed -= 0.5f;
	}

	if(Input()->KeyPresses(KEY_MOUSE_WHEEL_UP) && m_Dialog == DIALOG_NONE)
		m_ZoomLevel -= 20;

	if(Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN) && m_Dialog == DIALOG_NONE)
		m_ZoomLevel += 20;

	if(m_ZoomLevel < 50)
		m_ZoomLevel = 50;
	m_WorldZoom = m_ZoomLevel/100.0f;

	TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);


	// brush manipulation
	{
		int Enabled = m_Brush.IsEmpty()?-1:0;

		// flip buttons
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipXButton = 0;
		if(DoButton_Ex(&s_FlipXButton, "X/X", Enabled, &Button, 0, Localize("[N] Flip brush horizontal"), CUI::CORNER_L) || Input()->KeyDown('n'))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipX();
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipyButton = 0;
		if(DoButton_Ex(&s_FlipyButton, "Y/Y", Enabled, &Button, 0, Localize("[M] Flip brush vertical"), CUI::CORNER_R) || Input()->KeyDown('m'))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipY();
		}

		// rotate buttons
		TB_Top.VSplitLeft(15.0f, &Button, &TB_Top);

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_RotationAmount = 90;
		s_RotationAmount = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, 1, 360, 2.0f);

		TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CcwButton = 0;
		if(DoButton_Ex(&s_CcwButton, Localize("CCW"), Enabled, &Button, 0, Localize("[R] Rotates the brush counter clockwise"), CUI::CORNER_L) || Input()->KeyDown('r'))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(-s_RotationAmount/360.0f*pi*2);
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CwButton = 0;
		if(DoButton_Ex(&s_CwButton, Localize("CW"), Enabled, &Button, 0, Localize("[T] Rotates the brush clockwise"), CUI::CORNER_R) || Input()->KeyDown('t'))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(s_RotationAmount/360.0f*pi*2);
		}
	}

	// quad manipulation
	{
		// do add button
		TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);
		TB_Top.VSplitLeft(60.0f, &Button, &TB_Top);
		static int s_NewButton = 0;

		CLayerQuads *pQLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
		//CLayerTiles *tlayer = (CLayerTiles *)get_selected_layer_type(0, LAYERTYPE_TILES);
		if(DoButton_Editor(&s_NewButton, Localize("Add Quad"), pQLayer?0:-1, &Button, 0, Localize("Adds a new quad")))
		{
			if(pQLayer)
			{
				float Mapping[4];
				CLayerGroup *g = GetSelectedGroup();
				g->Mapping(Mapping);
				int AddX = f2fx(Mapping[0] + (Mapping[2]-Mapping[0])/2);
				int AddY = f2fx(Mapping[1] + (Mapping[3]-Mapping[1])/2);

				CQuad *q = pQLayer->NewQuad();
				for(int i = 0; i < 5; i++)
				{
					q->m_aPoints[i].x += AddX;
					q->m_aPoints[i].y += AddY;
				}
			}
		}
	}
    
	// tile manipulation
	{
		TB_Bottom.VSplitLeft(40.0f, &Button, &TB_Bottom);
		static int s_BorderBut = 0;
		CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
		
		// no border for tele layer
		if(pT && (pT->m_Tele || pT->m_Speedup))
			pT = 0;

		if(DoButton_Editor(&s_BorderBut, Localize("Border"), pT?0:-1, &Button, 0, Localize("Border")))
		{
			if(pT)
                DoMapBorder();
		}
		
		// do tele button
		TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
		TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
		static int s_TeleButton = 0;
		CLayerTiles *pS = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
		
		if(DoButton_Ex(&s_TeleButton, "Teleporter", (pS && pS->m_Tele)?0:-1, &Button, 0, "Teleporter", CUI::CORNER_ALL))
		{
			static int s_TelePopupId = 0;
			UiInvokePopupMenu(&s_TelePopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 23, PopupTele);
		}
		
		TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
		TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
		static int s_SpeedupButton = 0;
		if(DoButton_Ex(&s_SpeedupButton, "Speedup", (pS && pS->m_Speedup)?0:-1, &Button, 0, "Speedup", CUI::CORNER_ALL))
		{
			static int s_SpeedupPopupId = 0;
			UiInvokePopupMenu(&s_SpeedupPopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 43, PopupSpeedup);
		}
	}

	TB_Bottom.VSplitLeft(5.0f, 0, &TB_Bottom);

	// refocus button
	TB_Bottom.VSplitLeft(50.0f, &Button, &TB_Bottom);
	static int s_RefocusButton = 0;
	if(DoButton_Editor(&s_RefocusButton, Localize("Refocus"), m_WorldOffsetX&&m_WorldOffsetY?0:-1, &Button, 0, Localize("[HOME] Restore map focus")) || Input()->KeyDown(KEY_HOME))
	{
		m_WorldOffsetX = 0;
		m_WorldOffsetY = 0;
	}
}

static void Rotate(CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (int)(x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

void CEditor::DoQuad(CQuad *q, int Index)
{
	enum
	{
		OP_NONE=0,
		OP_MOVE_ALL,
		OP_MOVE_PIVOT,
		OP_ROTATE,
		OP_CONTEXT_MENU,
	};

	// some basic values
	void *pId = &q->m_aPoints[4]; // use pivot addr as id
	static CPoint s_RotatePoints[4];
	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;
	static float s_RotateAngle = 0;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	// get pivot
	float CenterX = fx2f(q->m_aPoints[4].x);
	float CenterY = fx2f(q->m_aPoints[4].y);

	float dx = (CenterX - wx)/m_WorldZoom;
	float dy = (CenterY - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50)
		UI()->SetHotItem(pId);

	// draw selection background
	if(m_SelectedQuad == Index)
	{
		Graphics()->SetColor(0,0,0,1);
		IGraphics::CQuadItem QuadItem(CenterX, CenterY, 7.0f, 7.0f);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	if(UI()->ActiveItem() == pId)
	{
		// check if we only should move pivot
		if(s_Operation == OP_MOVE_PIVOT)
		{
			q->m_aPoints[4].x += f2fx(wx-s_LastWx);
			q->m_aPoints[4].y += f2fx(wy-s_LastWy);
		}
		else if(s_Operation == OP_MOVE_ALL)
		{
			// move all points including pivot
			for(int v = 0; v < 5; v++)
			{
				q->m_aPoints[v].x += f2fx(wx-s_LastWx);
				q->m_aPoints[v].y += f2fx(wy-s_LastWy);
			}
		}
		else if(s_Operation == OP_ROTATE)
		{
			for(int v = 0; v < 4; v++)
			{
				q->m_aPoints[v] = s_RotatePoints[v];
				Rotate(&q->m_aPoints[4], &q->m_aPoints[v], s_RotateAngle);
			}
		}

		s_RotateAngle += (m_MouseDeltaX) * 0.002f;
		s_LastWx = wx;
		s_LastWy = wy;

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				static int s_QuadPopupId = 0;
				UiInvokePopupMenu(&s_QuadPopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 150, PopupQuad);
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}

		Graphics()->SetColor(1,1,1,1);
	}
	else if(UI()->HotItem() == pId)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1,1,1,1);
		m_pTooltip = Localize("Left mouse button to move. Hold shift to move pivot. Hold ctrl to rotate.");

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
				s_Operation = OP_MOVE_PIVOT;
			else if(Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL))
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
				s_RotateAngle = 0;
				s_RotatePoints[0] = q->m_aPoints[0];
				s_RotatePoints[1] = q->m_aPoints[1];
				s_RotatePoints[2] = q->m_aPoints[2];
				s_RotatePoints[3] = q->m_aPoints[3];
			}
			else
				s_Operation = OP_MOVE_ALL;

			UI()->SetActiveItem(pId);
			m_SelectedQuad = Index;
			s_LastWx = wx;
			s_LastWy = wy;
		}

		if(UI()->MouseButton(1))
		{
			m_SelectedQuad = Index;
			s_Operation = OP_CONTEXT_MENU;
			UI()->SetActiveItem(pId);
		}
	}
	else
		Graphics()->SetColor(0,1,0,1);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadPoint(CQuad *q, int QuadIndex, int v)
{
	void *pId = &q->m_aPoints[v];

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float px = fx2f(q->m_aPoints[v].x);
	float py = fx2f(q->m_aPoints[v].y);

	float dx = (px - wx)/m_WorldZoom;
	float dy = (py - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50)
		UI()->SetHotItem(pId);

	// draw selection background
	if(m_SelectedQuad == QuadIndex && m_SelectedPoints&(1<<v))
	{
		Graphics()->SetColor(0,0,0,1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f, 7.0f);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	enum
	{
		OP_NONE=0,
		OP_MOVEPOINT,
		OP_MOVEUV,
		OP_CONTEXT_MENU
	};

	static bool s_Moved;
	static int s_Operation = OP_NONE;

	if(UI()->ActiveItem() == pId)
	{
		float dx = m_MouseDeltaWx;
		float dy = m_MouseDeltaWy;
		if(!s_Moved)
		{
			if(dx*dx+dy*dy > 0.5f)
				s_Moved = true;
		}

		if(s_Moved)
		{
			if(s_Operation == OP_MOVEPOINT)
			{
				for(int m = 0; m < 4; m++)
					if(m_SelectedPoints&(1<<m))
					{
						q->m_aPoints[m].x += f2fx(dx);
						q->m_aPoints[m].y += f2fx(dy);
					}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				for(int m = 0; m < 4; m++)
					if(m_SelectedPoints&(1<<m))
					{
						q->m_aTexcoords[m].x += f2fx(dx*0.001f);
						q->m_aTexcoords[m].y += f2fx(dy*0.001f);
					}
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				static int s_PointPopupId = 0;
				UiInvokePopupMenu(&s_PointPopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 150, PopupPoint);
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(!s_Moved)
				{
					if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
						m_SelectedPoints ^= 1<<v;
					else
						m_SelectedPoints = 1<<v;
				}
				m_LockMouse = false;
				UI()->SetActiveItem(0);
			}
		}

		Graphics()->SetColor(1,1,1,1);
	}
	else if(UI()->HotItem() == pId)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1,1,1,1);
		m_pTooltip = Localize("Left mouse button to move. Hold shift to move the texture.");

		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pId);
			s_Moved = false;
			if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
			{
				s_Operation = OP_MOVEUV;
				m_LockMouse = true;
			}
			else
				s_Operation = OP_MOVEPOINT;

			if(!(m_SelectedPoints&(1<<v)))
			{
				if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1<<v;
				else
					m_SelectedPoints = 1<<v;
				s_Moved = true;
			}

			m_SelectedQuad = QuadIndex;
		}
		else if(UI()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;
			m_SelectedQuad = QuadIndex;
			UI()->SetActiveItem(pId);
			if(!(m_SelectedPoints&(1<<v)))
			{
				if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1<<v;
				else
					m_SelectedPoints = 1<<v;
				s_Moved = true;
			}
		}
	}
	else
		Graphics()->SetColor(1,0,0,1);

	IGraphics::CQuadItem QuadItem(px, py, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoMapEditor(CUIRect View, CUIRect ToolBar)
{
	//UI()->ClipEnable(&view);

	bool ShowPicker = Input()->KeyPressed(KEY_SPACE) != 0 && m_Dialog == DIALOG_NONE;

	// render all good stuff
	if(!ShowPicker)
	{
		for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		{
			if(m_Map.m_lGroups[g]->m_Visible)
				m_Map.m_lGroups[g]->Render();
			//UI()->ClipEnable(&view);
		}

		// render the game and tele above everything else
		if(m_Map.m_pGameGroup->m_Visible)
 		{
 			m_Map.m_pGameGroup->MapScreen();
			if(m_Map.m_pGameLayer->m_Visible)
				m_Map.m_pGameLayer->Render();
			if(m_Map.m_pTeleLayer && m_Map.m_pTeleLayer->m_Visible)
				m_Map.m_pTeleLayer->Render();
			if(m_Map.m_pSpeedupLayer && m_Map.m_pSpeedupLayer->m_Visible)
				m_Map.m_pSpeedupLayer->Render();
 		}
	}

	static void *s_pEditorId = (void *)&s_pEditorId;
	int Inside = UI()->MouseInside(&View);

	// fetch mouse position
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	float mx = UI()->MouseX();
	float my = UI()->MouseY();

	static float s_StartWx = 0;
	static float s_StartWy = 0;
	static float s_StartMx = 0;
	static float s_StartMy = 0;

	enum
	{
		OP_NONE=0,
		OP_BRUSH_GRAB,
		OP_BRUSH_DRAW,
		OP_BRUSH_PAINT,
		OP_PAN_WORLD,
		OP_PAN_EDITOR,
	};

	// remap the screen so it can display the whole tileset
	if(ShowPicker)
	{
		CUIRect Screen = *UI()->Screen();
		float Size = 32.0*16.0f;
		float w = Size*(Screen.w/View.w);
		float h = Size*(Screen.h/View.h);
		float x = -(View.x/Screen.w)*w;
		float y = -(View.y/Screen.h)*h;
		wx = x+w*mx/Screen.w;
		wy = y+h*my/Screen.h;
		Graphics()->MapScreen(x, y, x+w, y+h);
		CLayerTiles *t = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
		if(t)
		{
			m_TilesetPicker.m_Image = t->m_Image;
			m_TilesetPicker.m_TexId = t->m_TexId;
			m_TilesetPicker.Render();
		}
	}

	static int s_Operation = OP_NONE;

	// draw layer borders
	CLayer *pEditLayers[16];
	int NumEditLayers = 0;
	NumEditLayers = 0;

	if(ShowPicker)
	{
		pEditLayers[0] = &m_TilesetPicker;
		NumEditLayers++;
	}
	else
	{
		pEditLayers[0] = GetSelectedLayer(0);
		if(pEditLayers[0])
			NumEditLayers++;

		CLayerGroup *g = GetSelectedGroup();
		if(g)
		{
			g->MapScreen();

			for(int i = 0; i < NumEditLayers; i++)
			{
				if(pEditLayers[i]->m_Type != LAYERTYPE_TILES)
					continue;

				float w, h;
				pEditLayers[i]->GetSize(&w, &h);

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(0, 0, w, 0),
					IGraphics::CLineItem(w, 0, w, h),
					IGraphics::CLineItem(w, h, 0, h),
					IGraphics::CLineItem(0, h, 0, 0)};
				Graphics()->TextureSet(-1);
				Graphics()->LinesBegin();
				Graphics()->LinesDraw(Array, 4);
				Graphics()->LinesEnd();
			}
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(s_pEditorId);

		// do global operations like pan and zoom
		if(UI()->ActiveItem() == 0 && (UI()->MouseButton(0) || UI()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;
			s_StartMx = mx;
			s_StartMy = my;

			if(Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL) || UI()->MouseButton(2))
			{
				if(Input()->KeyPressed(KEY_LSHIFT))
					s_Operation = OP_PAN_EDITOR;
				else
					s_Operation = OP_PAN_WORLD;
				UI()->SetActiveItem(s_pEditorId);
			}
		}

		// brush editing
		if(UI()->HotItem() == s_pEditorId)
		{
			if(m_Brush.IsEmpty())
				m_pTooltip = Localize("Use left mouse button to drag and create a brush.");
			else
				m_pTooltip = Localize("Use left mouse button to paint with the brush. Right button clears the brush.");

			if(UI()->ActiveItem() == s_pEditorId)
			{
				CUIRect r;
				r.x = s_StartWx;
				r.y = s_StartWy;
				r.w = wx-s_StartWx;
				r.h = wy-s_StartWy;
				if(r.w < 0)
				{
					r.x += r.w;
					r.w = -r.w;
				}

				if(r.h < 0)
				{
					r.y += r.h;
					r.h = -r.h;
				}

				if(s_Operation == OP_BRUSH_DRAW)
				{
					if(!m_Brush.IsEmpty())
					{
						// draw with brush
						for(int k = 0; k < NumEditLayers; k++)
						{
							if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
								pEditLayers[k]->BrushDraw(m_Brush.m_lLayers[0], wx, wy);
						}
					}
				}
				else if(s_Operation == OP_BRUSH_GRAB)
				{
					if(!UI()->MouseButton(0))
					{
						// grab brush
						dbg_msg("editor", "grabbing %f %f %f %f", r.x, r.y, r.w, r.h);

						// TODO: do all layers
						int Grabs = 0;
						for(int k = 0; k < NumEditLayers; k++)
							Grabs += pEditLayers[k]->BrushGrab(&m_Brush, r);
						if(Grabs == 0)
							m_Brush.Clear();
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(int k = 0; k < NumEditLayers; k++)
							pEditLayers[k]->BrushSelecting(r);
						Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
					}
				}
				else if(s_Operation == OP_BRUSH_PAINT)
				{
					if(!UI()->MouseButton(0))
					{
                        for(int k = 0; k < NumEditLayers; k++)
                            pEditLayers[k]->FillSelection(m_Brush.IsEmpty(), m_Brush.m_lLayers[0], r);
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(int k = 0; k < NumEditLayers; k++)
							pEditLayers[k]->BrushSelecting(r);
						Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
					}
				}
			}
			else
			{
				if(UI()->MouseButton(1))
					m_Brush.Clear();

				if(UI()->MouseButton(0) && s_Operation == OP_NONE)
				{
					UI()->SetActiveItem(s_pEditorId);

					if(m_Brush.IsEmpty())
						s_Operation = OP_BRUSH_GRAB;
					else
					{
						s_Operation = OP_BRUSH_DRAW;
						for(int k = 0; k < NumEditLayers; k++)
						{
							if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
								pEditLayers[k]->BrushPlace(m_Brush.m_lLayers[0], wx, wy);
						}

					}
					
					CLayerTiles *pLayer = (CLayerTiles*)GetSelectedLayerType(0, LAYERTYPE_TILES);
					if((Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT)) && pLayer)
                        s_Operation = OP_BRUSH_PAINT;
				}

				if(!m_Brush.IsEmpty())
				{
					m_Brush.m_OffsetX = -(int)wx;
					m_Brush.m_OffsetY = -(int)wy;
					for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
					{
						if(m_Brush.m_lLayers[i]->m_Type == LAYERTYPE_TILES)
						{
							m_Brush.m_OffsetX = -(int)(wx/32.0f)*32;
							m_Brush.m_OffsetY = -(int)(wy/32.0f)*32;
							break;
						}
					}

					CLayerGroup *g = GetSelectedGroup();
					if(g)
					{
						m_Brush.m_OffsetX += g->m_OffsetX;
						m_Brush.m_OffsetY += g->m_OffsetY;
						m_Brush.m_ParallaxX = g->m_ParallaxX;
						m_Brush.m_ParallaxY = g->m_ParallaxY;
						m_Brush.Render();
						float w, h;
						m_Brush.GetSize(&w, &h);

						IGraphics::CLineItem Array[4] = {
							IGraphics::CLineItem(0, 0, w, 0),
							IGraphics::CLineItem(w, 0, w, h),
							IGraphics::CLineItem(w, h, 0, h),
							IGraphics::CLineItem(0, h, 0, 0)};
						Graphics()->TextureSet(-1);
						Graphics()->LinesBegin();
						Graphics()->LinesDraw(Array, 4);
						Graphics()->LinesEnd();
					}
				}
			}
		}

		// quad editing
		{
			if(!ShowPicker && m_Brush.IsEmpty())
			{
				// fetch layers
				CLayerGroup *g = GetSelectedGroup();
				if(g)
					g->MapScreen();

				for(int k = 0; k < NumEditLayers; k++)
				{
					if(pEditLayers[k]->m_Type == LAYERTYPE_QUADS)
					{
						CLayerQuads *pLayer = (CLayerQuads *)pEditLayers[k];

						Graphics()->TextureSet(-1);
						Graphics()->QuadsBegin();
						for(int i = 0; i < pLayer->m_lQuads.size(); i++)
						{
							for(int v = 0; v < 4; v++)
								DoQuadPoint(&pLayer->m_lQuads[i], i, v);

							DoQuad(&pLayer->m_lQuads[i], i);
						}
						Graphics()->QuadsEnd();
					}
				}

				Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
			}

			// do panning
			if(UI()->ActiveItem() == s_pEditorId)
			{
				if(s_Operation == OP_PAN_WORLD)
				{
					m_WorldOffsetX -= m_MouseDeltaX*m_WorldZoom;
					m_WorldOffsetY -= m_MouseDeltaY*m_WorldZoom;
				}
				else if(s_Operation == OP_PAN_EDITOR)
				{
					m_EditorOffsetX -= m_MouseDeltaX*m_WorldZoom;
					m_EditorOffsetY -= m_MouseDeltaY*m_WorldZoom;
				}

				// release mouse
				if(!UI()->MouseButton(0))
				{
					s_Operation = OP_NONE;
					UI()->SetActiveItem(0);
				}
			}
		}
	}
	else if(UI()->ActiveItem() == s_pEditorId)
	{
		// release mouse
		if(!UI()->MouseButton(0))
		{
			s_Operation = OP_NONE;
			UI()->SetActiveItem(0);
		}
	}

	if(GetSelectedGroup() && GetSelectedGroup()->m_UseClipping)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();
	
		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();

			CUIRect r;
			r.x = GetSelectedGroup()->m_ClipX;
			r.y = GetSelectedGroup()->m_ClipY;
			r.w = GetSelectedGroup()->m_ClipW;
			r.h = GetSelectedGroup()->m_ClipH;

			IGraphics::CLineItem Array[4] = {
				IGraphics::CLineItem(r.x, r.y, r.x+r.w, r.y),
				IGraphics::CLineItem(r.x+r.w, r.y, r.x+r.w, r.y+r.h),
				IGraphics::CLineItem(r.x+r.w, r.y+r.h, r.x, r.y+r.h),
				IGraphics::CLineItem(r.x, r.y+r.h, r.x, r.y)};
			Graphics()->SetColor(1,0,0,1);
			Graphics()->LinesDraw(Array, 4);

		Graphics()->LinesEnd();
	}

	// render screen sizes
	if(m_ProofBorders)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();

		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();

		float aLastPoints[4];
		float Start = 1.0f; //9.0f/16.0f;
		float End = 16.0f/9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End-Start)*(i/(float)NumSteps);

			RenderTools()->MapscreenToWorld(
				m_WorldOffsetX, m_WorldOffsetY,
				1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

			if(i == 0)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[2], aPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			if(i != 0)
			{
				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aLastPoints[0], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aLastPoints[2], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aLastPoints[0], aLastPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[3], aLastPoints[2], aLastPoints[3])};
				Graphics()->LinesDraw(Array, 4);
			}

			if(i == NumSteps)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[0], aPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			mem_copy(aLastPoints, aPoints, sizeof(aPoints));
		}

		if(1)
		{
			Graphics()->SetColor(1,0,0,1);
			for(int i = 0; i < 2; i++)
			{
				float aPoints[4];
				float aAspects[] = {4.0f/3.0f, 16.0f/10.0f, 5.0f/4.0f, 16.0f/9.0f};
				float Aspect = aAspects[i];

				RenderTools()->MapscreenToWorld(
					m_WorldOffsetX, m_WorldOffsetY,
					1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

				CUIRect r;
				r.x = aPoints[0];
				r.y = aPoints[1];
				r.w = aPoints[2]-aPoints[0];
				r.h = aPoints[3]-aPoints[1];

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(r.x, r.y, r.x+r.w, r.y),
					IGraphics::CLineItem(r.x+r.w, r.y, r.x+r.w, r.y+r.h),
					IGraphics::CLineItem(r.x+r.w, r.y+r.h, r.x, r.y+r.h),
					IGraphics::CLineItem(r.x, r.y+r.h, r.x, r.y)};
				Graphics()->LinesDraw(Array, 4);
				Graphics()->SetColor(0,1,0,1);
			}
		}

		Graphics()->LinesEnd();
	}

	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
	//UI()->ClipDisable();
}


int CEditor::DoProperties(CUIRect *pToolBox, CProperty *pProps, int *pIds, int *pNewVal)
{
	int Change = -1;

	for(int i = 0; pProps[i].m_pName; i++)
	{
		CUIRect Slot;
		pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
		CUIRect Label, Shifter;
		Slot.VSplitMid(&Label, &Shifter);
		Shifter.HMargin(1.0f, &Shifter);
		UI()->DoLabel(&Label, pProps[i].m_pName, 10.0f, -1, -1);

		if(pProps[i].m_Type == PROPTYPE_INT_STEP)
		{
			CUIRect Inc, Dec;
			char aBuf[64];

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			str_format(aBuf, sizeof(aBuf),"%d", pProps[i].m_Value);
			RenderTools()->DrawUIRect(&Shifter, vec4(1,1,1,0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, aBuf, 10.0f, 0, -1);

			if(DoButton_ButtonDec(&pIds[i], 0, 0, &Dec, 0, Localize("Decrease")))
			{
				if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
					*pNewVal = pProps[i].m_Value-5;
				else
					*pNewVal = pProps[i].m_Value-1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIds[i])+1, 0, 0, &Inc, 0, Localize("Increase")))
			{
				if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
					*pNewVal = pProps[i].m_Value+5;
				else
					*pNewVal = pProps[i].m_Value+1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_BOOL)
		{
			CUIRect No, Yes;
			Shifter.VSplitMid(&No, &Yes);
			if(DoButton_ButtonDec(&pIds[i], Localize("No"), !pProps[i].m_Value, &No, 0, ""))
			{
				*pNewVal = 0;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIds[i])+1, Localize("Yes"), pProps[i].m_Value, &Yes, 0, ""))
			{
				*pNewVal = 1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_INT_SCROLL)
		{
			int NewValue = UiDoValueSelector(&pIds[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1.0f);
			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_COLOR)
		{
			static const char *s_paTexts[4] = {"R", "G", "B", "A"};
			static int s_aShift[] = {24, 16, 8, 0};
			int NewColor = 0;

			for(int c = 0; c < 4; c++)
			{
				int v = (pProps[i].m_Value >> s_aShift[c])&0xff;
				NewColor |= UiDoValueSelector(((char *)&pIds[i])+c, &Shifter, s_paTexts[c], v, 0, 255, 1.0f)<<s_aShift[c];

				if(c != 3)
				{
					pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
					Slot.VSplitMid(0, &Shifter);
					Shifter.HMargin(1.0f, &Shifter);
				}
			}

			if(NewColor != pProps[i].m_Value)
			{
				*pNewVal = NewColor;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_IMAGE)
		{
			char aBuf[64];
			if(pProps[i].m_Value < 0)
				str_copy(aBuf, Localize("None"), sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf),"%s",  m_Map.m_lImages[pProps[i].m_Value]->m_aName);

			if(DoButton_Editor(&pIds[i], aBuf, 0, &Shifter, 0, 0))
				PopupSelectImageInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectImageResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
	}

	return Change;
}

void CEditor::RenderLayers(CUIRect ToolBox, CUIRect ToolBar, CUIRect View)
{
	CUIRect LayersBox = ToolBox;

	if(!m_GuiActive)
		return;

	CUIRect Slot, Button;
	char aBuf[64];

	int ValidGroup = 0;
	int ValidLayer = 0;
	if(m_SelectedGroup >= 0 && m_SelectedGroup < m_Map.m_lGroups.size())
		ValidGroup = 1;

	if(ValidGroup && m_SelectedLayer >= 0 && m_SelectedLayer < m_Map.m_lGroups[m_SelectedGroup]->m_lLayers.size())
		ValidLayer = 1;

	float LayersHeight = 12.0f;	 // Height of AddGroup button
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;

	for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		// Each group is 19.0f
		// Each layer is 14.0f
		LayersHeight += 19.0f + m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f;

	float ScrollDifference = LayersHeight - LayersBox.h;

	if(LayersHeight > LayersBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		LayersBox.VSplitRight(15.0f, &LayersBox, &Scroll);
		LayersBox.VSplitRight(3.0f, &LayersBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);
	}

	float LayerStartAt = ScrollDifference * s_ScrollValue;
	if(LayerStartAt < 0.0f)
		LayerStartAt = 0.0f;

	float LayerStopAt = LayersHeight - ScrollDifference * (1 - s_ScrollValue);
	float LayerCur = 0;

	// render layers
	{
		for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		{
			if(LayerCur > LayerStopAt)
				break;
			else if(LayerCur + m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f + 19.0f < LayerStartAt)
			{
				LayerCur += m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f + 19.0f;
				continue;
			}

			CUIRect VisibleToggle;
			if(LayerCur >= LayerStartAt)
			{
				LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);
				Slot.VSplitLeft(12, &VisibleToggle, &Slot);
				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_Visible, m_Map.m_lGroups[g]->m_Visible?"V":"H", 0, &VisibleToggle, 0, "Toggle group visibility", CUI::CORNER_L))
					m_Map.m_lGroups[g]->m_Visible = !m_Map.m_lGroups[g]->m_Visible;

				str_format(aBuf, sizeof(aBuf),"#%d %s", g, m_Map.m_lGroups[g]->m_pName);
				if(int Result = DoButton_Ex(&m_Map.m_lGroups[g], aBuf, g==m_SelectedGroup, &Slot,
					BUTTON_CONTEXT, "Select group. Right click for properties.", CUI::CORNER_R))
				{
					m_SelectedGroup = g;
					m_SelectedLayer = 0;

					static int s_GroupPopupId = 0;
					if(Result == 2)
						UiInvokePopupMenu(&s_GroupPopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 200, PopupGroup);
				}
				LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
			}
			LayerCur += 14.0f;

			for(int i = 0; i < m_Map.m_lGroups[g]->m_lLayers.size(); i++)
			{
				if(LayerCur > LayerStopAt)
					break;
				else if(LayerCur < LayerStartAt)
				{
					LayerCur += 14.0f;
					continue;
				}

				//visible
				LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);
				Slot.VSplitLeft(12.0f, 0, &Button);
				Button.VSplitLeft(15, &VisibleToggle, &Button);

				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible, m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible?"V":"H", 0, &VisibleToggle, 0, Localize("Toggle layer visibility"), CUI::CORNER_L))
					m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible = !m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible;

				str_format(aBuf, sizeof(aBuf),"#%d %s ", i, m_Map.m_lGroups[g]->m_lLayers[i]->m_pTypeName);
				if(int Result = DoButton_Ex(m_Map.m_lGroups[g]->m_lLayers[i], aBuf, g==m_SelectedGroup&&i==m_SelectedLayer, &Button,
					BUTTON_CONTEXT, Localize("Select layer. Right click for properties."), CUI::CORNER_R))
				{
					if(m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pTeleLayer || m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pSpeedupLayer)
						m_Brush.Clear();
					m_SelectedLayer = i;
					m_SelectedGroup = g;
					static int s_LayerPopupId = 0;
					if(Result == 2)
						UiInvokePopupMenu(&s_LayerPopupId, 0, UI()->MouseX(), UI()->MouseY(), 120, 150, PopupLayer);
				}

				LayerCur += 14.0f;
				LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
			}
			if(LayerCur > LayerStartAt && LayerCur < LayerStopAt)
				LayersBox.HSplitTop(5.0f, &Slot, &LayersBox);
			LayerCur += 5.0f;
		}
	}

	if(LayerCur <= LayerStopAt)
	{
		LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);

		static int s_NewGroupButton = 0;
		if(DoButton_Editor(&s_NewGroupButton, Localize("Add group"), 0, &Slot, 0, Localize("Adds a new group")))
		{
			m_Map.NewGroup();
			m_SelectedGroup = m_Map.m_lGroups.size()-1;
		}
	}
}

static void ExtractName(const char *pFileName, char *pName)
{
	int Len = str_length(pFileName);
	int Start = Len;
	int End = Len;

	while(Start > 0)
	{
		Start--;
		if(pFileName[Start] == '/' || pFileName[Start] == '\\')
		{
			Start++;
			break;
		}
	}

	End = Start;
	for(int i = Start; i < Len; i++)
	{
		if(pFileName[i] == '.')
			End = i;
	}

	if(End == Start)
		End = Len;

	int FinalLen = End-Start;
	mem_copy(pName, &pFileName[Start], FinalLen);
	pName[FinalLen] = 0;
	dbg_msg("", "%s %s %d %d", pFileName, pName, Start, End);
}

void CEditor::ReplaceImage(const char *pFileName, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName))
		return;

	CEditorImage *pImg = pEditor->m_Map.m_lImages[pEditor->m_SelectedImage];
	pEditor->Graphics()->UnloadTexture(pImg->m_TexId);
	*pImg = ImgInfo;
	ExtractName(pFileName, pImg->m_aName);
	pImg->m_TexId = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, 0);
	pEditor->SortImages();
}

void CEditor::AddImage(const char *pFileName, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName))
		return;

	CEditorImage *pImg = new CEditorImage(pEditor);
	*pImg = ImgInfo;
	pImg->m_TexId = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, 0);
	pImg->m_External = 1; // external by default
	ExtractName(pFileName, pImg->m_aName);

	for(int i = 0; i < pEditor->m_Map.m_lImages.size(); ++i)
	{
	    if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, pImg->m_aName))
            return;
	}

	pEditor->m_Map.m_lImages.add(pImg);
	pEditor->SortImages();
}


static int gs_ModifyIndexDeletedIndex;
static void ModifyIndexDeleted(int *pIndex)
{
	if(*pIndex == gs_ModifyIndexDeletedIndex)
		*pIndex = -1;
	else if(*pIndex > gs_ModifyIndexDeletedIndex)
		*pIndex = *pIndex - 1;
}

int CEditor::PopupImage(CEditor *pEditor, CUIRect View)
{
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	CEditorImage *pImg = pEditor->m_Map.m_lImages[pEditor->m_SelectedImage];

	static int s_ExternalButton = 0;
	if(pImg->m_External)
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, Localize("Embed"), 0, &Slot, 0, Localize("Embeds the image into the map file.")))
		{
			pImg->m_External = 0;
			return 1;
		}
	}
	else
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, Localize("Make external"), 0, &Slot, 0, Localize("Removes the image from the map file.")))
		{
			pImg->m_External = 1;
			return 1;
		}
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, Localize("Replace"), 0, &Slot, 0, Localize("Replaces the image with a new one")))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, Localize("Replace Image"), Localize("Replace"), "mapres/", "", ReplaceImage, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, Localize("Remove"), 0, &Slot, 0, Localize("Removes the image from the map")))
	{
		delete pImg;
		pEditor->m_Map.m_lImages.remove_index(pEditor->m_SelectedImage);
		gs_ModifyIndexDeletedIndex = pEditor->m_SelectedImage;
		pEditor->m_Map.ModifyImageIndex(ModifyIndexDeleted);
		return 1;
	}

	return 0;
}

static int CompareImageName(const void *pObject1, const void *pObject2)
{
	CEditorImage *pImage1 = *(CEditorImage**)pObject1;
	CEditorImage *pImage2 = *(CEditorImage**)pObject2;

	return str_comp(pImage1->m_aName, pImage2->m_aName);
}

static int *gs_pSortedIndex = 0;
static void ModifySortedIndex(int *pIndex)
{
	if(*pIndex > -1)
		*pIndex = gs_pSortedIndex[*pIndex];
}

void CEditor::SortImages()
{
	bool Sorted = true;
	for(int i = 1; i < m_Map.m_lImages.size(); i++)
		if( str_comp(m_Map.m_lImages[i]->m_aName, m_Map.m_lImages[i-1]->m_aName) < 0 )
		{
			Sorted = false;
			break;
		}

	if(!Sorted)
	{
		array<CEditorImage*> lTemp = array<CEditorImage*>(m_Map.m_lImages);
		gs_pSortedIndex = new int[lTemp.size()];

		qsort(m_Map.m_lImages.base_ptr(), m_Map.m_lImages.size(), sizeof(CEditorImage*), CompareImageName);

		for(int OldIndex = 0; OldIndex < lTemp.size(); OldIndex++)
			for(int NewIndex = 0; NewIndex < m_Map.m_lImages.size(); NewIndex++)
				if(lTemp[OldIndex] == m_Map.m_lImages[NewIndex])
					gs_pSortedIndex[OldIndex] = NewIndex;

		m_Map.ModifyImageIndex(ModifySortedIndex);

		delete [] gs_pSortedIndex;
		gs_pSortedIndex = 0;
	}
}
	

void CEditor::RenderImages(CUIRect ToolBox, CUIRect ToolBar, CUIRect View)
{
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;
	float ImagesHeight = 30.0f + 14.0f * m_Map.m_lImages.size() + 27.0f;
	float ScrollDifference = ImagesHeight - ToolBox.h;

	if(ImagesHeight > ToolBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		ToolBox.VSplitRight(15.0f, &ToolBox, &Scroll);
		ToolBox.VSplitRight(3.0f, &ToolBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);
	}

	float ImageStartAt = ScrollDifference * s_ScrollValue;
	if(ImageStartAt < 0.0f)
		ImageStartAt = 0.0f;

	float ImageStopAt = ImagesHeight - ScrollDifference * (1 - s_ScrollValue);
	float ImageCur = 0.0f;

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;

		if(ImageCur > ImageStopAt)
			break;
		else if(ImageCur >= ImageStartAt)
		{

			ToolBox.HSplitTop(15.0f, &Slot, &ToolBox);
			if(e == 0)
				UI()->DoLabel(&Slot, Localize("Embedded"), 12.0f, 0);
			else
				UI()->DoLabel(&Slot, Localize("External"), 12.0f, 0);
		}
		ImageCur += 15.0f;

		for(int i = 0; i < m_Map.m_lImages.size(); i++)
		{
			if((e && !m_Map.m_lImages[i]->m_External) ||
				(!e && m_Map.m_lImages[i]->m_External))
			{
				continue;
			}

			if(ImageCur > ImageStopAt)
				break;
			else if(ImageCur < ImageStartAt)
			{
				ImageCur += 14.0f;
				continue;
			}
			ImageCur += 14.0f;

			char aBuf[128];
			str_copy(aBuf, m_Map.m_lImages[i]->m_aName, sizeof(aBuf));
			ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);

			if(int Result = DoButton_Editor(&m_Map.m_lImages[i], aBuf, m_SelectedImage == i, &Slot,
				BUTTON_CONTEXT, Localize("Select image")))
			{
				m_SelectedImage = i;

				static int s_PopupImageId = 0;
				if(Result == 2)
					UiInvokePopupMenu(&s_PopupImageId, 0, UI()->MouseX(), UI()->MouseY(), 120, 80, PopupImage);
			}

			ToolBox.HSplitTop(2.0f, 0, &ToolBox);

			// render image
			if(m_SelectedImage == i)
			{
				CUIRect r;
				View.Margin(10.0f, &r);
				if(r.h < r.w)
					r.w = r.h;
				else
					r.h = r.w;
				Graphics()->TextureSet(m_Map.m_lImages[i]->m_TexId);
				Graphics()->BlendNormal();
				Graphics()->QuadsBegin();
				IGraphics::CQuadItem QuadItem(r.x, r.y, r.w, r.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();

			}
		}
	}

	if(ImageCur + 27.0f > ImageStopAt)
		return;

	CUIRect Slot;
	ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);

	// new image
	static int s_NewImageButton = 0;
	ToolBox.HSplitTop(10.0f, &Slot, &ToolBox);
	ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);
	if(DoButton_Editor(&s_NewImageButton, "Add", 0, &Slot, 0, Localize("Load a new image to use in the map")))
		InvokeFileDialog(IStorage::TYPE_ALL, Localize("Add Image"), Localize("Add"), "mapres/", "", AddImage, this);
}


static int gs_FileDialogDirTypes = 0;
static const char *gs_pFileDialogTitle = 0;
static const char *gs_pFileDialogButtonText = 0;
static void (*gs_pfnFileDialogFunc)(const char *pFileName, void *pUser);
static void *gs_pFileDialogUser = 0;
static char gs_FileDialogFileName[512] = {0};
static char gs_aFileDialogPath[512] = {0};
static char gs_aFileDialogCompleteFilename[512] = {0};
static int gs_FilesNum = 0;
static sorted_array<string> gs_FileList;
int g_FilesStartAt = 0;
int g_FilesCur = 0;
int g_FilesStopAt = 999;

static void EditorListdirCallback(const char *pName, int IsDir, void *pUser)
{
	if(pName[0] == '.' || IsDir) // skip this shit!
		return;

	gs_FileList.add(string(pName));
}

void CEditor::AddFileDialogEntry(const char *pName, CUIRect *pView)
{
	if(g_FilesCur > gs_FilesNum)
		gs_FilesNum = g_FilesCur;

	g_FilesCur++;
	if(g_FilesCur-1 < g_FilesStartAt || g_FilesCur > g_FilesStopAt)
		return;

	CUIRect Button;
	pView->HSplitTop(15.0f, &Button, pView);
	pView->HSplitTop(2.0f, 0, pView);
	//char buf[512];

	if(DoButton_File((void*)(10+(int)Button.y), pName, 0, &Button, 0, 0))
	{
		str_copy(gs_FileDialogFileName, pName, sizeof(gs_FileDialogFileName));

		gs_aFileDialogCompleteFilename[0] = 0;
		str_append(gs_aFileDialogCompleteFilename, gs_aFileDialogPath, sizeof(gs_aFileDialogCompleteFilename));
		str_append(gs_aFileDialogCompleteFilename, gs_FileDialogFileName, sizeof(gs_aFileDialogCompleteFilename));

		if(Input()->MouseDoubleClick())
		{
			if(gs_pfnFileDialogFunc)
				gs_pfnFileDialogFunc(gs_aFileDialogCompleteFilename, this);
			m_Dialog = DIALOG_NONE;
		}
	}
}

void CEditor::RenderFileDialog()
{
	// GUI coordsys
	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);

	CUIRect View = *UI()->Screen();
	RenderTools()->DrawUIRect(&View, vec4(0,0,0,0.25f), 0, 0);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	RenderTools()->DrawUIRect(&View, vec4(0,0,0,0.75f), CUI::CORNER_ALL, 5.0f);
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, Scroll;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, 0, &View); // some spacing
	View.HSplitBottom(14.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, 0); // some spacing
	View.HSplitBottom(14.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.VSplitRight(15.0f, &View, &Scroll);

	// title
	RenderTools()->DrawUIRect(&Title, vec4(1, 1, 1, 0.25f), CUI::CORNER_ALL, 4.0f);
	Title.VMargin(10.0f, &Title);
	UI()->DoLabel(&Title, gs_pFileDialogTitle, 12.0f, -1, -1);

	// filebox
	static int s_FileBoxId = 0;
	UI()->DoLabel(&FileBoxLabel, Localize("Filename:"), 10.0f, -1, -1);
	DoEditBox(&s_FileBoxId, &FileBox, gs_FileDialogFileName, sizeof(gs_FileDialogFileName), 10.0f);

	gs_aFileDialogCompleteFilename[0] = 0;
	str_append(gs_aFileDialogCompleteFilename, gs_aFileDialogPath, sizeof(gs_aFileDialogCompleteFilename));
	str_append(gs_aFileDialogCompleteFilename, gs_FileDialogFileName, sizeof(gs_aFileDialogCompleteFilename));

	int Num = (int)(View.h/17.0);
	static float s_ScrollValue = 0;
	static int ScrollBar = 0;
	Scroll.HMargin(5.0f, &Scroll);
	s_ScrollValue = UiDoScrollbarV(&ScrollBar, &Scroll, s_ScrollValue);

	int ScrollNum = gs_FilesNum-Num+10;
	if(ScrollNum > 0)
	{
		if(Input()->KeyPresses(KEY_MOUSE_WHEEL_UP))
			s_ScrollValue -= 3.0f/ScrollNum;
		if(Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN))
			s_ScrollValue += 3.0f/ScrollNum;

		if(s_ScrollValue < 0) s_ScrollValue = 0;
		if(s_ScrollValue > 1) s_ScrollValue = 1;
	}
	else
		ScrollNum = 0;

	g_FilesStartAt = (int)(ScrollNum*s_ScrollValue);
	if(g_FilesStartAt < 0)
		g_FilesStartAt = 0;

	g_FilesStopAt = g_FilesStartAt+Num;

	g_FilesCur = 0;

	// set clipping
	UI()->ClipEnable(&View);

	// TODO: lazy ass coding, should store the interface pointer somewere
	Kernel()->RequestInterface<IStorage>()->ListDirectory(gs_FileDialogDirTypes, gs_aFileDialogPath, EditorListdirCallback, 0);

	for(int i = 0; i < gs_FileList.size(); i++)
		AddFileDialogEntry(gs_FileList[i].cstr(), &View);
	gs_FileList.clear();

	// disable clipping again
	UI()->ClipDisable();

	// the buttons
	static int s_OkButton = 0;
	static int s_CancelButton = 0;

	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_OkButton, gs_pFileDialogButtonText, 0, &Button, 0, 0) || Input()->KeyPressed(KEY_RETURN))
	{
		if(gs_pfnFileDialogFunc)
			gs_pfnFileDialogFunc(gs_aFileDialogCompleteFilename, gs_pFileDialogUser);
		m_Dialog = DIALOG_NONE;
	}

	ButtonBar.VSplitRight(40.0f, &ButtonBar, &Button);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_CancelButton, Localize("Cancel"), 0, &Button, 0, 0) || Input()->KeyPressed(KEY_ESCAPE))
		m_Dialog = DIALOG_NONE;
}

void CEditor::InvokeFileDialog(int ListDirTypes, const char *pTitle, const char *pButtonText,
	const char *pBasePath, const char *pDefaultName,
	void (*pfnFunc)(const char *pFileName, void *pUser), void *pUser)
{
	gs_FileDialogDirTypes = ListDirTypes;
	gs_pFileDialogTitle = pTitle;
	gs_pFileDialogButtonText = pButtonText;
	gs_pfnFileDialogFunc = pfnFunc;
	gs_pFileDialogUser = pUser;
	gs_FileDialogFileName[0] = 0;
	gs_aFileDialogPath[0] = 0;

	if(pDefaultName)
		str_copy(gs_FileDialogFileName, pDefaultName, sizeof(gs_FileDialogFileName));
	if(pBasePath)
		str_copy(gs_aFileDialogPath, pBasePath, sizeof(gs_aFileDialogPath));

	m_Dialog = DIALOG_FILE;
}



void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Button;

	// mode buttons
	{
		View.VSplitLeft(65.0f, &Button, &View);
		Button.HSplitTop(30.0f, 0, &Button);
		static int s_Button = 0;
		const char *pButName = m_Mode == MODE_LAYERS ? Localize("Layers") : Localize("Images");
		if(DoButton_Tab(&s_Button, pButName, 0, &Button, 0, Localize("Switch between images and layers managment.")))
		{
		    if(m_Mode == MODE_LAYERS)
                m_Mode = MODE_IMAGES;
            else
                m_Mode = MODE_LAYERS;
		}
	}

	View.VSplitLeft(5.0f, 0, &View);
}

void CEditor::RenderStatusbar(CUIRect View)
{
	CUIRect Button;
	View.VSplitRight(60.0f, &View, &Button);
	static int s_EnvelopeButton = 0;
	if(DoButton_Editor(&s_EnvelopeButton, Localize("Envelopes"), m_ShowEnvelopeEditor, &Button, 0, Localize("Toggles the envelope editor.")))
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor+1)%4;

	if(m_pTooltip)
	{
		if(ms_pUiGotContext && ms_pUiGotContext == UI()->HotItem())
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), Localize("%s Right click for context menu."), m_pTooltip);
			UI()->DoLabel(&View, aBuf, 10.0f, -1, -1);
		}
		else
			UI()->DoLabel(&View, m_pTooltip, 10.0f, -1, -1);
	}
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	if(m_SelectedEnvelope < 0) m_SelectedEnvelope = 0;
	if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size()) m_SelectedEnvelope = m_Map.m_lEnvelopes.size()-1;

	CEnvelope *pEnvelope = 0;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < m_Map.m_lEnvelopes.size())
		pEnvelope = m_Map.m_lEnvelopes[m_SelectedEnvelope];

	CUIRect ToolBar, CurveBar, ColorBar;
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	// do the toolbar
	{
		CUIRect Button;
		CEnvelope *pNewEnv = 0;

		// Delete button
		if(m_Map.m_lEnvelopes.size())
		{
			ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			static int s_DelButton = 0;
			if(DoButton_Editor(&s_DelButton, Localize("Delete"), 0, &Button, 0, Localize("Delete this envelope")))
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
		
			// little space
			ToolBar.VSplitRight(10.0f, &ToolBar, &Button);
		}
		
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, Localize("Color+"), 0, &Button, 0, Localize("Creates a new color envelope")))
			pNewEnv = m_Map.NewEnvelope(4);

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, Localize("Pos.+"), 0, &Button, 0, Localize("Creates a new pos envelope")))
			pNewEnv = m_Map.NewEnvelope(3);
			
		// Delete button
		if(m_SelectedEnvelope >= 0)
		{
			ToolBar.VSplitRight(10.0f, &ToolBar, &Button);
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			static int s_DelButton = 0;
			if(DoButton_Editor(&s_DelButton, Localize("Delete"), 0, &Button, 0, Localize("Delete this envelope")))
			{
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_lEnvelopes.size()-1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_lEnvelopes[m_SelectedEnvelope] : 0;
			}
		}

		if(pNewEnv) // add the default points
		{
			if(pNewEnv->m_Channels == 4)
			{
				pNewEnv->AddPoint(0, 1,1,1,1);
				pNewEnv->AddPoint(1000, 1,1,1,1);
			}
			else
			{
				pNewEnv->AddPoint(0, 0);
				pNewEnv->AddPoint(1000, 0);
			}
		}

		CUIRect Shifter, Inc, Dec;
		ToolBar.VSplitLeft(60.0f, &Shifter, &ToolBar);
		Shifter.VSplitRight(15.0f, &Shifter, &Inc);
		Shifter.VSplitLeft(15.0f, &Dec, &Shifter);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),"%d/%d", m_SelectedEnvelope+1, m_Map.m_lEnvelopes.size());
		RenderTools()->DrawUIRect(&Shifter, vec4(1,1,1,0.5f), 0, 0.0f);
		UI()->DoLabel(&Shifter, aBuf, 10.0f, 0, -1);

		static int s_PrevButton = 0;
		if(DoButton_ButtonDec(&s_PrevButton, 0, 0, &Dec, 0, Localize("Previous Envelope")))
			m_SelectedEnvelope--;

		static int s_NextButton = 0;
		if(DoButton_ButtonInc(&s_NextButton, 0, 0, &Inc, 0, Localize("Next Envelope")))
			m_SelectedEnvelope++;

		if(pEnvelope)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(35.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, Localize("Name:"), 10.0f, -1, -1);

			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);

			static int s_NameBox = 0;
			DoEditBox(&s_NameBox, &Button, pEnvelope->m_aName, sizeof(pEnvelope->m_aName), 10.0f);
		}
	}

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->m_Channels == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
		RenderBackground(ColorBar, ms_CheckerTexture, 16.0f, 1.0f);
	}

	RenderBackground(View, ms_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		static array<int> Selection;
		static int sEnvelopeEditorId = 0;
		static int s_ActiveChannels = 0xf;

		if(pEnvelope)
		{
			CUIRect Button;

			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

			static const char *s_paNames[4][4] = {
				{"X", "", "", ""},
				{"X", "Y", "", ""},
				{"X", "Y", "R", ""},
				{"R", "G", "B", "A"},
			};

			static int s_aChannelButtons[4] = {0};
			int Bit = 1;
			//ui_draw_button_func draw_func;

			for(int i = 0; i < pEnvelope->m_Channels; i++, Bit<<=1)
			{
				ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

				/*if(i == 0) draw_func = draw_editor_button_l;
				else if(i == envelope->channels-1) draw_func = draw_editor_button_r;
				else draw_func = draw_editor_button_m;*/

				if(DoButton_Editor(&s_aChannelButtons[i], s_paNames[pEnvelope->m_Channels-1][i], s_ActiveChannels&Bit, &Button, 0, 0))
					s_ActiveChannels ^= Bit;
			}
		}

		float EndTime = pEnvelope->EndTime();
		if(EndTime < 1)
			EndTime = 1;

		pEnvelope->FindTopBottom(s_ActiveChannels);
		float Top = pEnvelope->m_Top;
		float Bottom = pEnvelope->m_Bottom;

		if(Top < 1)
			Top = 1;
		if(Bottom >= 0)
			Bottom = 0;

		float TimeScale = EndTime/View.w;
		float ValueScale = (Top-Bottom)/View.h;

		if(UI()->MouseInside(&View))
			UI()->SetHotItem(&sEnvelopeEditorId);

		if(UI()->HotItem() == &sEnvelopeEditorId)
		{
			// do stuff
			if(pEnvelope)
			{
				if(UI()->MouseButtonClicked(1))
				{
					// add point
					int Time = (int)(((UI()->MouseX()-View.x)*TimeScale)*1000.0f);
					//float env_y = (UI()->MouseY()-view.y)/TimeScale;
					float aChannels[4];
					pEnvelope->Eval(Time, aChannels);
					pEnvelope->AddPoint(Time,
						f2fx(aChannels[0]), f2fx(aChannels[1]),
						f2fx(aChannels[2]), f2fx(aChannels[3]));
				}

				m_pTooltip = Localize("Press right mouse button to create a new point");
			}
		}

		vec3 aColors[] = {vec3(1,0.2f,0.2f), vec3(0.2f,1,0.2f), vec3(0.2f,0.2f,1), vec3(1,1,0.2f)};

		// render lines
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureSet(-1);
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(s_ActiveChannels&(1<<c))
					Graphics()->SetColor(aColors[c].r,aColors[c].g,aColors[c].b,1);
				else
					Graphics()->SetColor(aColors[c].r*0.5f,aColors[c].g*0.5f,aColors[c].b*0.5f,1);

				float PrevX = 0;
				float aResults[4];
				pEnvelope->Eval(0.000001f, aResults);
				float PrevValue = aResults[c];

				int Steps = (int)((View.w/UI()->Screen()->w) * Graphics()->ScreenWidth());
				for(int i = 1; i <= Steps; i++)
				{
					float a = i/(float)Steps;
					pEnvelope->Eval(a*EndTime, aResults);
					float v = aResults[c];
					v = (v-Bottom)/(Top-Bottom);

					IGraphics::CLineItem LineItem(View.x + PrevX*View.w, View.y+View.h - PrevValue*View.h, View.x + a*View.w, View.y+View.h - v*View.h);
					Graphics()->LinesDraw(&LineItem, 1);
					PrevX = a;
					PrevValue = v;
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render curve options
		{
			for(int i = 0; i < pEnvelope->m_lPoints.size()-1; i++)
			{
				float t0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
				float t1 = pEnvelope->m_lPoints[i+1].m_Time/1000.0f/EndTime;

				//dbg_msg("", "%f", end_time);

				CUIRect v;
				v.x = CurveBar.x + (t0+(t1-t0)*0.5f) * CurveBar.w;
				v.y = CurveBar.y;
				v.h = CurveBar.h;
				v.w = CurveBar.h;
				v.x -= v.w/2;
				void *pId = &pEnvelope->m_lPoints[i].m_Curvetype;
				const char *paTypeName[] = {
					"N", "L", "S", "F", "M"
					};

				if(DoButton_Editor(pId, paTypeName[pEnvelope->m_lPoints[i].m_Curvetype], 0, &v, 0, Localize("Switch curve type")))
					pEnvelope->m_lPoints[i].m_Curvetype = (pEnvelope->m_lPoints[i].m_Curvetype+1)%NUM_CURVETYPES;
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			for(int i = 0; i < pEnvelope->m_lPoints.size()-1; i++)
			{
				float r0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[0]);
				float g0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[1]);
				float b0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[2]);
				float a0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[3]);
				float r1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[0]);
				float g1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[1]);
				float b1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[2]);
				float a1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[3]);

				IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, r0, g0, b0, a0),
													IGraphics::CColorVertex(1, r1, g1, b1, a1),
													IGraphics::CColorVertex(2, r1, g1, b1, a1),
													IGraphics::CColorVertex(3, r0, g0, b0, a0)};
				Graphics()->SetColorVertex(Array, 4);

				float x0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
//				float y0 = (fx2f(envelope->points[i].values[c])-bottom)/(top-bottom);
				float x1 = pEnvelope->m_lPoints[i+1].m_Time/1000.0f/EndTime;
				//float y1 = (fx2f(envelope->points[i+1].values[c])-bottom)/(top-bottom);
				CUIRect v;
				v.x = ColorBar.x + x0*ColorBar.w;
				v.y = ColorBar.y;
				v.w = (x1-x0)*ColorBar.w;
				v.h = ColorBar.h;

				IGraphics::CQuadItem QuadItem(v.x, v.y, v.w, v.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();
		}

		// render handles
		{
			static bool s_Move = false;

			int CurrentValue = 0, CurrentTime = 0;

			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(!(s_ActiveChannels&(1<<c)))
					continue;

				for(int i = 0; i < pEnvelope->m_lPoints.size(); i++)
				{
					float x0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
					float y0 = (fx2f(pEnvelope->m_lPoints[i].m_aValues[c])-Bottom)/(Top-Bottom);
					CUIRect Final;
					Final.x = View.x + x0*View.w;
					Final.y = View.y+View.h - y0*View.h;
					Final.x -= 2.0f;
					Final.y -= 2.0f;
					Final.w = 4.0f;
					Final.h = 4.0f;

					void *pId = &pEnvelope->m_lPoints[i].m_aValues[c];

					if(UI()->MouseInside(&Final))
						UI()->SetHotItem(pId);

					float ColorMod = 1.0f;

					if(UI()->ActiveItem() == pId)
					{
						if(!UI()->MouseButton(0))
						{
							UI()->SetActiveItem(0);
							s_Move = false;
						}
						else
						{
							if((Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
								pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY*0.001f);
							else
								pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY*ValueScale);
							
							if(Input()->KeyPressed(KEY_LSHIFT) || Input()->KeyPressed(KEY_RSHIFT))
							{
								if(i != 0)
								{
									if((Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
										pEnvelope->m_lPoints[i].m_Time += (int)((m_MouseDeltaX));
									else
										pEnvelope->m_lPoints[i].m_Time += (int)((m_MouseDeltaX*TimeScale)*1000.0f);
									if(pEnvelope->m_lPoints[i].m_Time < pEnvelope->m_lPoints[i-1].m_Time)
										pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i-1].m_Time + 1;
									if(i+1 != pEnvelope->m_lPoints.size() && pEnvelope->m_lPoints[i].m_Time > pEnvelope->m_lPoints[i+1].m_Time)
										pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i+1].m_Time - 1;
								}
							}
						}

						ColorMod = 100.0f;
						Graphics()->SetColor(1,1,1,1);
					}
					else if(UI()->HotItem() == pId)
					{
						if(UI()->MouseButton(0))
						{
							Selection.clear();
							Selection.add(i);
							UI()->SetActiveItem(pId);
						}

						// remove point
						if(UI()->MouseButtonClicked(1))
							pEnvelope->m_lPoints.remove_index(i);

						ColorMod = 100.0f;
						Graphics()->SetColor(1,0.75f,0.75f,1);
						m_pTooltip = Localize("Left mouse to drag. Hold ctfl to be more precise. Hold shift to alter time point aswell. Right click to delete.");
					}

					if(UI()->ActiveItem() == pId || UI()->HotItem() == pId)
					{
						CurrentTime = pEnvelope->m_lPoints[i].m_Time;
						CurrentValue = pEnvelope->m_lPoints[i].m_aValues[c];
					}

					Graphics()->SetColor(aColors[c].r*ColorMod, aColors[c].g*ColorMod, aColors[c].b*ColorMod, 1.0f);
					IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
			Graphics()->QuadsEnd();

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf),"%.3f %.3f", CurrentTime/1000.0f, fx2f(CurrentValue));
			UI()->DoLabel(&ToolBar, aBuf, 10.0f, 0, -1);
		}
	}
}

int CEditor::PopupMenuFile(CEditor *pEditor, CUIRect View)
{
	static int s_NewMapButton = 0;
	static int s_SaveButton = 0;
	static int s_SaveAsButton = 0;
	static int s_OpenButton = 0;
	static int s_AppendButton = 0;
	static int s_ExitButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_NewMapButton, Localize("New"), 0, &Slot, 0, Localize("Creates a new map")))
	{
		pEditor->Reset();
		pEditor->m_aFileName[0] = 0;
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_OpenButton, Localize("Open"), 0, &Slot, 0, Localize("Opens a map for editing")))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, Localize("Open map"), Localize("Open"), "maps/", "", CallbackOpenMap, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_AppendButton, Localize("Append"), 0, &Slot, 0, Localize("Opens a map and adds everything from that map to the current one")))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, Localize("Append map"), Localize("Append"), "maps/", "", CallbackAppendMap, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveButton, Localize("Save"), 0, &Slot, 0, Localize("Saves the current map")))
	{
		if(pEditor->m_aFileName[0])	
			CallbackSaveMap(pEditor->m_aFileName, pEditor);
		else
			pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, Localize("Save Map"), Localize("Save"), "maps/", "", CallbackSaveMap, pEditor);
		return 1;
	}

	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveAsButton, Localize("Save As"), 0, &Slot, 0, Localize("Saves the current map under a new name")))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, Localize("Save Map"), Localize("Save"), "maps/", "", CallbackSaveMap, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ExitButton, "Exit", 0, &Slot, 0, Localize("Exits from the editor")))
	{
		g_Config.m_ClEditor = 0;
		return 1;
	}

	return 0;
}

void CEditor::RenderMenubar(CUIRect MenuBar)
{
	static CUIRect s_File /*, view, help*/;

	MenuBar.VSplitLeft(60.0f, &s_File, &MenuBar);
	if(DoButton_Menu(&s_File, Localize("File"), 0, &s_File, 0, 0))
		UiInvokePopupMenu(&s_File, 1, s_File.x, s_File.y+s_File.h-1.0f, 120, 150, PopupMenuFile, this);

	/*
	menubar.VSplitLeft(5.0f, 0, &menubar);
	menubar.VSplitLeft(60.0f, &view, &menubar);
	if(do_editor_button(&view, "View", 0, &view, draw_editor_button_menu, 0, 0))
		(void)0;

	menubar.VSplitLeft(5.0f, 0, &menubar);
	menubar.VSplitLeft(60.0f, &help, &menubar);
	if(do_editor_button(&help, "Help", 0, &help, draw_editor_button_menu, 0, 0))
		(void)0;
		*/
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(1.0f, 0.0f, 1.0f);
	CUIRect View = *UI()->Screen();
	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);

	// reset tip
	m_pTooltip = 0;

	// render checker
	RenderBackground(View, ms_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, CModeBar, ToolBar, StatusBar, EnvelopeEditor, ToolBox;

	if(m_GuiActive)
	{

		View.HSplitTop(16.0f, &MenuBar, &View);
		View.HSplitTop(53.0f, &ToolBar, &View);
		View.VSplitLeft(100.0f, &ToolBox, &View);
		View.HSplitBottom(16.0f, &View, &StatusBar);

		if(m_ShowEnvelopeEditor)
		{
			float size = 125.0f;
			if(m_ShowEnvelopeEditor == 2)
				size *= 2.0f;
			else if(m_ShowEnvelopeEditor == 3)
				size *= 3.0f;
			View.HSplitBottom(size, &View, &EnvelopeEditor);
		}
	}

	//	a little hack for now
	if(m_Mode == MODE_LAYERS)
		DoMapEditor(View, ToolBar);

	if(m_GuiActive)
	{
		float Brightness = 0.25f;
		RenderBackground(MenuBar, ms_BackgroundTexture, 128.0f, Brightness*0);
		MenuBar.Margin(2.0f, &MenuBar);

		RenderBackground(ToolBox, ms_BackgroundTexture, 128.0f, Brightness);
		ToolBox.Margin(2.0f, &ToolBox);

		RenderBackground(ToolBar, ms_BackgroundTexture, 128.0f, Brightness);
		ToolBar.Margin(2.0f, &ToolBar);
		ToolBar.VSplitLeft(100.0f, &CModeBar, &ToolBar);

		RenderBackground(StatusBar, ms_BackgroundTexture, 128.0f, Brightness);
		StatusBar.Margin(2.0f, &StatusBar);

		// do the toolbar
		if(m_Mode == MODE_LAYERS)
			DoToolbar(ToolBar);

		if(m_ShowEnvelopeEditor)
		{
			RenderBackground(EnvelopeEditor, ms_BackgroundTexture, 128.0f, Brightness);
			EnvelopeEditor.Margin(2.0f, &EnvelopeEditor);
		}
	}


	if(m_Mode == MODE_LAYERS)
		RenderLayers(ToolBox, ToolBar, View);
	else if(m_Mode == MODE_IMAGES)
		RenderImages(ToolBox, ToolBar, View);

	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);

	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);

		RenderModebar(CModeBar);
		if(m_ShowEnvelopeEditor)
			RenderEnvelopeEditor(EnvelopeEditor);
	}

	if(m_Dialog == DIALOG_FILE)
	{
		static int s_NullUiTarget = 0;
		UI()->SetHotItem(&s_NullUiTarget);
		RenderFileDialog();
	}


	UiDoPopupMenu();

	if(m_GuiActive)
		RenderStatusbar(StatusBar);

	//
	if(g_Config.m_EdShowkeys)
	{
		Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, View.x+10, View.y+View.h-24-10, 24.0f, TEXTFLAG_RENDER);

		int NKeys = 0;
		for(int i = 0; i < KEY_LAST; i++)
		{
			if(Input()->KeyPressed(i))
			{
				if(NKeys)
					TextRender()->TextEx(&Cursor, " + ", -1);
				TextRender()->TextEx(&Cursor, Input()->KeyName(i), -1);
				NKeys++;
			}
		}
	}

	if(m_ShowMousePointer)
	{
		// render butt ugly mouse cursor
		float mx = UI()->MouseX();
		float my = UI()->MouseY();
		Graphics()->TextureSet(ms_CursorTexture);
		Graphics()->QuadsBegin();
		if(ms_pUiGotContext == UI()->HotItem())
			Graphics()->SetColor(1,0,0,1);
		IGraphics::CQuadItem QuadItem(mx,my, 16.0f, 16.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

}

void CEditor::Reset(bool CreateDefault)
{
	m_Map.Clean();

	// create default layers
	if(CreateDefault)
		m_Map.CreateDefault(ms_EntitiesTexture);

	/*
	{
	}*/

	m_SelectedLayer = 0;
	m_SelectedGroup = 0;
	m_SelectedQuad = -1;
	m_SelectedPoints = 0;
	m_SelectedEnvelope = 0;
	m_SelectedImage = 0;
	
	m_WorldOffsetX = 0;
	m_WorldOffsetY = 0;
	m_EditorOffsetX = 0.0f;
	m_EditorOffsetY = 0.0f;
	
	m_WorldZoom = 1.0f;
	m_ZoomLevel = 200;

	m_MouseDeltaX = 0;
	m_MouseDeltaY = 0;
	m_MouseDeltaWx = 0;
	m_MouseDeltaWy = 0;
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= m_lEnvelopes.size())
		return;

	// fix links between envelopes and quads
	for(int i = 0; i < m_lGroups.size(); ++i)
		for(int j = 0; j < m_lGroups[i]->m_lLayers.size(); ++j)
			if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *Layer = static_cast<CLayerQuads *>(m_lGroups[i]->m_lLayers[j]);
				for(int k = 0; k < Layer->m_lQuads.size(); ++k)
				{
					if(Layer->m_lQuads[k].m_PosEnv == Index)
						Layer->m_lQuads[k].m_PosEnv = -1;
					else if(Layer->m_lQuads[k].m_PosEnv > Index)
						Layer->m_lQuads[k].m_PosEnv--;
					if(Layer->m_lQuads[k].m_ColorEnv == Index)
						Layer->m_lQuads[k].m_ColorEnv = -1;
					else if(Layer->m_lQuads[k].m_ColorEnv > Index)
						Layer->m_lQuads[k].m_ColorEnv--;
				}
			}

	m_lEnvelopes.remove_index(Index);
}

void CEditorMap::MakeGameLayer(CLayer *pLayer)
{
	m_pGameLayer = (CLayerGame *)pLayer;
	m_pGameLayer->m_pEditor = m_pEditor;
	m_pGameLayer->m_TexId = m_pEditor->ms_EntitiesTexture;
}

void CEditorMap::MakeTeleLayer(CLayer *pLayer)
{
	m_pTeleLayer = (CLayerTele *)pLayer;
	m_pTeleLayer->m_pEditor = m_pEditor;
	m_pTeleLayer->m_TexId = m_pEditor->ms_EntitiesTexture;
}

void CEditorMap::MakeSpeedupLayer(CLayer *pLayer)
{
	m_pSpeedupLayer = (CLayerSpeedup *)pLayer;
	m_pSpeedupLayer->m_pEditor = m_pEditor;
	m_pSpeedupLayer->m_TexId = m_pEditor->ms_EntitiesTexture;
}

void CEditorMap::MakeGameGroup(CLayerGroup *pGroup)
{
	m_pGameGroup = pGroup;
	m_pGameGroup->m_GameGroup = true;
	m_pGameGroup->m_pName = "Game";
}



void CEditorMap::Clean()
{
	m_lGroups.delete_all();
	m_lEnvelopes.delete_all();
	m_lImages.delete_all();

	m_pGameLayer = 0x0;
	m_pTeleLayer = 0x0;
	m_pSpeedupLayer = 0x0;
	m_pGameGroup = 0x0;
}

void CEditorMap::CreateDefault(int EntitiesTexture)
{
	// add background
	CLayerGroup *pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	CLayerQuads *pLayer = new CLayerQuads;
	pLayer->m_pEditor = m_pEditor;
	CQuad *pQuad = pLayer->NewQuad();
	const int Width = 800000;
	const int Height = 600000;
	pQuad->m_aPoints[0].x = pQuad->m_aPoints[2].x = -Width;
	pQuad->m_aPoints[1].x = pQuad->m_aPoints[3].x = Width;
	pQuad->m_aPoints[0].y = pQuad->m_aPoints[1].y = -Height;
	pQuad->m_aPoints[2].y = pQuad->m_aPoints[3].y = Height;
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 94;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 132;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 174;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 204;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 232;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pGroup->AddLayer(pLayer);

	// add game layer
	MakeGameGroup(NewGroup());
	MakeGameLayer(new CLayerGame(50, 50));
	m_pGameGroup->AddLayer(m_pGameLayer);
	m_pTeleLayer = 0x0;
	m_pSpeedupLayer = 0x0;
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_RenderTools.m_pGraphics = m_pGraphics;
	m_RenderTools.m_pUI = &m_UI;
	m_UI.SetGraphics(m_pGraphics, m_pTextRender);
	m_Map.m_pEditor = this;

	ms_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", CImageInfo::FORMAT_AUTO, 0);
	ms_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", CImageInfo::FORMAT_AUTO, 0);
	ms_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", CImageInfo::FORMAT_AUTO, 0);
	ms_EntitiesTexture = Graphics()->LoadTexture("editor/entities.png", CImageInfo::FORMAT_AUTO, 0);

	m_TilesetPicker.m_pEditor = this;
	m_TilesetPicker.MakePalette();
	m_TilesetPicker.m_Readonly = true;

	m_Brush.m_pMap = &m_Map;

	Reset();
}

void CEditor::DoMapBorder()
{
    CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
    
    for(int i = 0; i < pT->m_Width*2; ++i)
        pT->m_pTiles[i].m_Index = 1;
        
    for(int i = 0; i < pT->m_Width*pT->m_Height; ++i)
    {
        if(i%pT->m_Width < 2 || i%pT->m_Width > pT->m_Width-3)
            pT->m_pTiles[i].m_Index = 1;
    }
    
    for(int i = (pT->m_Width*(pT->m_Height-2)); i < pT->m_Width*pT->m_Height; ++i)
        pT->m_pTiles[i].m_Index = 1;
}

void CEditor::UpdateAndRender()
{
	static int s_MouseX = 0;
	static int s_MouseY = 0;

	if(m_Animate)
		m_AnimateTime = (time_get()-m_AnimateStart)/(float)time_freq();
	else
		m_AnimateTime = 0;
	ms_pUiGotContext = 0;

	// handle mouse movement
	float mx, my, Mwx, Mwy;
	int rx, ry;
	{
		Input()->MouseRelative(&rx, &ry);
		m_MouseDeltaX = rx;
		m_MouseDeltaY = ry;

		if(!m_LockMouse)
		{
			s_MouseX += rx;
			s_MouseY += ry;
		}

		if(s_MouseX < 0) s_MouseX = 0;
		if(s_MouseY < 0) s_MouseY = 0;
		if(s_MouseX > UI()->Screen()->w) s_MouseX = (int)UI()->Screen()->w;
		if(s_MouseY > UI()->Screen()->h) s_MouseY = (int)UI()->Screen()->h;

		// update the ui
		mx = s_MouseX;
		my = s_MouseY;
		Mwx = 0;
		Mwy = 0;

		// fix correct world x and y
		CLayerGroup *g = GetSelectedGroup();
		if(g)
		{
			float aPoints[4];
			g->Mapping(aPoints);

			float WorldWidth = aPoints[2]-aPoints[0];
			float WorldHeight = aPoints[3]-aPoints[1];

			Mwx = aPoints[0] + WorldWidth * (s_MouseX/UI()->Screen()->w);
			Mwy = aPoints[1] + WorldHeight * (s_MouseY/UI()->Screen()->h);
			m_MouseDeltaWx = m_MouseDeltaX*(WorldWidth / UI()->Screen()->w);
			m_MouseDeltaWy = m_MouseDeltaY*(WorldHeight / UI()->Screen()->h);
		}

		int Buttons = 0;
		if(Input()->KeyPressed(KEY_MOUSE_1)) Buttons |= 1;
		if(Input()->KeyPressed(KEY_MOUSE_2)) Buttons |= 2;
		if(Input()->KeyPressed(KEY_MOUSE_3)) Buttons |= 4;

		UI()->Update(mx,my,Mwx,Mwy,Buttons);
	}

	// toggle gui
	if(Input()->KeyDown(KEY_TAB))
		m_GuiActive = !m_GuiActive;

	if(Input()->KeyDown(KEY_F5))
		Save("maps/debug_test2.map");

	if(Input()->KeyDown(KEY_F6))
		Load("maps/debug_test2.map");
	
	if(Input()->KeyDown(KEY_F8))
		Load("maps/debug_test.map");
	
	if(Input()->KeyDown(KEY_F7))
		Save("maps/quicksave.map");

	if(Input()->KeyDown(KEY_F10))
		m_ShowMousePointer = false;

	Render();

	if(Input()->KeyDown(KEY_F10))
	{
		Graphics()->TakeScreenshot();
		m_ShowMousePointer = true;
	}

	Input()->ClearEvents();
}

IEditor *CreateEditor() { return new CEditor; }

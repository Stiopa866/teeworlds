#include <engine/config.h>
#include <engine/shared/config.h>
#include "binds.h"

bool CBinds::CBindsSpecial::OnInput(IInput::CEvent Event)
{
	// don't handle invalid events and keys that arn't set to anything
	if(Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F15 && m_pBinds->m_aaKeyBindings[Event.m_Key][0] != 0)
	{
		int Stroke = 0;
		if(Event.m_Flags&IInput::FLAG_PRESS)
			Stroke = 1;
			
		m_pBinds->GetConsole()->ExecuteLineStroked(Stroke, m_pBinds->m_aaKeyBindings[Event.m_Key]);
		return true;
	}
	
	return false;
}

CBinds::CBinds()
{
	mem_zero(m_aaKeyBindings, sizeof(m_aaKeyBindings));
	m_SpecialBinds.m_pBinds = this;
}

void CBinds::Bind(int KeyId, const char *pStr)
{
	if(KeyId < 0 || KeyId >= KEY_LAST)
		return;
		
	str_copy(m_aaKeyBindings[KeyId], pStr, sizeof(m_aaKeyBindings[KeyId]));
	if(!m_aaKeyBindings[KeyId][0])
		dbg_msg("binds", "unbound %s (%d)", Input()->KeyName(KeyId), KeyId);
	else
		dbg_msg("binds", "bound %s (%d) = %s", Input()->KeyName(KeyId), KeyId, m_aaKeyBindings[KeyId]);
}


bool CBinds::OnInput(IInput::CEvent e)
{
	// don't handle invalid events and keys that arn't set to anything
	if(e.m_Key <= 0 || e.m_Key >= KEY_LAST || m_aaKeyBindings[e.m_Key][0] == 0)
		return false;

	int Stroke = 0;
	if(e.m_Flags&IInput::FLAG_PRESS)
		Stroke = 1;
	Console()->ExecuteLineStroked(Stroke, m_aaKeyBindings[e.m_Key]);
	return true;
}

void CBinds::UnbindAll()
{
	for(int i = 0; i < KEY_LAST; i++)
		m_aaKeyBindings[i][0] = 0;
}

const char *CBinds::Get(int KeyId)
{
	if(KeyId > 0 && KeyId < KEY_LAST)
		return m_aaKeyBindings[KeyId];
	return "";
}

const char *CBinds::GetKey(const char *pBindStr)
{
	for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
	{
		const char *pBind = Get(KeyId);
		if(!pBind[0])
			continue;
			
		if(str_comp(pBind, pBindStr) == 0)
			return Input()->KeyName(KeyId);
	}
	
	return "";
}

void CBinds::SetDefaults()
{
	// set default key bindings
	UnbindAll();
	Bind(KEY_F1, "toggle_local_console");
	Bind(KEY_F2, "toggle_remote_console");
	Bind(KEY_TAB, "+scoreboard");
	Bind('u', "+show_chat");
	Bind(KEY_F10, "screenshot");

	Bind('a', "+left");
	Bind('d', "+right");

	Bind(KEY_SPACE, "+jump");
	Bind(KEY_MOUSE_1, "+fire");
	Bind(KEY_MOUSE_2, "+hook");
	Bind(KEY_LSHIFT, "+emote");

	Bind('1', "+weapon1");
	Bind('2', "+weapon2");
	Bind('3', "+weapon3");
	Bind('4', "+weapon4");
	Bind('5', "+weapon5");
	
	Bind(KEY_MOUSE_WHEEL_UP, "+prevweapon");
	Bind(KEY_MOUSE_WHEEL_DOWN, "+nextweapon");
	
	Bind('t', "chat all");
	Bind('y', "chat team");	

	Bind(KEY_F3, "vote yes");
	Bind(KEY_F4, "vote no");
	
	// lvlx
	Bind(KEY_F5, "coop yes");
	Bind(KEY_F6, "coop no");
	
	Bind(KEY_LCTRL, "+coopboard");
	Bind(KEY_LSHIFT, "+statboard");
}

void CBinds::OnConsoleInit()
{
	// bindings
	IConfig *pConfig = Kernel()->RequestInterface<IConfig>();
	if(pConfig)
		pConfig->RegisterCallback(ConfigSaveCallback, this);
	
	Console()->Register("bind", "sr", CFGFLAG_CLIENT, ConBind, this, "Bind key to execute the command");
	Console()->Register("unbind", "s", CFGFLAG_CLIENT, ConUnbind, this, "Unbind key");
	Console()->Register("unbindall", "", CFGFLAG_CLIENT, ConUnbindAll, this, "Unbind all keys");
	Console()->Register("dump_binds", "", CFGFLAG_CLIENT, ConDumpBinds, this, "Dump binds");
	
	// default bindings
	SetDefaults();
}

void CBinds::ConBind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pKeyName = pResult->GetString(0);
	int id = pBinds->GetKeyId(pKeyName);
	
	if(!id)
	{
		dbg_msg("binds", "key %s not found", pKeyName);
		return;
	}
	
	pBinds->Bind(id, pResult->GetString(1));
}


void CBinds::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pKeyName = pResult->GetString(0);
	int id = pBinds->GetKeyId(pKeyName);
	
	if(!id)
	{
		dbg_msg("binds", "key %s not found", pKeyName);
		return;
	}
	
	pBinds->Bind(id, "");
}


void CBinds::ConUnbindAll(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	pBinds->UnbindAll();
}


void CBinds::ConDumpBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	char aBuf[1024];
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(pBinds->m_aaKeyBindings[i][0] == 0)
			continue;
		str_format(aBuf, sizeof(aBuf), "[binds] %s (%d) = %s", pBinds->Input()->KeyName(i), i, pBinds->m_aaKeyBindings[i]);
		pBinds->Console()->Print(aBuf);
	}
}

int CBinds::GetKeyId(const char *pKeyName)
{
	// check for numeric
	if(pKeyName[0] == '&')
	{
		int i = str_toint(pKeyName+1);
		if(i > 0 && i < KEY_LAST)
			return i; // numeric
	}
		
	// search for key
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(str_comp(pKeyName, Input()->KeyName(i)) == 0)
			return i;
	}
	
	return 0;
}

void CBinds::ConfigSaveCallback(IConfig *pConfig, void *pUserData)
{
	CBinds *pSelf = (CBinds *)pUserData;
	
	char aBuffer[256];
	char *pEnd = aBuffer+sizeof(aBuffer)-8;
	pConfig->WriteLine("unbindall");
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(pSelf->m_aaKeyBindings[i][0] == 0)
			continue;
		str_format(aBuffer, sizeof(aBuffer), "bind %s ", pSelf->Input()->KeyName(i));
		
		// process the string. we need to escape some characters
		const char *pSrc = pSelf->m_aaKeyBindings[i];
		char *pDst = aBuffer + str_length(aBuffer);
		*pDst++ = '"';
		while(*pSrc && pDst < pEnd)
		{
			if(*pSrc == '"' || *pSrc == '\\') // escape \ and "
				*pDst++ = '\\';
			*pDst++ = *pSrc++;
		}
		*pDst++ = '"';
		*pDst++ = 0;
		
		pConfig->WriteLine(aBuffer);
	}
}


//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/06/07

#pragma once
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <Windows.h>

// --> NSIS plugin API
#include <nsis/pluginapi.h>

#undef EXDLL_INIT
#define EXDLL_INIT()           {  \
        g_stringsize=string_size; \
        g_stacktop=stacktop;      \
        g_variables=variables;    \
        g_ep=extra;               \
        g_hwndparent=parent; }

#define EXDLL_VALIDATE() \
	if (g_ep && g_ep->exec_flags && (g_ep->exec_flags->plugin_api_version != NSISPIAPIVER_CURR))  \
		return;

extern extra_parameters	*g_ep;			/// main.c
extern HWND				g_hwndparent;	/// main.c
UINT_PTR __cdecl		PluginCallback( enum NSPIM iMessage );
// <-- NSIS plugin API

#if _DEBUG || DBG
	VOID DebugString( _In_ LPCTSTR pszFormat, _In_opt_ ... );
#else
	#define DebugString(...) ;
#endif

#endif	///_PLATFORM_H_
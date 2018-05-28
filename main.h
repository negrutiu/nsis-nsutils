#pragma once
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <Windows.h>
#include "nsiswapi\pluginapi.h"

#if _DEBUG || DBG
	VOID DebugString( _In_ LPCTSTR pszFormat, _In_opt_ ... );
#else
	#define DebugString(...) ;
#endif

#endif	///_PLATFORM_H_
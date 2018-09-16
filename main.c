
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/06/07

#include "main.h"

// Global variables
HINSTANCE g_hModule = NULL;

// Notifications
extern VOID UtilsUnload();

//++ DllMain
BOOL WINAPI DllMain(
	HINSTANCE hInst,
	ULONG ul_reason_for_call,
	LPVOID lpReserved
	)
{
	if ( ul_reason_for_call == DLL_PROCESS_ATTACH ) {
		g_hModule = hInst;
		CoInitialize( NULL );
	} else if ( ul_reason_for_call == DLL_PROCESS_DETACH ) {
		UtilsUnload();
		g_hModule = NULL;
		CoUninitialize();
	}
	return TRUE;
}

//+ DebugString
#if _DEBUG || DBG
VOID DebugString( _In_ LPCTSTR pszFormat, _In_opt_ ... )
{
	if (pszFormat && *pszFormat) {

		TCHAR szStr[1024];		/// Enough? Dynamic?

		va_list args;
		va_start( args, pszFormat );

		wvsprintf( szStr, pszFormat, args );

		va_end( args );

		OutputDebugString( szStr );
	}
}
#endif
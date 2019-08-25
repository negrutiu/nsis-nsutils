
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/06/07

#include "main.h"

// Global variables
HINSTANCE g_hModule = NULL;

// Notifications
extern VOID UtilsUnload();

// NSIS plugin API
extra_parameters *g_ep = NULL;
HWND g_hwndparent = NULL;


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


//+ PluginCallback
UINT_PTR __cdecl PluginCallback( enum NSPIM iMessage )
{
	switch ( iMessage )
	{
		case NSPIM_UNLOAD:
			DebugString( _T( "NSPIM_UNLOAD\n" ) );
			//x UtilsUnload();	// DLL_PROCESS_DETACH will handle this
			break;

		case NSPIM_GUIUNLOAD:
			DebugString( _T( "NSPIM_GUIUNLOAD\n" ) );
			break;
	}
	return 0;
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
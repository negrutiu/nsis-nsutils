
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


//+ MyStrFind
/// Finds the first occurrence of a substring in a string
/// E.g. MyStrFind( L"aaabbbcccbbb", L"bbb" ) returns pointer to L"bbbcccbbb"
/// If no match is found, NULL is returned
LPCTSTR MyStrFind( _In_ LPCTSTR pszStr, _In_ LPCTSTR pszSubstr, _In_ BOOL bMatchCase )
{
	LPCTSTR psz;
	if ( pszStr && *pszStr && pszSubstr && *pszSubstr ) {
		int iSubstrLen = lstrlen( pszSubstr );
		for (psz = pszStr; *psz; psz++)
			if (CompareString( LOCALE_USER_DEFAULT, (bMatchCase ? 0 : NORM_IGNORECASE), psz, (int)iSubstrLen, pszSubstr, (int)iSubstrLen ) == CSTR_EQUAL)
				return psz;
	}
	return NULL;
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
#include <windows.h>

// Global variables
HINSTANCE g_hModule = NULL;

// Notifications
extern VOID UtilsUnload();

//
// DllMain
//
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

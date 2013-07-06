#include <windows.h>

//	Global variables
///HINSTANCE g_hInst = NULL;

//
//  DllMain
//
BOOL WINAPI DllMain(
	HINSTANCE hInst,
	ULONG ul_reason_for_call,
	LPVOID lpReserved
	)
{
	if ( ul_reason_for_call == DLL_PROCESS_ATTACH ) {
		///g_hInst = hInst;
		CoInitialize( NULL );
	} else if ( ul_reason_for_call == DLL_PROCESS_DETACH ) {
		///g_hInst = NULL;
		CoUninitialize();
	}
	return TRUE;
}

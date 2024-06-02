
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/06/11

#include "main.h"
#include <winioctl.h>
#include <commctrl.h>
#include <intrin.h>


#define CINTERFACE
#include <prsht.h>
#include <gpedit.h>
#pragma comment (lib, "gpedit.lib")

#ifndef PBM_SETSTATE
	#define PBM_SETSTATE (WM_USER+16)
#endif


// Globals
extern HINSTANCE g_hModule;				/// Defined in main.c
HHOOK g_hMessageLoopHook = NULL;

// Timer cache
typedef struct _TMPAIR { int NsisCallback; UINT_PTR TimerID; } TMPAIR;
TMPAIR g_Timers[25] = {0};


//++ UtilsUnload
/// Called when the plugin unloads
VOID UtilsUnload()
{
	int i;
	// If still hooked, unhook the message loop
	if ( g_hMessageLoopHook ) {
		UnhookWindowsHookEx( g_hMessageLoopHook );
		g_hMessageLoopHook = NULL;
		///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] auto false\n"));
	}
	// Kill existing timers
	for (i = 0; i < ARRAYSIZE( g_Timers ); i++) {
		if (g_Timers[i].TimerID != 0) {
			ULONG err = KillTimer( NULL, g_Timers[i].TimerID ) ? ERROR_SUCCESS : GetLastError();
			DebugString( _T( "[NSutils::%hs] KillTimer( NSIS:%d, ID:%u, Idx:%d/%d ) == 0x%x\n" ), __FUNCTION__, g_Timers[i].NsisCallback, g_Timers[i].TimerID, i, ARRAYSIZE(g_Timers), err );
			g_Timers[i].TimerID = 0;
			g_Timers[i].NsisCallback = 0;
			UNREFERENCED_PARAMETER( err );
		}
	}
}


//++ LogWriteFile
DWORD LogWriteFile(
	__in HANDLE hFile,
	__in LPCTSTR pszFormat,
	...
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( hFile && (hFile != INVALID_HANDLE_VALUE) && pszFormat && *pszFormat ) {

		TCHAR szStr[1024];
		int iLen;
		DWORD dwWritten;
		va_list args;

		va_start(args, pszFormat);
		iLen = _vsntprintf( szStr, ARRAYSIZE(szStr), pszFormat, args );
		if ( iLen > 0 ) {

			if ( iLen < ARRAYSIZE(szStr))
				szStr[iLen] = 0;	/// The string is not guaranteed to be null terminated. We'll add the terminator ourselves

			WriteFile( hFile, szStr, iLen * sizeof(TCHAR), &dwWritten, NULL );
		}
		va_end(args);

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ LogCreateFile
HANDLE LogCreateFile(
	__in LPCTSTR pszLogFile,
	__in BOOLEAN bOverwrite
	)
{
	HANDLE hFile = INVALID_HANDLE_VALUE;

	if ( pszLogFile && *pszLogFile ) {

		hFile = CreateFile( pszLogFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, bOverwrite ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( hFile != INVALID_HANDLE_VALUE ) {

			ULARGE_INTEGER iFileSize;
			if ( bOverwrite ||
				 ((iFileSize.LowPart = GetFileSize( hFile, &iFileSize.HighPart ) != INVALID_FILE_SIZE) && ( iFileSize.QuadPart == 0 )))
			{
				/// New file
#ifdef _UNICODE
				DWORD dwWritten;
				WORD iBom = 0xFEFF;
				WriteFile( hFile, &iBom, sizeof(iBom), &dwWritten, NULL );
#endif
			} else {
				if ( !bOverwrite ) {
					/// Existing file
					SetFilePointer( hFile, 0, NULL, FILE_END );
					LogWriteFile( hFile, _T("\r\n"));
				}
			}
		}
	} else {
		SetLastError( ERROR_INVALID_PARAMETER );
	}
	return hFile;
}


//++ LogCloseHandle
BOOL LogCloseHandle( __in HANDLE hFile )
{
	if ( hFile && (hFile != INVALID_HANDLE_VALUE)) {
		return CloseHandle( hFile );
	} else {
		SetLastError( ERROR_INVALID_PARAMETER );
		return FALSE;
	}
}

//++ IsWow64
BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress( GetModuleHandle( _T("kernel32")), "IsWow64Process" );

	if ( fnIsWow64Process && fnIsWow64Process( GetCurrentProcess(), &bIsWow64 ))
		return bIsWow64;

	return FALSE;
}

//++ EnableWow64FsRedirection
BOOLEAN EnableWow64FsRedirection( __in BOOLEAN bEnable )
{
	BOOL bRet = FALSE;

	typedef BOOLEAN (WINAPI *LPFN_WOW64EW64FSR)(BOOLEAN);
	LPFN_WOW64EW64FSR fnEnableFsRedir = (LPFN_WOW64EW64FSR)GetProcAddress( GetModuleHandle( _T("kernel32")), "Wow64EnableWow64FsRedirection" );

	if ( fnEnableFsRedir )
		bRet = fnEnableFsRedir( bEnable );

	return bRet;
}


//+ Subclassing definitions
#define PROP_WNDPROC_OLD				_T("NSutils.WndProc.Old")
#define PROP_WNDPROC_REFCOUNT			_T("NSutils.WndProc.RefCount")
#define PROP_PROGRESSBAR_NOSTEPBACK		_T("NSutils.ProgressBar.NoStepBack")
#define PROP_PROGRESSBAR_REDIRECTWND	_T("NSutils.ProgressBar.RedirectWnd")
#define PROP_BNCLICKED_CALLBACK			_T("NSutils.BN_CLICKED.Callback")


//++ MySubclassWindow
INT_PTR MySubclassWindow(
	__in HWND hWnd,
	__in WNDPROC fnWndProc
	)
{
	INT_PTR iRefCount = 0;

	BOOLEAN bSubclassed = FALSE;
	if ( GetProp( hWnd, PROP_WNDPROC_OLD )) {
		/// Already subclassed
		bSubclassed = TRUE;
	} else {
		/// Subclass now
		WNDPROC fnOldWndProc = (WNDPROC)SetWindowLongPtr( hWnd, GWLP_WNDPROC, (LONG_PTR)fnWndProc );
		if ( fnOldWndProc ) {
			SetProp( hWnd, PROP_WNDPROC_OLD, (HANDLE)fnOldWndProc );
			bSubclassed = TRUE;
		}
	}

	// Update the reference count
	if ( bSubclassed ) {
		iRefCount = (INT_PTR)GetProp( hWnd, PROP_WNDPROC_REFCOUNT );
		iRefCount++;
		SetProp( hWnd, PROP_WNDPROC_REFCOUNT, (HANDLE)iRefCount );
	}

	return iRefCount;
}


//++ MyUnsubclassWindow
INT_PTR MyUnsubclassWindow(
	__in HWND hWnd
	)
{
	INT_PTR iRefCount = 0;

	WNDPROC fnOldWndProc = (WNDPROC)GetProp( hWnd, PROP_WNDPROC_OLD );
	if ( fnOldWndProc ) {

		// Unsubclass the window if there are no other active timers (check the refcount)
		iRefCount = (INT_PTR)GetProp( hWnd, PROP_WNDPROC_REFCOUNT );
		iRefCount--;
		if ( iRefCount <= 0 ) {
			/// Unsubclass the window
			SetWindowLongPtr( hWnd, GWLP_WNDPROC, (LONG_PTR)fnOldWndProc );
			RemoveProp( hWnd, PROP_WNDPROC_OLD );
			RemoveProp( hWnd, PROP_WNDPROC_REFCOUNT );
		} else {
			/// Decrement the refcount
			SetProp( hWnd, PROP_WNDPROC_REFCOUNT, (HANDLE)iRefCount );
		}
	}

	return iRefCount;
}


/***
*memmove - Copy source buffer to destination buffer
*
*Purpose:
*       memmove() copies a source memory buffer to a destination memory buffer.
*       This routine recognize overlapping buffers to avoid propagation.
*       For cases where propogation is not a problem, memcpy() can be used.
*
*Entry:
*       void *dst = pointer to destination buffer
*       const void *src = pointer to source buffer
*       size_t count = number of bytes to copy
*
*Exit:
*       Returns a pointer to the destination buffer
*
*Exceptions:
*******************************************************************************/
#if (_MSC_VER > 1900)		/// Visual Studio 2017+
#pragma function (memmove)
#endif
void * __cdecl memmove (
        void * dst,
        const void * src,
        size_t count
        )
{
        void * ret = dst;

        if (dst <= src || (char *)dst >= ((char *)src + count)) {
                /*
                 * Non-Overlapping Buffers
                 * copy from lower addresses to higher addresses
                 */
                while (count--) {
                        *(char *)dst = *(char *)src;
                        dst = (char *)dst + 1;
                        src = (char *)src + 1;
                }
        }
        else {
                /*
                 * Overlapping Buffers
                 * copy from higher addresses to lower addresses
                 */
                dst = (char *)dst + count - 1;
                src = (char *)src + count - 1;

                while (count--) {
                        *(char *)dst = *(char *)src;
                        dst = (char *)dst - 1;
                        src = (char *)src - 1;
                }
        }

        return(ret);
}


//++ ExecutePendingFileRenameOperationsImpl
DWORD ExecutePendingFileRenameOperationsImpl(
	__in_opt LPCTSTR pszSrcFileSubstr,		/// Case insensitive. Only SrcFile-s that contain this substring will be processed. Can be empty in which case all pended file operations are executed.
	__out_opt LPDWORD pdwFileOpError,
	__in_opt LPCTSTR pszLogFile				/// If not NULL, the routine will log all file rename operations
	)
{
#define REGKEY_PENDING_FILE_OPS	_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager")
#define REGVAL_PENDING_FILE_OPS _T("PendingFileRenameOperations")

	DWORD err = ERROR_SUCCESS, err2, err3;
	HKEY hKey;
	BYTE iMajorVersion = LOBYTE(LOWORD(GetVersion()));
	BYTE iMinorVersion = HIBYTE(LOWORD(GetVersion()));
	DWORD dwKeyFlags = 0;
	BOOLEAN bDisabledFSRedirection = FALSE;
	HANDLE hLogFile = INVALID_HANDLE_VALUE;

	if ( iMajorVersion > 5 || ( iMajorVersion == 5 && iMinorVersion >= 1 ))		/// XP or newer
		dwKeyFlags |= KEY_WOW64_64KEY;

	if ( pdwFileOpError )
		*pdwFileOpError = ERROR_SUCCESS;

	// Create the log file
	hLogFile = LogCreateFile( pszLogFile, FALSE );
	LogWriteFile( hLogFile, _T("ExecutePendingFileRenameOperations( \"%s\" ) {\r\n"), pszSrcFileSubstr ? pszSrcFileSubstr : _T(""));

	// Read the REG_MULTI_SZ value
	err = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REGKEY_PENDING_FILE_OPS, 0, KEY_READ | KEY_WRITE | dwKeyFlags, &hKey );
	LogWriteFile( hLogFile, _T("\tRegOpenKeyEx( \"HKLM\\%s\" ) == 0x%x\r\n"), REGKEY_PENDING_FILE_OPS, err );
	if ( err == ERROR_SUCCESS ) {
		DWORD dwType, dwSize = 0;
		err = RegQueryValueEx( hKey, REGVAL_PENDING_FILE_OPS, NULL, &dwType, NULL, &dwSize );
		LogWriteFile( hLogFile, _T("\tRegQueryValueEx( \"REGVAL_PENDING_FILE_OPS\", 0 ) == 0x%x, Size == %u\r\n"), err, dwSize );
		if ( err == ERROR_SUCCESS && dwType == REG_MULTI_SZ && dwSize > 0 ) {
			LPTSTR pszValue = (LPTSTR)GlobalAlloc( GMEM_FIXED, dwSize );
			if ( pszValue ) {
				err = RegQueryValueEx( hKey, REGVAL_PENDING_FILE_OPS, NULL, &dwType, (LPBYTE)pszValue, &dwSize );
				LogWriteFile( hLogFile, _T("\tRegQueryValueEx( \"REGVAL_PENDING_FILE_OPS\", %u ) == 0x%x\r\n"), dwSize, err );
				if ( err == ERROR_SUCCESS && dwType == REG_MULTI_SZ && dwSize > 0 ) {

					// PendingFileRenameOperations contains pairs of strings (SrcFile, DstFile)
					// At boot time every SrcFile is renamed to DstFile.
					// If DstFile is empty, SrcFile is deleted.
					// (DstFile might start with "!", not sure why...)

					int i;
					int iLen = (int)(dwSize / sizeof(TCHAR));	/// Registry value length in TCHAR-s
					int iIndexSrcFile, iIndexDstFile, iNextIndex;
					BOOLEAN bValueDirty = FALSE;

					for ( i = 0, iIndexSrcFile = 0, iIndexDstFile = -1; i < iLen; i++ ) {

						if ( pszValue[i] == _T('\0')) {

							if ( iIndexDstFile == -1 ) {

								// At this point we have SrcFile
								iIndexDstFile = i + 1;	/// Remember where DstFile begins

							} else {

								// At this point we have both, SrcFile and DstFile
								// We'll execute the pended operation if SrcFile matches our search criteria

								LPCTSTR pszSrcFile = pszValue + iIndexSrcFile;
								if ( *pszSrcFile &&
									( !pszSrcFileSubstr || !*pszSrcFileSubstr || MyStrFind( pszSrcFile, pszSrcFileSubstr, FALSE ))
									)
								{
									/// Ignore "\??\" prefix
									if ( CompareString( CP_ACP, NORM_IGNORECASE, pszSrcFile, 4, _T("\\??\\"), 4 ) == CSTR_EQUAL )
										pszSrcFile += 4;

									/// Disable file system redirection (Vista+)
									if ( !bDisabledFSRedirection && IsWow64())
										bDisabledFSRedirection = EnableWow64FsRedirection( FALSE );

									if ( !pszValue[iIndexDstFile] ) {

										// TODO: Handle directories!
										// Delete SrcFile
										err3 = ERROR_SUCCESS;
										if ( !DeleteFile( pszSrcFile )) {
											err3 = err2 = GetLastError();
											if ((err2 == ERROR_FILE_NOT_FOUND) || (err2 == ERROR_INVALID_NAME) || (err2 == ERROR_PATH_NOT_FOUND) || (err2 == ERROR_INVALID_DRIVE))
												err2 = ERROR_SUCCESS;	/// Forget errors for files that don't exist
											if ( pdwFileOpError && ( *pdwFileOpError == ERROR_SUCCESS ))	/// Only the first encountered error is remembered
												*pdwFileOpError = err2;
										}
										LogWriteFile( hLogFile, _T("\tDelete( \"%s\" ) == 0x%x\r\n"), pszSrcFile, err3 );
										///DebugString( _T("Delete( \"%s\" ) == 0x%x\n"), pszSrcFile, err3 );

									} else {

										// TODO: Handle directories!
										/// Ignore "!" and "\??\" prefixes
										LPCTSTR pszDstFile = pszValue + iIndexDstFile;
										if ( *pszDstFile == _T('!'))
											pszDstFile++;
										if ( CompareString( CP_ACP, NORM_IGNORECASE, pszDstFile, 4, _T("\\??\\"), 4 ) == CSTR_EQUAL )
											pszDstFile += 4;

										// Rename SrcFile -> DstFile
										err3 = ERROR_SUCCESS;
										if ( !MoveFileEx( pszSrcFile, pszDstFile, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED )) {
											err3 = err2 = GetLastError();
											if ((err2 == ERROR_FILE_NOT_FOUND) || (err2 == ERROR_INVALID_NAME) || (err2 == ERROR_PATH_NOT_FOUND) || (err2 == ERROR_INVALID_DRIVE))
												err2 = ERROR_SUCCESS;	/// Forget errors for files that don't exist
											if ( pdwFileOpError && ( *pdwFileOpError == ERROR_SUCCESS ))	/// Only the first encountered error is remembered
												*pdwFileOpError = err2;
										}
										LogWriteFile( hLogFile, _T("\tRename( \"%s\", \"%s\" ) == 0x%x\r\n"), pszSrcFile, pszDstFile, err3 );
										///DebugString( _T("Rename( \"%s\", \"%s\" ) == 0x%x\n"), pszSrcFile, pszDstFile, err3 );
									}

									// Remove the current pended operation from memory
									iNextIndex = i + 1;
									MoveMemory( pszValue + iIndexSrcFile, pszValue + iNextIndex, (iLen - iNextIndex) * sizeof(TCHAR));
									iLen -= (iNextIndex - iIndexSrcFile);
									dwSize -= (iNextIndex - iIndexSrcFile) * sizeof(TCHAR);
									bValueDirty = TRUE;

									// Next string index
									i = iIndexSrcFile - 1;
									iIndexDstFile = -1;

								} else {
									if ( *pszSrcFile )
										LogWriteFile( hLogFile, _T("\tIgnore( \"%s\" )\r\n"), pszSrcFile );
									iIndexSrcFile = i + 1;
									iIndexDstFile = -1;
								}
							}
						}
					}

					// Write the new PendingFileRenameOperations value.
					// Pended operations that we've executed were removed from the list.
					if ( bValueDirty ) {
						if ( dwSize <= 2 ) {
							err = RegDeleteValue( hKey, REGVAL_PENDING_FILE_OPS );
						} else {
							err = RegSetValueEx( hKey, REGVAL_PENDING_FILE_OPS, 0, REG_MULTI_SZ, (LPBYTE)pszValue, dwSize );
						}
					}
				}

				GlobalFree( pszValue );

			} else {
				err = ERROR_OUTOFMEMORY;
			}
		}
		RegCloseKey( hKey );
	}

	/// Re-enable file system redirection (Vista+)
	if ( bDisabledFSRedirection )
		EnableWow64FsRedirection( TRUE );

	/// Close the log
	LogWriteFile( hLogFile, _T("}\r\n"));
	LogCloseHandle( hLogFile );

	return err;
}


//++ [exported] ExecutePendingFileRenameOperations
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::ExecutePendingFileRenameOperations "SrcFileSubstr" "X:\path\mylog.log"
//    Pop $0 ; Win32 error code
//    Pop $1 ; File operations Win32 error code
//    ${If} $0 = 0
//      ;Success
//    ${Else}
//      ;Error
//    ${EndIf}

void __declspec(dllexport) ExecutePendingFileRenameOperations(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		DWORD err, fileop_err;
		TCHAR szSubstring[255];
		TCHAR szLogFile[MAX_PATH];

		///	Param1: FileSubstring
		szSubstring[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szSubstring, pszBuf );

		///	Param2: LogFile
		szLogFile[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szLogFile, pszBuf );

		// Execute
		err = ExecutePendingFileRenameOperationsImpl( szSubstring, &fileop_err, szLogFile );

		_sntprintf( pszBuf, string_size, _T("%u"), fileop_err );
		pushstring( pszBuf );

		_sntprintf( pszBuf, string_size, _T("%u"), err );
		pushstring( pszBuf );

		/// Free memory
		GlobalFree( pszBuf );
	}
}


//++ FindPendingFileRenameOperationsImpl
DWORD FindPendingFileRenameOperationsImpl(
	__in LPCTSTR pszFileSubstr,		/// Case insensitive
	__out LPTSTR pszFirstFile,
	__in ULONG iFirstFileLen
	)
{
#define REGKEY_PENDING_FILE_OPS	_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager")
#define REGVAL_PENDING_FILE_OPS _T("PendingFileRenameOperations")

	DWORD err = ERROR_SUCCESS;
	HKEY hKey;
	BYTE iMajorVersion = LOBYTE(LOWORD(GetVersion()));
	BYTE iMinorVersion = HIBYTE(LOWORD(GetVersion()));
	DWORD dwKeyFlags = 0;

	if ( !pszFileSubstr || !*pszFileSubstr || !pszFirstFile || !iFirstFileLen )
		return ERROR_INVALID_PARAMETER;

	if ( iMajorVersion > 5 || ( iMajorVersion == 5 && iMinorVersion >= 1 ))		/// XP or newer
		dwKeyFlags |= KEY_WOW64_64KEY;

	// Read the REG_MULTI_SZ value
	err = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REGKEY_PENDING_FILE_OPS, 0, KEY_READ | dwKeyFlags, &hKey );
	if ( err == ERROR_SUCCESS ) {
		DWORD dwType, dwSize = 0;
		err = RegQueryValueEx( hKey, REGVAL_PENDING_FILE_OPS, NULL, &dwType, NULL, &dwSize );
		if ( err == ERROR_SUCCESS && dwType == REG_MULTI_SZ && dwSize > 0 ) {
			LPTSTR pszValue = (LPTSTR)GlobalAlloc( GMEM_FIXED, dwSize );
			if ( pszValue ) {
				err = RegQueryValueEx( hKey, REGVAL_PENDING_FILE_OPS, NULL, &dwType, (LPBYTE)pszValue, &dwSize );
				if ( err == ERROR_SUCCESS && dwType == REG_MULTI_SZ && dwSize > 0 ) {

					// PendingFileRenameOperations contains pairs of strings (SrcFile, DstFile)
					// At boot time every SrcFile is renamed to DstFile.
					// If DstFile is empty, SrcFile is deleted.
					// (DstFile might start with "!", not sure why...)

					int i;
					int iLen = (int)(dwSize / sizeof(TCHAR));	/// Registry value length in TCHAR-s
					int iIndexSrcFile, iIndexDstFile;

					err = ERROR_FILE_NOT_FOUND;		/// Default return value
					for ( i = 0, iIndexSrcFile = 0, iIndexDstFile = -1; i < iLen; i++ ) {

						if ( pszValue[i] == _T('\0')) {

							if ( iIndexDstFile == -1 ) {

								// At this point we have SrcFile
								iIndexDstFile = i + 1;	/// Remember where DstFile begins

							} else {

								// At this point we have both, SrcFile and DstFile
								// We'll test if SrcFile contains our substring

								LPCTSTR pszSrcFile = pszValue + iIndexSrcFile;
								iIndexSrcFile = i + 1;

								/*/// Ignore "\??\" prefix
								if ( CompareString( CP_ACP, NORM_IGNORECASE, pszSrcFile, 4, _T("\\??\\"), 4 ) == CSTR_EQUAL )
									pszSrcFile += 4;*/

								if ( !pszValue[iIndexDstFile] ) {

									if ( MyStrFind( pszSrcFile, pszFileSubstr, FALSE )) {
										lstrcpyn( pszFirstFile, pszSrcFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}

									///DebugString( _T("Delete( \"%s\" )\n"), pszSrcFile );

								} else {

									/// Ignore "!" and "\??\" prefixes
									LPCTSTR pszDstFile = pszValue + iIndexDstFile;
									if ( *pszDstFile == _T('!'))
										pszDstFile++;
									/*if ( CompareString( CP_ACP, NORM_IGNORECASE, pszDstFile, 4, _T("\\??\\"), 4 ) == CSTR_EQUAL )
										pszDstFile += 4;*/

									if ( MyStrFind( pszSrcFile, pszFileSubstr, FALSE )) {
										lstrcpyn( pszFirstFile, pszSrcFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}
									if ( MyStrFind( pszDstFile, pszFileSubstr, FALSE )) {
										lstrcpyn( pszFirstFile, pszDstFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}

									///DebugString( _T("Rename( \"%s\", \"%s\" )\n"), pszSrcFile, pszDstFile );
								}

								// Index
								iIndexDstFile = -1;
							}
						}
					}
				}

				GlobalFree( pszValue );

			} else {
				err = ERROR_OUTOFMEMORY;
			}
		}
		RegCloseKey( hKey );
	}

	return err;
}


//++ [exported] FindPendingFileRenameOperations
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::FindPendingFileRenameOperations "FileSubstr"
//    Pop $0 ; First file path containing FileSubstr, or, an empty string if nothing is found
//    ${If} $0 != ""
//      ;Found
//    ${Else}
//      ;Not found
//    ${EndIf}

void __declspec(dllexport) FindPendingFileRenameOperations(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		DWORD err;
		TCHAR szSubstring[255];

		///	Param1: SrcFileSubstr
		szSubstring[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szSubstring, pszBuf );

		// Execute
		err = FindPendingFileRenameOperationsImpl( szSubstring, pszBuf, string_size );
		if ( err == ERROR_SUCCESS ) {
			pushstring( pszBuf );
		} else {
			pushstring( _T(""));
		}

		/// Free memory
		GlobalFree( pszBuf );
	}
}


//++ ProgressBarWndProc
LRESULT CALLBACK ProgressBarWndProc(
	__in HWND hWnd,
	__in UINT iMsg,
	__in WPARAM wParam,
	__in LPARAM lParam
	)
{
	WNDPROC fnOriginalWndProc = (WNDPROC)GetProp( hWnd, PROP_WNDPROC_OLD );

	switch ( iMsg )
	{
		case PBM_SETPOS:
			{
				// Prevent stepping back?
				if ( GetProp( hWnd, PROP_PROGRESSBAR_NOSTEPBACK ) == (HANDLE)TRUE )
				{
					int iNewPos = (int)wParam;
					int iCurPos = (int)SendMessage( hWnd, PBM_GETPOS, 0, 0 );

					// We don't allow the progress bar to go backward
					// ...except when set to zero
					if (( iNewPos <= iCurPos ) && ( iNewPos > 0 )) {
						return iCurPos;
					}
				}

				// Redirect the message to the second progress bar, if set
				if ( TRUE ) {
					HWND hProgressBar2 = (HWND)GetProp( hWnd, PROP_PROGRESSBAR_REDIRECTWND );
					if ( hProgressBar2 )
						SendMessage( hProgressBar2, iMsg, wParam, lParam );
				}
				break;
			}

		case PBM_SETRANGE:
		case PBM_DELTAPOS:
		case PBM_SETSTEP:
		case PBM_STEPIT:
		case PBM_SETRANGE32:
		case PBM_SETBARCOLOR:
		case PBM_SETBKCOLOR:
		case PBM_SETMARQUEE:
		case PBM_SETSTATE:
			{
				// Redirect the message to the second progress bar, if set
				HWND hProgressBar2 = (HWND)GetProp( hWnd, PROP_PROGRESSBAR_REDIRECTWND );
				if ( hProgressBar2 )
					SendMessage( hProgressBar2, iMsg, wParam, lParam );
				break;
			}

		case WM_DESTROY:
			{
				// Unsubclass this window. Should have been done by the caller...
				while ( MyUnsubclassWindow( hWnd ) > 0 );
				RemoveProp( hWnd, PROP_PROGRESSBAR_NOSTEPBACK );
				RemoveProp( hWnd, PROP_PROGRESSBAR_REDIRECTWND );
				break;
			}
	}

	// Call the original WndProc
	if ( fnOriginalWndProc ) {
		return CallWindowProc( fnOriginalWndProc, hWnd, iMsg, wParam, lParam );
	} else {
		return DefWindowProc( hWnd, iMsg, wParam, lParam );
	}
}


//++ [exported] DisableProgressStepBack
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Progress bar window handle
//+ Output:
//    None
//+ Example:
//    NSutils::DisableProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar

void __declspec(dllexport) DisableProgressStepBack(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// By registering PluginCallback the plugin remains locked in memory
	// Otherwise the framework would unload it when this call returns... Unless the caller specifies /NOUNLOAD...
	extra->RegisterPluginCallback( g_hModule, PluginCallback );

	//	Retrieve NSIS parameters
	///	Param1: Progress bar handle
	hProgressBar = (HWND)popintptr();
	if ( hProgressBar && IsWindow( hProgressBar )) {

		if ( GetProp( hProgressBar, PROP_PROGRESSBAR_NOSTEPBACK ) == NULL ) {	/// Already set?
			if ( MySubclassWindow( hProgressBar, ProgressBarWndProc ) > 0 ) {
				SetProp( hProgressBar, PROP_PROGRESSBAR_NOSTEPBACK, (HANDLE)TRUE );
			}
		}
	}
}


//++ [exported] RestoreProgressStepBack
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Progress bar window handle
//+ Output:
//    None
//+ Example:
//    NSutils::RestoreProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar

void __declspec(dllexport) RestoreProgressStepBack(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	///	Param1: Progress bar handle
	hProgressBar = (HWND)popintptr();
	if ( hProgressBar && IsWindow( hProgressBar )) {

		if ( GetProp( hProgressBar, PROP_PROGRESSBAR_NOSTEPBACK ) != NULL ) {	/// Ever set?
			RemoveProp( hProgressBar, PROP_PROGRESSBAR_NOSTEPBACK );
			MyUnsubclassWindow( hProgressBar );
		}
	}
}


//++ [exported] RedirectProgressBar
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] ProgressBarWnd SecondProgressBarWnd
//+ Output:
//    None
//+ Example:
//    NSutils::RedirectProgressBar /NOUNLOAD $mui.InstFilesPage.ProgressBar $mui.MyProgressBar

void __declspec(dllexport) RedirectProgressBar(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar, hProgressBar2;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// By registering PluginCallback the plugin remains locked in memory
	// Otherwise the framework would unload it when this call returns... Unless the caller specifies /NOUNLOAD...
	extra->RegisterPluginCallback( g_hModule, PluginCallback );

	//	Retrieve NSIS parameters

	///	Param1: Progress bar handle
	hProgressBar = (HWND)popintptr();

	///	Param2: Second progress bar handle. If NULL, the redirection is canceled
	hProgressBar2 = (HWND)popintptr();

	if ( hProgressBar && IsWindow( hProgressBar )) {

		if ( hProgressBar2 ) {

			// Activate progress bar message redirection
			if ( GetProp( hProgressBar, PROP_PROGRESSBAR_REDIRECTWND ) == NULL ) {
				/// New redirection
				if ( MySubclassWindow( hProgressBar, ProgressBarWndProc ) > 1 ) {
					SetProp( hProgressBar, PROP_PROGRESSBAR_REDIRECTWND, hProgressBar2 );
				}
			} else {
				/// Update existing redirection
				SetProp( hProgressBar, PROP_PROGRESSBAR_REDIRECTWND, hProgressBar2 );
			}

			// Clone the characteristics of the first progress bar to the second one
			SetWindowLongPtr( hProgressBar2, GWL_STYLE, GetWindowLongPtr( hProgressBar, GWL_STYLE ));
			SetWindowLongPtr( hProgressBar2, GWL_EXSTYLE, GetWindowLongPtr( hProgressBar, GWL_EXSTYLE ));
			SendMessage(
				hProgressBar2,
				PBM_SETRANGE32,
				SendMessage( hProgressBar, PBM_GETRANGE, TRUE, 0 ),		/// Low limit
				SendMessage( hProgressBar, PBM_GETRANGE, FALSE, 0 )		/// High limit
				);

		} else {

			// Cancel progress bar message redirection
			if ( GetProp( hProgressBar, PROP_PROGRESSBAR_REDIRECTWND ) != NULL ) {
				RemoveProp( hProgressBar, PROP_PROGRESSBAR_REDIRECTWND );
				MyUnsubclassWindow( hProgressBar );
			}
		}
	}
}


//++ MainWndProc
LRESULT CALLBACK MainWndProc(
	__in HWND hWnd,
	__in UINT iMsg,
	__in WPARAM wParam,
	__in LPARAM lParam
	)
{
	WNDPROC fnOriginalWndProc = (WNDPROC)GetProp( hWnd, PROP_WNDPROC_OLD );

	switch ( iMsg )
	{
	case WM_COMMAND:
		{
			if ( HIWORD( wParam ) == BN_CLICKED ) {

				int iNsisCallback = HandleToULong( GetProp( hWnd, PROP_BNCLICKED_CALLBACK ) );
				if ( iNsisCallback != 0 ) {
					pushintptr( LOWORD( wParam ));		/// Button control ID
					pushintptr( lParam );				/// Button HWND
					g_ep->ExecuteCodeSegment( iNsisCallback - 1, 0 );
				}
			}
		}
		break;

	case WM_DESTROY:
		{
			// Unsubclass this window. Should have been done by the caller...
			while ( MyUnsubclassWindow( hWnd ) > 0 );
			RemoveProp( hWnd, PROP_BNCLICKED_CALLBACK );
			break;
		}
	}

	// Call the original WndProc
	if ( fnOriginalWndProc ) {
		return CallWindowProc( fnOriginalWndProc, hWnd, iMsg, wParam, lParam );
	} else {
		return DefWindowProc( hWnd, iMsg, wParam, lParam );
	}
}


//++ TimerWndProc
VOID CALLBACK TimerWndProc( _In_ HWND hWnd, _In_ UINT iMsg, _In_ UINT_PTR iEventID, _In_ DWORD iTime )
{
	if (iMsg == WM_TIMER) {

		// Check timer cache
		int i;
		for (i = 0; i < ARRAYSIZE( g_Timers ); i++) {
			if (g_Timers[i].TimerID == iEventID) {
				// NSIS callback
				DebugString( _T( "[NSutils::%hs] OnTimer( NSIS:%d, ID:%u, Idx:%d/%d )\n" ), __FUNCTION__, g_Timers[i].NsisCallback, (ULONG)g_Timers[i].TimerID, i, ARRAYSIZE(g_Timers) );
				g_ep->ExecuteCodeSegment( g_Timers[i].NsisCallback - 1, 0 );
				break;
			}
		}
		if (i == ARRAYSIZE(g_Timers))
			DebugString( _T( "[NSutils::%hs] WM_TIMER( ID:%u, Time:%ums ) not found in cache\n" ), __FUNCTION__, (ULONG)iEventID, iTime );
	}
}


//++ [exported] StartTimer
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] NsisCallbackFunction (regular NSIS function, no input, no output)
//    [Stack] TimerInterval (in milliseconds, 1000ms = 1s)
//+ Output:
//    None
//+ Example:
//    GetFunctionAddress $0 OnMyTimer
//    NSutils::StartTimer /NOUNLOAD $0 1000

void __declspec(dllexport) StartTimer(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	int iCallback;
	int iPeriod;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// By registering PluginCallback the plugin remains locked in memory
	// Otherwise the framework would unload it when this call returns... Unless the caller specifies /NOUNLOAD...
	extra->RegisterPluginCallback( g_hModule, PluginCallback );

	//	Retrieve NSIS parameters

	///	Param1: Callback function
	iCallback = popint();

	/// Param2: Timer interval (milliseconds)
	iPeriod = popint();

	// SetTimer
	if (( iCallback != 0 ) && ( iPeriod > 0 )) {

		int i, idx;

		// Check timer cache
		for (i = 0, idx = -1; i < ARRAYSIZE( g_Timers ); i++) {

			/// Remember the first vacant cache index
			if (idx == -1 && g_Timers[i].TimerID == 0)
				idx = i;

			if (g_Timers[i].NsisCallback == iCallback) {
				/// Rearm the timer
				g_Timers[i].TimerID = SetTimer( NULL, g_Timers[i].TimerID, iPeriod, TimerWndProc );
				DebugString( _T( "[NSutils::%hs] RearmTimer( NSIS:%d, ID:%u, Idx:%d/%d )\n" ), __FUNCTION__, g_Timers[i].NsisCallback, (ULONG)g_Timers[i].TimerID, i, ARRAYSIZE(g_Timers) );
				break;
			}
		}
		if (i == ARRAYSIZE( g_Timers )) {
			/// New timer
			if (idx >= 0) {
				g_Timers[idx].TimerID = SetTimer( NULL, (g_Timers[idx].NsisCallback = iCallback), iPeriod, TimerWndProc );
				DebugString( _T( "[NSutils::%hs] SetTimer( NSIS:%d, ID:%u, Idx:%d/%d )\n" ), __FUNCTION__, g_Timers[idx].NsisCallback, (ULONG)g_Timers[idx].TimerID, idx, ARRAYSIZE(g_Timers) );
			} else {
				/// Cache full
				DebugString( _T( "[NSutils::%hs] Timer cache is full (Capacity:%d)\n" ), __FUNCTION__, ARRAYSIZE(g_Timers) );
			}
		}
	}
}


//++ [exported] StopTimer
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] NsisCallbackFunction
//+ Output:
//    None
//+ Example:
//    GetFunctionAddress $0 OnMyTimer
//    NSutils::StopTimer $0

void __declspec(dllexport) StopTimer(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	ULONG err;
	int i, iNsisCallback;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters

	///	Param1: Callback function
	iNsisCallback = popint();

	// Check timer cache
	if (iNsisCallback != 0) {
		for (i = 0; i < ARRAYSIZE( g_Timers ); i++) {
			if (g_Timers[i].NsisCallback == iNsisCallback) {
				err = KillTimer( NULL, g_Timers[i].TimerID ) ? ERROR_SUCCESS : GetLastError();
				DebugString( _T( "[NSutils::%hs] KillTimer( NSIS:%d, ID:%u, Idx:%d/%d ) == 0x%x\n" ), __FUNCTION__, g_Timers[i].NsisCallback, (ULONG)g_Timers[i].TimerID, i, ARRAYSIZE(g_Timers), err );
				g_Timers[i].TimerID = 0;
				g_Timers[i].NsisCallback = 0;
				break;
			}
		}
		if (i == ARRAYSIZE( g_Timers ))
			DebugString( _T( "[NSutils::%hs] NSIS callback %d not found in cache\n" ), __FUNCTION__, iNsisCallback );
	}
	UNREFERENCED_PARAMETER( err );
}


//++ [exported] StartReceivingClicks
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Button Parent (Dialog!)
//    [Stack] NsisCallbackFunction (Regular NSIS function. The HWND of the button is passed on the top of the stack)
//+ Output:
//    None
//+ Example:
//    Function OnButtonClick
//       Pop $1 ; Button HWND
//       Pop $2 ; Button control ID
//       MessageBox MB_OK "Button $2 pressed"
//    FunctionEnd
//    ...
//    GetFunctionAddress $0 OnButtonClick
//    NSutils::StartReceivingClicks /NOUNLOAD $HWNDPARENT $0
//    ...
//    GetFunctionAddress $0 OnButtonClick
//    NSutils::StopReceivingClicks /NOUNLOAD $HWNDPARENT $0

void __declspec(dllexport) StartReceivingClicks(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hBtnParentWnd;
	int iCallback;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// By registering PluginCallback the plugin remains locked in memory
	// Otherwise the framework would unload it when this call returns... Unless the caller specifies /NOUNLOAD...
	extra->RegisterPluginCallback( g_hModule, PluginCallback );

	//	Retrieve NSIS parameters

	///	Param1: HWND of the buttons' parent
	hBtnParentWnd = (HWND)popintptr();

	/// Param2: NSIS callback function
	iCallback = popint();

	if ( hBtnParentWnd && IsWindow( hBtnParentWnd ) && ( iCallback != 0 )) {

		if ( GetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK ) != NULL ) {
			/// Already receiving clicks from this window. Just update the callback
			SetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK, ULongToHandle( iCallback ) );
		} else {
			/// Subclass the window and start receiving button clicks
			if ( MySubclassWindow( hBtnParentWnd, MainWndProc ) > 0 ) {
				SetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK, ULongToHandle( iCallback ) );
			}
		}
	}
}

//++ StopReceivingClicks
void __declspec(dllexport) StopReceivingClicks(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hBtnParentWnd;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters

	///	Param1: HWND of the buttons' parent
	hBtnParentWnd = (HWND)popintptr();

	// Stop receiving button clicks
	if ( hBtnParentWnd && IsWindow( hBtnParentWnd )) {
		if ( GetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK ) != NULL ) {
			RemoveProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK );
			MyUnsubclassWindow( hBtnParentWnd );
		}
	}
}


//++ MessageLoopRejectCloseWndProc
LRESULT CALLBACK MessageLoopRejectCloseWndProc( __in int code, __in WPARAM wParam, __in LPARAM lParam )
{
	if ( code >= 0 )
	{
		LPMSG pMsg = (LPMSG)lParam;
		switch ( pMsg->message )
		{
		case WM_CLOSE:
			///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_CLOSE rejected\n"));
			pMsg->message = WM_NULL;
			return 0;

		case WM_QUIT:
			///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_QUIT rejected\n"));
			pMsg->message = WM_NULL;
			return 0;

		case WM_COMMAND:
			switch ( LOWORD(pMsg->wParam))
			{
			case IDCANCEL:
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_COMMAND(IDCANCEL) rejected\n"));
				pMsg->message = WM_NULL;
				return 0;

			case IDYES:
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_COMMAND(IDYES) rejected\n"));
				pMsg->message = WM_NULL;
				return 0;

			case IDNO:
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_COMMAND(IDNO) rejected\n"));
				pMsg->message = WM_NULL;
				return 0;
			}
			break;

		case WM_SYSCOMMAND:
			if ( pMsg->wParam == SC_CLOSE ) {
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_SYSCOMMAND(SC_CLOSE) rejected\n"));
				pMsg->message = WM_NULL;
				return 0;
			}
			break;

		case WM_DESTROY:
			///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] WM_DESTROY rejected\n"));
			pMsg->message = WM_NULL;
			return 0;
		}
	}
	return CallNextHookEx( g_hMessageLoopHook, code, wParam, lParam );
}


//++ [exported] RejectCloseMessages
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] true/false
//+ Output:
//    None
//+ Example:
//    NSutils::RejectCloseMessages /NOUNLOAD true
//    [...]
//    NSutils::RejectCloseMessages false

void __declspec(dllexport) RejectCloseMessages(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// By registering PluginCallback the plugin remains locked in memory
	// Otherwise the framework would unload it when this call returns... Unless the caller specifies /NOUNLOAD...
	extra->RegisterPluginCallback( g_hModule, PluginCallback );

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		DWORD err = ERROR_SUCCESS;
		TCHAR szParam1[255];

		///	Param1: true/false
		szParam1[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szParam1, pszBuf );

		// Execute
		if ( lstrcmpi( szParam1, _T("true")) == 0 ) {

			if ( !g_hMessageLoopHook ) {
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] true\n"));
				g_hMessageLoopHook = SetWindowsHookEx( WH_GETMESSAGE, MessageLoopRejectCloseWndProc, g_hModule, 0 );
				if ( !g_hMessageLoopHook )
					err = GetLastError();
			}

		} else if ( lstrcmpi( szParam1, _T("false")) == 0 ) {

			if ( g_hMessageLoopHook ) {
				///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] false\n"));
				if ( !UnhookWindowsHookEx( g_hMessageLoopHook ))
					err = GetLastError();
				g_hMessageLoopHook = NULL;
			}
		}


		/// Free memory
		GlobalFree( pszBuf );
		UNREFERENCED_PARAMETER( err );
	}
}


//++ [exported] CPUID
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] CPUID Function ID (such as 1, 2, 0x80000001, etc.)
//+ Output:
//    [Stack] EAX
//    [Stack] EBX
//    [Stack] ECX
//    [Stack] EEX
//+ Example:
//    NSutils::CPUID /NOUNLOAD 1
//    Pop $1	; EAX
//    Pop $2	; EBX
//    Pop $3	; ECX
//    Pop $4	; EDX
//    IntOp $0 $4 & 0x4000000	; Check EDX, bit 26
//    ${If} $0 <> 0
//        DetailPrint "CPU supports SSE2"
//    ${EndIf}

void __declspec(dllexport) CPUID(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	INT iFnId;
	INT regs[4];	/// {EAX, EBX, ECX, EDX}

	// Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	// Input
	iFnId = (UINT)popint();

	// CPUID
	if (iFnId < 0x80000000) {
		/// Standard functions
		__cpuid( regs, 0 );
		if (iFnId > 0 && iFnId <= regs[0])
			__cpuid( regs, iFnId );
	} else {
		/// Extended functions
		__cpuid( regs, 0x80000000 );
		if (iFnId > 0x80000000 && iFnId <= regs[0])
			__cpuid( regs, iFnId );
	}

	// Output
	pushint( regs[3] );	/// EDX
	pushint( regs[2] );	/// ECX
	pushint( regs[1] );	/// EBX
	pushint( regs[0] );	/// EAX
}


//++ [exported] CompareFiles
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] File1
//    [Stack] File2
//+ Output:
//    [Stack] TRUE/FALSE

void __declspec(dllexport) CompareFiles(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	BOOL bEqual = FALSE;
	LPTSTR pszFile1;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszFile1 = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszFile1 && (popstring( pszFile1 ) == 0)) {
		LPTSTR pszFile2 = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
		if (pszFile2 && (popstring( pszFile2 ) == 0)) {

			/// Compare file sizes
			WIN32_FILE_ATTRIBUTE_DATA attr1, attr2;
			if (GetFileAttributesEx( pszFile1, GetFileExInfoStandard, &attr1 ) && GetFileAttributesEx( pszFile2, GetFileExInfoStandard, &attr2 )) {
				if (attr1.nFileSizeLow == attr2.nFileSizeLow && attr1.nFileSizeHigh == attr2.nFileSizeHigh) {

					/// Equal file sizes. Compare the content
					HANDLE hFile1 = CreateFile( pszFile1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
					if (hFile1 != INVALID_HANDLE_VALUE) {
						HANDLE hFile2 = CreateFile( pszFile2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
						if (hFile2 != INVALID_HANDLE_VALUE) {

							BYTE buf1[1024], buf2[1024];
							DWORD size1, size2;

							while (TRUE) {
								if (ReadFile( hFile1, buf1, sizeof( buf1 ), &size1, NULL ) && ReadFile( hFile2, buf2, sizeof( buf2 ), &size2, NULL )) {
									if (size1 == size2) {
										if (size1 > 0) {
											/// Compare buffers
											ULONG i;
											for (i = 0; i < size1; i++)
												if (buf1[i] != buf2[i])
													break;
											if (i < size1) {
												break;	/// Different
											}
										} else {
											/// Reached EOF
											bEqual = TRUE;
											break;	/// Equal
										}
									} else {
										break;	/// Different
									}
								}
							}

							CloseHandle( hFile2 );
						}
						CloseHandle( hFile1 );
					}
				} else {
					/// File sizes differ. No need to compare the content
				}
			}
			GlobalFree( pszFile2 );
		}
		GlobalFree( pszFile1 );
	}

	pushint( bEqual );		/// Return value
}


//++ RemoveSoftwareRestrictionPoliciesImpl
HRESULT RemoveSoftwareRestrictionPoliciesImpl(
	__in LPCTSTR pszPathSubstring,
	__in ULONG iOpenKeyFlags,
	__in_opt LPCTSTR pszLogFile,
	__out_opt PULONG piRemovedCount
	)
{
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	BOOL bDirty = FALSE;
	HANDLE hLogFile;

	const IID my_IID_IGroupPolicyObject = { 0xea502723, 0xa23d, 0x11d1, {0xa7, 0xd3, 0x0, 0x0, 0xf8, 0x75, 0x71, 0xe3} };
	const IID my_CLSID_GroupPolicyObject = { 0xea502722, 0xa23d, 0x11d1, {0xa7, 0xd3, 0x0, 0x0, 0xf8, 0x75, 0x71, 0xe3} };

	/// NOTE: COM must be already initialized!
	/// NOTE: CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (!pszPathSubstring || !*pszPathSubstring)
		return E_INVALIDARG;

	if (piRemovedCount)
		*piRemovedCount = 0;

	/// Open the log file (optional)
	hLogFile = LogCreateFile( pszLogFile, FALSE );
	LogWriteFile( hLogFile, _T("RemoveSoftwareRestrictionPolicies( \"%s\" ) {\r\n"), pszPathSubstring );

	iOpenKeyFlags |= KEY_READ;		/// KEY_READ is mandatory

	hr = CoCreateInstance( &my_CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER, &my_IID_IGroupPolicyObject, (LPVOID*)&pLGPO );
	LogWriteFile(hLogFile, _T("\tCoCreateInstance( CLSID_GroupPolicyObject ) == 0x%x\r\n"), hr);
	if (SUCCEEDED(hr)) {
		hr = pLGPO->lpVtbl->OpenLocalMachineGPO(pLGPO, GPO_OPEN_LOAD_REGISTRY);
		LogWriteFile(hLogFile, _T("\tOpenLocalMachineGPO( GPO_OPEN_LOAD_REGISTRY ) == 0x%x\r\n"), hr);
		if (SUCCEEDED(hr)) {
			HKEY hLGPOkey;
			hr = pLGPO->lpVtbl->GetRegistryKey(pLGPO, GPO_SECTION_MACHINE, &hLGPOkey);
			LogWriteFile(hLogFile, _T("\tGetRegistryKey( GPO_SECTION_MACHINE ) == 0x%x\r\n"), hr);
			if (SUCCEEDED(hr)) {
				/// NOTES:
				/// LGPO registry key looks like "HKCU\Software\Microsoft\Windows\CurrentVersion\Group Policy Objects\{GUID}\Machine"
				/// We will enumerate the subkeys "...\SOFTWARE\Policies\Microsoft\Windows\Safer\CodeIdentifiers\*\Paths\*" and remove ones that contain pszPathSubstring in the path
				HKEY hKey1;
				hr = RegOpenKeyEx(hLGPOkey, _T("Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers"), 0, iOpenKeyFlags, &hKey1);
				LogWriteFile(hLogFile, _T("\tRegOpenKeyEx( Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers ) == 0x%x\r\n"), hr);
				if (hr == ERROR_SUCCESS) {
					ULONG i;
					for (i = 0; TRUE; i++) {
						TCHAR szSubkey1[MAX_PATH];
						DWORD dwSubkey1Len = ARRAYSIZE(szSubkey1);
						szSubkey1[0] = 0;
						if (RegEnumKeyEx(hKey1, i, szSubkey1, &dwSubkey1Len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
							HKEY hKey2;
							LogWriteFile(hLogFile, _T("\t[+] %s\r\n"), szSubkey1);
							lstrcat(szSubkey1, _T("\\Paths"));
							if (RegOpenKeyEx(hKey1, szSubkey1, 0, iOpenKeyFlags, &hKey2) == ERROR_SUCCESS) {
								ULONG j;
								for (j = 0; TRUE; ) {
									BOOL bRemoveMe = FALSE;
									TCHAR szSubkey2[MAX_PATH];
									DWORD dwSubkey2Len = ARRAYSIZE(szSubkey2);
									szSubkey2[0] = 0;
									if (RegEnumKeyEx(hKey2, j, szSubkey2, &dwSubkey2Len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
										HKEY hKey3;
										LogWriteFile(hLogFile, _T("\t\t[*] %s\r\n"), szSubkey2);
										if (RegOpenKeyEx(hKey2, szSubkey2, 0, iOpenKeyFlags, &hKey3) == ERROR_SUCCESS) {
											TCHAR szValue[MAX_PATH];
											DWORD dwSize = sizeof(szValue), dwType;
											if (RegQueryValueEx(hKey3, _T("ItemData"), NULL, &dwType, (LPBYTE)szValue, &dwSize) == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
												dwSize /= sizeof(TCHAR);
												szValue[dwSize] = 0;
												LogWriteFile(hLogFile, _T("\t\t\t%s\r\n"), szValue);
												if (MyStrFind( szValue, pszPathSubstring, FALSE )) {
													bRemoveMe = TRUE;
												} else {
													j++;
												}
											}
											RegCloseKey(hKey3);
										}
									} else {
										break;	///for
									}
									if (bRemoveMe) {
										DWORD err = RegDeleteKey(hKey2, szSubkey2);
										LogWriteFile(hLogFile, _T("\t\t\tDeleteRegKeyEx( ) == 0x%x\r\n"), err);
										if (err == ERROR_SUCCESS) {
											if (piRemovedCount)
												(*piRemovedCount)++;
											bDirty = TRUE;
										} else {
											j++;
										}
									}
								}
								RegCloseKey(hKey2);
							}
						} else {
							break;	///for
						}
					}
					RegCloseKey(hKey1);
				}
				RegCloseKey( hLGPOkey );
			}
			if ( bDirty ) {
				GUID ext_guid = REGISTRY_EXTENSION_GUID;
				GUID NSutils_GUID = { 0x3d271cfc, 0x2bc6, 0x4ac2, {0xb6, 0x33, 0x3b, 0xd0, 0xf5, 0xbd, 0xab, 0x2a} };	/// Some random GUID
				hr = pLGPO->lpVtbl->Save(pLGPO, TRUE, TRUE, &ext_guid, &NSutils_GUID);
				LogWriteFile(hLogFile, _T("\tSave( ) == 0x%x\r\n"), hr);
			}
		}
		pLGPO->lpVtbl->Release(pLGPO);
	}

	LogWriteFile(hLogFile, _T("}\r\n"));
	LogCloseHandle(hLogFile);
	return hr;
}


//++ [exported] RemoveSoftwareRestrictionPolicies
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] FileSubstring
//    [Stack] LogFile
//+ Output:
//    [Stack] Win32/HRESULT
//    [Stack] The number of removed policies
//+ Example:
//    NSutils::RemoveSoftwareRestrictionPolicies "MyExecutable.exe" "$EXEDIR\MyLog.txt"
//    Pop $0 ; Win32 error code
//    Pop $1 ; Removed policy count

void __declspec(dllexport) RemoveSoftwareRestrictionPolicies(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		DWORD err;
		TCHAR szSubstring[255];
		TCHAR szLogFile[MAX_PATH];
		ULONG iRemovedCnt = 0;

		///	Param1: FileSubstring
		szSubstring[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szSubstring, pszBuf );

		///	Param2: LogFile
		szLogFile[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szLogFile, pszBuf );

		// Execute
		err = RemoveSoftwareRestrictionPoliciesImpl( szSubstring, KEY_READ|KEY_WRITE|KEY_WOW64_64KEY, szLogFile, &iRemovedCnt );

		_sntprintf( pszBuf, string_size, _T("%u"), iRemovedCnt );
		pushstring( pszBuf );

		_sntprintf( pszBuf, string_size, _T("%u"), err );
		pushstring( pszBuf );

		/// Free memory
		GlobalFree( pszBuf );
	}
}


//++ IsSSD
BOOL IsSSD( _In_ LPCTSTR pszPath )
{
	DWORD err = ERROR_SUCCESS;
	BOOL bTrim = FALSE;

	if (pszPath && (lstrlen( pszPath ) > 1) && (pszPath[1] == _T( ':' ))) {

		TCHAR szDisk[20];
		HANDLE hDisk;

		_sntprintf( szDisk, ARRAYSIZE(szDisk), _T( "\\\\.\\%C:" ), pszPath[0] );
		hDisk = CreateFile( szDisk, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL );
		if (hDisk != INVALID_HANDLE_VALUE) {

			DWORD iBytes;

			// Test whether the drive supports TRIM
			// Some older SSDs might be missed, but it's acceptable for now
			// (Marius)
			{
				STORAGE_PROPERTY_QUERY spq;
				DEVICE_TRIM_DESCRIPTOR dtr;

				ZeroMemory( &spq, sizeof( spq ) );
				ZeroMemory( &dtr, sizeof( dtr ) );
				spq.PropertyId = StorageDeviceTrimProperty;
				spq.QueryType = PropertyStandardQuery;

				if (DeviceIoControl( hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof( spq ), &dtr, sizeof( dtr ), &iBytes, NULL )) {
					bTrim = dtr.TrimEnabled;
				} else {
					err = GetLastError();
				}
			}

			///{
			///	//? https://msdn.microsoft.com/en-us/library/windows/hardware/ff560517(v=vs.85).aspx
			///	//! The disk handle must be opened with GENERIC_READ|GENERIC_WRITE
			///	struct {
			///		SRB_IO_CONTROL ctl;
			///		NVCACHE_REQUEST_BLOCK nrb;
			///		NV_FEATURE_PARAMETER nfp;
			///	} req = {0};
			///	
			///	req.ctl.HeaderLength = sizeof( req.ctl );
			///	CopyMemory( req.ctl.Signature, IOCTL_MINIPORT_SIGNATURE_HYBRDISK, 8 );
			///	///req.ctl.Timeout = 5000;
			///	req.ctl.ControlCode = IOCTL_SCSI_MINIPORT_NVCACHE;
			///	req.ctl.Length = sizeof( req.nrb ) + sizeof( req.nfp );
			///
			///	req.nrb.NRBSize = sizeof( req.nrb );
			///	req.nrb.Function = NRB_FUNCTION_NVCACHE_INFO;
			///	req.nrb.DataBufSize = sizeof( req.nfp );
			///
			///	if (DeviceIoControl( hDisk, IOCTL_SCSI_MINIPORT, &req, sizeof( req ), NULL, 0, &iBytes, NULL )) {
			///		iBytes = iBytes;
			///	} else {
			///		err = GetLastError();
			///	}
			///}

			CloseHandle( hDisk );

		} else {
			err = GetLastError();		/// CreateFile
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return (err == ERROR_SUCCESS) && bTrim;
}


//++ [exported] DriveIsSSD
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] File|Dir
//+ Output:
//    [Stack] TRUE/FALSE
//+ Example:
//    NSutils::DriveIsSSD "$INSTDIR"
//    Pop $0 ; TRUE/FALSE

void __declspec(dllexport) DriveIsSSD(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		BOOL bSSD = FALSE;

		///	Param1: File|Dir
		if (popstring( pszBuf ) == 0) {

			/// Execute
			bSSD = IsSSD( pszBuf );
		}

		pushint( bSSD );

		/// Free memory
		GlobalFree( pszBuf );
	}
}

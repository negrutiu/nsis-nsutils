#include <windows.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include "nsiswapi\pluginapi.h"
#include <intrin.h>


#define CINTERFACE
#include <prsht.h>
#include <GPEdit.h>
#pragma comment (lib, "GPEdit.lib")


// Globals
extern HINSTANCE g_hModule;				/// Defined in main.c
HHOOK g_hMessageLoopHook = NULL;


// UtilsUnload
// Called when the plugin unloads
VOID UtilsUnload()
{
	// If still hooked, unhook the message loop
	if ( g_hMessageLoopHook ) {
		UnhookWindowsHookEx( g_hMessageLoopHook );
		g_hMessageLoopHook = NULL;
		///OutputDebugString( _T("[NSutils::MessageLoopRejectCloseWndProc] auto false\n"));
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
		iLen = wvnsprintf( szStr, (int)ARRAYSIZE(szStr), pszFormat, args );
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

			LARGE_INTEGER iFileSize;
			if ( bOverwrite ||
				 (GetFileSizeEx( hFile, &iFileSize ) && ( iFileSize.QuadPart == 0 )))
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


//++ Subclassing definitions
#define PROP_WNDPROC_OLD				_T("NSutils.WndProc.Old")
#define PROP_WNDPROC_REFCOUNT			_T("NSutils.WndProc.RefCount")
#define PROP_PROGRESSBAR_NOSTEPBACK		_T("NSutils.ProgressBar.NoStepBack")
#define PROP_PROGRESSBAR_REDIRECTWND	_T("NSutils.ProgressBar.RedirectWnd")
#define PROP_BNCLICKED_CALLBACK			_T("NSutils.BN_CLICKED.Callback")
#define PROP_WMTIMER_CALLBACK			_T("NSutils.WM_TIMER.Callback.%d")

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

void * __cdecl memmove (
        void * dst,
        const void * src,
        size_t count
        )
{
        void * ret = dst;

#if defined (_M_X64)

        {


        __declspec(dllimport)


        void RtlMoveMemory( void *, const void *, size_t count );

        RtlMoveMemory( dst, src, count );

        }

#else  /* defined (_M_X64) */
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
#endif  /* defined (_M_X64) */

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
									( !pszSrcFileSubstr || !*pszSrcFileSubstr || StrStrI( pszSrcFile, pszSrcFileSubstr ))
									)
								{
									/// Ignore "\??\" prefix
									if ( StrCmpN( pszSrcFile, _T("\\??\\"), 4 ) == 0 )
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

										/*{
											TCHAR sz[512];
											wsprintf( sz, _T("Delete( \"%s\" ) == 0x%x\n"), pszSrcFile, err3 );
											OutputDebugString( sz );
										}*/

									} else {

										// TODO: Handle directories!
										/// Ignore "!" and "\??\" prefixes
										LPCTSTR pszDstFile = pszValue + iIndexDstFile;
										if ( *pszDstFile == _T('!'))
											pszDstFile++;
										if ( StrCmpN( pszDstFile, _T("\\??\\"), 4 ) == 0 )
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

										/*{
											TCHAR sz[512];
											wsprintf( sz, _T("Rename( \"%s\", \"%s\" ) == 0x%x\n"), pszSrcFile, pszDstFile, err3 );
											OutputDebugString( sz );
										}*/
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


//
//  [exported] ExecutePendingFileRenameOperations
//  ----------------------------------------------------------------------
//  Example:
//    NSutils::ExecutePendingFileRenameOperations "SrcFileSubstr" "X:\path\mylog.log"
//    Pop $0 ; Win32 error code
//    Pop $1 ; File operations Win32 error code
//    ${If} $0 = 0
//      ;Success
//    ${Else}
//      ;Error
//    ${EndIf}
//
void __declspec(dllexport) ExecutePendingFileRenameOperations(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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

		wsprintf( pszBuf, _T("%hu"), fileop_err );
		pushstring( pszBuf );

		wsprintf( pszBuf, _T("%hu"), err );
		pushstring( pszBuf );

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


//
//  [exported] DisableProgressStepBack
//  ----------------------------------------------------------------------
//  Input:  Progress bar window handle
//  Output: None
//
//  Example:
//    NSutils::DisableProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar
//    /NOUNLOAD is mandatory for obvious reasons...
void __declspec(dllexport) DisableProgressStepBack(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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


//
//  [exported] RestoreProgressStepBack
//  ----------------------------------------------------------------------
//  Input:  Progress bar window handle
//  Output: None
//
//  Example:
//    NSutils::RestoreProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar
//
void __declspec(dllexport) RestoreProgressStepBack(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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


//
//  [exported] RedirectProgressBar
//  ----------------------------------------------------------------------
//  Input:  ProgressBarWnd SecondProgressBarWnd
//  Output: None
//
//  Example:
//    NSutils::RedirectProgressBar /NOUNLOAD $mui.InstFilesPage.ProgressBar $mui.MyProgressBar
//
void __declspec(dllexport) RedirectProgressBar(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hProgressBar, hProgressBar2;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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
	case WM_TIMER:
		{
			// We use the NSIS callback also as a timer ID
			// In order to avoid collisions with other (legitimate) WM_TIMER ticks, we have to make sure that this timer ID is actually one of our callbacks
			int iNsisCallback = (int)wParam;

			TCHAR szPropName[128];
			wsprintf( szPropName, PROP_WMTIMER_CALLBACK, iNsisCallback );
			if ( GetProp( hWnd, szPropName )) {

				// Call the NSIS callback
				g_ep->ExecuteCodeSegment( iNsisCallback - 1, 0 );
			}

			break;
		}

	case WM_COMMAND:
		{
			if ( HIWORD( wParam ) == BN_CLICKED ) {

				int iNsisCallback = (int)GetProp( hWnd, PROP_BNCLICKED_CALLBACK );
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

//
//  [exported] StartTimer
//  ----------------------------------------------------------------------
//  Input:  NsisCallbackFunction TimerInterval
//  Output: None
//
//  The NsisCallbackFunction is a regular NSIS function, no input, no output.
//  TimerInterval in milliseconds (1000ms = 1s)
//
//  Example:
//    GetFunctionAddress $0 OnMyTimer
//    NSutils::StartTimer /NOUNLOAD $0 1000
//
void __declspec(dllexport) StartTimer(
	HWND hWndParent,
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

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters

	///	Param1: Callback function
	iCallback = popint();

	/// Param2: Timer interval (milliseconds)
	iPeriod = popint();

	// SetTimer
	if (( iCallback != 0 ) && ( iPeriod > 0 ) && hWndParent && IsWindow( hWndParent )) {

		if ( MySubclassWindow( hWndParent, MainWndProc ) > 0 ) {

			/// Remember the NSIS callback for later. Needed for validations
			TCHAR szPropName[128];
			wsprintf( szPropName, PROP_WMTIMER_CALLBACK, iCallback );
			SetProp( hWndParent, szPropName, (HANDLE)TRUE );

			/// Start the timer
			/// Use the NSIS callback as timer ID
			SetTimer( hWndParent, iCallback, iPeriod, NULL );
		}
	}
}


//
//  [exported] StopTimer
//  ----------------------------------------------------------------------
//  Input:  NsisCallbackFunction
//  Output: None
//
//  Example:
//    GetFunctionAddress $0 OnMyTimer
//    NSutils::StopTimer $0
//
void __declspec(dllexport) StopTimer(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	int iCallback;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters

	///	Param1: Callback function
	iCallback = popint();

	// Kill the timer
	if (( iCallback != 0 ) && hWndParent && IsWindow( hWndParent )) {

		KillTimer( hWndParent, iCallback );
		MyUnsubclassWindow( hWndParent );

		/// Forget this NSIS callback
		if ( TRUE ) {
			TCHAR szPropName[128];
			wsprintf( szPropName, PROP_WMTIMER_CALLBACK, iCallback );
			RemoveProp( hWndParent, szPropName );
		}
	}
}


//
//  [exported] StartReceivingClicks
//  ----------------------------------------------------------------------
//  Input:  ParentWindow NsisCallbackFunction
//  Output: None
//
//  The NsisCallbackFunction is a regular NSIS function. The HWND of the button is passed on the top of the stack.
//  
//
//  Example:
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
//
void __declspec(dllexport) StartReceivingClicks(
	HWND hWndParent,
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

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters

	///	Param1: HWND of the buttons' parent
	hBtnParentWnd = (HWND)popintptr();

	/// Param2: NSIS callback function
	iCallback = popint();

	if ( hBtnParentWnd && IsWindow( hBtnParentWnd ) && ( iCallback != 0 )) {

		if ( GetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK ) != NULL ) {
			/// Already receiving clicks from this window. Just update the callback
			SetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK, (HANDLE)iCallback );
		} else {
			/// Subclass the window and start receiving button clicks
			if ( MySubclassWindow( hBtnParentWnd, MainWndProc ) > 0 ) {
				SetProp( hBtnParentWnd, PROP_BNCLICKED_CALLBACK, (HANDLE)iCallback );
			}
		}
	}
}

//++ StopReceivingClicks
void __declspec(dllexport) StopReceivingClicks(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	HWND hBtnParentWnd;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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
								if ( StrCmpN( pszSrcFile, _T("\\??\\"), 4 ) == 0 )
									pszSrcFile += 4;*/

								if ( !pszValue[iIndexDstFile] ) {

									if ( StrStrI( pszSrcFile, pszFileSubstr ) != NULL ) {
										lstrcpyn( pszFirstFile, pszSrcFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}

									/*{
										TCHAR sz[512];
										wsprintf( sz, _T("Delete( \"%s\" )\n"), pszSrcFile );
										OutputDebugString( sz );
									}*/

								} else {

									/// Ignore "!" and "\??\" prefixes
									LPCTSTR pszDstFile = pszValue + iIndexDstFile;
									if ( *pszDstFile == _T('!'))
										pszDstFile++;
									/*if ( StrCmpN( pszDstFile, _T("\\??\\"), 4 ) == 0 )
										pszDstFile += 4;*/

									if ( StrStrI( pszSrcFile, pszFileSubstr ) != NULL ) {
										lstrcpyn( pszFirstFile, pszSrcFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}
									if ( StrStrI( pszDstFile, pszFileSubstr ) != NULL ) {
										lstrcpyn( pszFirstFile, pszDstFile, iFirstFileLen );
										err = ERROR_SUCCESS;
										break;
									}

									/*{
										TCHAR sz[512];
										wsprintf( sz, _T("Rename( \"%s\", \"%s\" )\n"), pszSrcFile, pszDstFile );
										OutputDebugString( sz );
									}*/
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


//
//  [exported] FindPendingFileRenameOperations
//  ----------------------------------------------------------------------
//  Example:
//    NSutils::FindPendingFileRenameOperations "FileSubstr"
//    Pop $0 ; First file path containing FileSubstr, or, an empty string if nothing is found
//    ${If} $0 != ""
//      ;Found
//    ${Else}
//      ;Not found
//    ${EndIf}
//
void __declspec(dllexport) FindPendingFileRenameOperations(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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


//
//  [exported] RejectCloseMessages
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] true/false
//  Output:
//    None
//  Example:
//    NSutils::RejectCloseMessages /NOUNLOAD true
//    [...]
//    NSutils::RejectCloseMessages false
//
void __declspec(dllexport) RejectCloseMessages(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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
	}
}


//++ DosPathToSystemPath
DWORD DosPathToSystemPath( __inout LPTSTR pszPath, __in DWORD iPathLen )
{
	DWORD err = ERROR_SUCCESS;
	if ( pszPath && iPathLen && *pszPath ) {
		if ( pszPath[1] == _T(':')) {

			TCHAR szSysPath[128];
			TCHAR szLetter[3] = {pszPath[0], pszPath[1], 0};

			ULONG iLen = QueryDosDevice( szLetter, szSysPath, ARRAYSIZE(szSysPath));
			if ( iLen > 0 ) {
				int iOriginalLen = lstrlen(pszPath);
				iLen = lstrlen(szSysPath);	/// Recompute string length. The length returned by QueryDosDevice is not reliable, because it adds multiple trailing NULL terminators
				if ( iOriginalLen - 2 + iLen < iPathLen ) {
					int i;
					for ( i = iOriginalLen; i >= 2; i-- )
						pszPath[i + iLen - 2] = pszPath[i];
					for ( i = 0; szSysPath[i]; i++ )
						pszPath[i] = szSysPath[i];
				} else {
					err = ERROR_INSUFFICIENT_BUFFER;
				}
			} else {
				err = GetLastError();
			}
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ CloseFileHandlesImpl
ULONG CloseFileHandlesImpl(
	__in LPCTSTR pszHandleName,
	__out_opt PULONG piClosedCount
	)
{
	#define NT_SUCCESS(x) ((x) >= 0)
	#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

	#define SystemHandleInformation 16
	#define ObjectBasicInformation 0
	#define ObjectNameInformation 1
	#define ObjectTypeInformation 2

	typedef struct _UNICODE_STRING
	{
		USHORT Length;
		USHORT MaximumLength;
		PWSTR Buffer;
	} UNICODE_STRING, *PUNICODE_STRING;

	typedef struct _SYSTEM_HANDLE
	{
		ULONG ProcessId;
		BYTE ObjectTypeNumber;
		BYTE Flags;
		USHORT Handle;
		PVOID Object;
		ACCESS_MASK GrantedAccess;
	} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

	typedef struct _SYSTEM_HANDLE_INFORMATION
	{
		ULONG HandleCount;
		SYSTEM_HANDLE Handles[1];
	} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

	typedef enum _POOL_TYPE
	{
		NonPagedPool,
		PagedPool,
		NonPagedPoolMustSucceed,
		DontUseThisType,
		NonPagedPoolCacheAligned,
		PagedPoolCacheAligned,
		NonPagedPoolCacheAlignedMustS
	} POOL_TYPE, *PPOOL_TYPE;

	typedef struct _OBJECT_TYPE_INFORMATION
	{
		UNICODE_STRING Name;
		ULONG TotalNumberOfObjects;
		ULONG TotalNumberOfHandles;
		ULONG TotalPagedPoolUsage;
		ULONG TotalNonPagedPoolUsage;
		ULONG TotalNamePoolUsage;
		ULONG TotalHandleTableUsage;
		ULONG HighWaterNumberOfObjects;
		ULONG HighWaterNumberOfHandles;
		ULONG HighWaterPagedPoolUsage;
		ULONG HighWaterNonPagedPoolUsage;
		ULONG HighWaterNamePoolUsage;
		ULONG HighWaterHandleTableUsage;
		ULONG InvalidAttributes;
		GENERIC_MAPPING GenericMapping;
		ULONG ValidAccess;
		BOOLEAN SecurityRequired;
		BOOLEAN MaintainHandleCount;
		USHORT MaintainTypeList;
		POOL_TYPE PoolType;
		ULONG PagedPoolUsage;
		ULONG NonPagedPoolUsage;
	} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

	// ---------------------------------------------------------------------------------- //

	NTSTATUS status = ERROR_SUCCESS;

	POBJECT_TYPE_INFORMATION objectTypeInfo;
	ULONG objectTypeInfoSize = 0x80000;

	PVOID objectNameInfo;
	ULONG objectNameInfoSize = 0x80000;

	PSYSTEM_HANDLE_INFORMATION handleInfo;
	ULONG handleInfoSize = 0x10000;

	ULONG processPID = 0;
	HANDLE processHandle = NULL;

	ULONG i;
	BYTE iFileObjType = 0;		/// Identified dynamically

	LPWSTR pszHandleNameW = NULL;

	typedef NTSTATUS (NTAPI *_NtQuerySystemInformation)( ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength );
	typedef NTSTATUS (NTAPI *_NtDuplicateObject)( HANDLE SourceProcessHandle, HANDLE SourceHandle, HANDLE TargetProcessHandle, PHANDLE TargetHandle, ACCESS_MASK DesiredAccess, ULONG Attributes, ULONG Options );
	typedef NTSTATUS (NTAPI *_NtQueryObject)( HANDLE ObjectHandle, ULONG ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength );

	_NtQuerySystemInformation NtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress( GetModuleHandle(_T("ntdll")), "NtQuerySystemInformation");
	_NtDuplicateObject NtDuplicateObject = (_NtDuplicateObject)GetProcAddress( GetModuleHandle(_T("ntdll")), "NtDuplicateObject");
	_NtQueryObject NtQueryObject = (_NtQueryObject)GetProcAddress( GetModuleHandle(_T("ntdll")), "NtQueryObject");

	// Validate input
	if ( !pszHandleName || !*pszHandleName )
		return ERROR_INVALID_PARAMETER;

	if ( piClosedCount )
		*piClosedCount = 0;

	// UNICODE file name
#if defined (_UNICODE)
	pszHandleNameW = (LPWSTR)pszHandleName;
#else
	pszHandleNameW = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (lstrlen(pszHandleName) + 128) * sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, pszHandleName, -1, pszHandleNameW, HeapSize(GetProcessHeap(), 0, pszHandleNameW) / sizeof(WCHAR));
#endif

	// Allocate working structures
	objectTypeInfo = (POBJECT_TYPE_INFORMATION)HeapAlloc(GetProcessHeap(), 0, objectTypeInfoSize);
	objectNameInfo = HeapAlloc(GetProcessHeap(), 0, objectNameInfoSize);
	handleInfo = (PSYSTEM_HANDLE_INFORMATION)HeapAlloc(GetProcessHeap(), 0, handleInfoSize);
	if ( objectTypeInfo && objectNameInfo && handleInfo )
	{
		/* NtQuerySystemInformation won't give us the correct buffer size, so we guess by doubling the buffer size. */
		while ((status = NtQuerySystemInformation(
			SystemHandleInformation,
			handleInfo,
			handleInfoSize,
			NULL
			)) == STATUS_INFO_LENGTH_MISMATCH)
		{
			handleInfo = (PSYSTEM_HANDLE_INFORMATION)HeapReAlloc(GetProcessHeap(), 0, handleInfo, handleInfoSize *= 2);
		}

		/* NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH. */
		if (NT_SUCCESS(status))
		{
			for (i = 0; i < handleInfo->HandleCount; i++)
			{
				SYSTEM_HANDLE handle = handleInfo->Handles[i];
				HANDLE dupHandle = NULL;
				PUNICODE_STRING pObjectName;
				ULONG returnLength;

				/* Only interested in files */
				if (( iFileObjType != 0 ) && ( handle.ObjectTypeNumber != iFileObjType ))
					continue;

				/* Open the process */
				if ( processPID != handle.ProcessId ) {
					if ( processHandle )
						CloseHandle( processHandle );
					processPID = handle.ProcessId;
					processHandle = OpenProcess( PROCESS_DUP_HANDLE, FALSE, processPID );
					/*printf( "%u. OpenProcess( Pid:%u ) == 0x%x\n", i, processPID, processHandle ? ERROR_SUCCESS : GetLastError());*/
				}
				if ( !processHandle )
					continue;

				/* Duplicate the handle so we can query it. */
				status = NtDuplicateObject( processHandle, (HANDLE)handle.Handle, GetCurrentProcess(), &dupHandle, 0, 0, 0 );
				if (NT_SUCCESS(status)) {

					/* Query the object type. */
					status = NtQueryObject( dupHandle, ObjectTypeInformation, objectTypeInfo, objectTypeInfoSize, NULL );
					if (NT_SUCCESS(status)) {

						/* Dynamically identify FILE object type */
						if ( iFileObjType == 0 ) {
							if (( objectTypeInfo->Name.Length == 8 ) && ( CompareStringW( 0, NORM_IGNORECASE, objectTypeInfo->Name.Buffer, 4, L"File", 4 ) == CSTR_EQUAL ))
								iFileObjType = handle.ObjectTypeNumber;
							else {
								CloseHandle(dupHandle);
								continue;
							}
						}

						/* Query the object name (unless it has an access of 0x0012019f, on which NtQueryObject could hang. */
						if (handle.GrantedAccess != 0x0012019f) {

							status = NtQueryObject( dupHandle, ObjectNameInformation, objectNameInfo, objectNameInfoSize, &returnLength );
							if (NT_SUCCESS(status)) {

								/* Cast our buffer into an UNICODE_STRING. */
								pObjectName = (PUNICODE_STRING)objectNameInfo;
								if (pObjectName->Length)
								{
									/* The object has a name. */
									if ( CompareStringW( 0, NORM_IGNORECASE, pObjectName->Buffer, pObjectName->Length / 2, pszHandleNameW, -1 ) == CSTR_EQUAL )
									{
										HANDLE dupCloseHandle;
										status = NtDuplicateObject(processHandle, (HANDLE)handle.Handle, GetCurrentProcess(), &dupCloseHandle, 0, 0, DUPLICATE_CLOSE_SOURCE);
										if (NT_SUCCESS(status))
											CloseHandle(dupCloseHandle);
										/*printf("NtDuplicateObject( pid:%u, handle:%u, type:%.*S, name:%.*S, DUPLICATE_CLOSE_SOURCE ) == 0x%x\n", handle.ProcessId, (ULONG)handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer, pObjectName->Length / 2, pObjectName->Buffer, status);*/
										if (piClosedCount)
											(*piClosedCount)++;
									}
									/*printf("[%#x, pid:%u] %.*S(%u): %.*S\n", handle.Handle, handle.ProcessId, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer, (ULONG)handle.ObjectTypeNumber, pObjectName->Length / 2, pObjectName->Buffer)*/;

								} else {
									/* Unnamed object */
									/*printf("[%#x] %.*S(%u): (unnamed)\n", handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer, (ULONG)handle.ObjectTypeNumber );*/
								}

							} else {
								/* We have the type name, so just display that. */
								/*printf("[%#x] %.*S: (could not get name) == 0x%x\n", handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer, status );*/
							}
						} else {
							/* We have the type, so display that. */
							/*printf("[%#x] %.*S: (did not get name)\n", handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer );*/
						}
					} else {
						/*printf("[%#x] NtQueryObject = 0x%x\n", handle.Handle, status);*/
					}

					CloseHandle(dupHandle);

				} else {
					/*printf("[%#x] NtDuplicateObject == 0x%x\n", handle.Handle, status);*/
				}
			} /// for

			if (processHandle)
				CloseHandle(processHandle);

			/// Reset the error
			status = ERROR_SUCCESS;

		} else {
			/*printf("NtQuerySystemInformation(...) == 0x%x\n", status);*/
		}
	} else {
		status = ERROR_OUTOFMEMORY;
	}

	if (objectTypeInfo)
		HeapFree(GetProcessHeap(), 0, objectTypeInfo);
	if (objectNameInfo)
		HeapFree(GetProcessHeap(), 0, objectNameInfo);
	if (handleInfo)
		HeapFree(GetProcessHeap(), 0, handleInfo);

#if !defined (_UNICODE)
	HeapFree(GetProcessHeap(), 0, pszHandleNameW);
#endif

	return status;
}


//
//  [exported] CloseFileHandles
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] File/folder path
//  Output:
//    [Stack] Number of closed handles
//  Example:
//    NSutils::CloseFileHandles [/NOUNLOAD] "C:\Windows\System32\drivers\etc\hosts"
//    Pop $0	; The number of closed handles
//
void __declspec(dllexport) CloseFileHandles(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;
	ULONG iClosedCount = 0;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		DWORD err = ERROR_SUCCESS;

		///	Param1: file path
		if ( popstring( pszBuf ) == 0 )
		{
			err = DosPathToSystemPath( pszBuf, string_size );
			err = CloseFileHandlesImpl( pszBuf, &iClosedCount );
		}

		/// Free memory
		GlobalFree( pszBuf );
	}

	/// Return value
	pushintptr( iClosedCount );
}


// [not exported] RegistryParsePath
BOOLEAN RegistryParsePath(
	__in LPCTSTR szFullPath,		/// ex: "HKLM\Software\Microsoft\Whatever"
	__out PHKEY phRoot,				/// Receives HKLM
	__out LPCTSTR *ppszPath			/// Receives "Software\Microsoft\Whatever"
	)
{
	BOOLEAN bRet = TRUE;
	if ( szFullPath && *szFullPath && ppszPath ) {

		*phRoot = NULL;
		*ppszPath = NULL;

		if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKLM\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_LOCAL_MACHINE;
			*ppszPath = szFullPath + 5;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKCU\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_CURRENT_USER;
			*ppszPath = szFullPath + 5;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 4, _T("HKU\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_USERS;
			*ppszPath = szFullPath + 4;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKCR\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_CLASSES_ROOT;
			*ppszPath = szFullPath + 5;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKCC\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_CURRENT_CONFIG;
			*ppszPath = szFullPath + 5;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKDD\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_DYN_DATA;
			*ppszPath = szFullPath + 5;
		} else if (CompareString(0, NORM_IGNORECASE, szFullPath, 5, _T("HKPD\\"), -1) == CSTR_EQUAL) {
			*phRoot = HKEY_PERFORMANCE_DATA;
			*ppszPath = szFullPath + 5;
		} else {
			/// Invalid root key
			bRet = FALSE;
		}

	} else {
		bRet = FALSE;
	}

	return bRet;
}


#define MULTISZ_ACTION_NONE				0
#define MULTISZ_ACTION_INSERT_BEFORE	1
#define MULTISZ_ACTION_INSERT_AFTER		2
typedef int (*MultiSzCallback)(__in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam);

int CallbackMultiSzInsertBefore(__in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam)
{
	LPCTSTR pszInsertAfter = (LPCTSTR)pParam;
	if (pszInsertAfter && *pszInsertAfter && CompareString(0, NORM_IGNORECASE, pszSubstr, -1, pszInsertAfter, -1) == CSTR_EQUAL)
		return MULTISZ_ACTION_INSERT_BEFORE;
	return MULTISZ_ACTION_NONE;
}

int CallbackMultiSzInsertAfter(__in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam)
{
	LPCTSTR pszInsertAfter = (LPCTSTR)pParam;
	if (pszInsertAfter && *pszInsertAfter && CompareString(0, NORM_IGNORECASE, pszSubstr, -1, pszInsertAfter, -1) == CSTR_EQUAL)
		return MULTISZ_ACTION_INSERT_AFTER;
	return MULTISZ_ACTION_NONE;
}

int CallbackMultiSzInsertAtIndex(__in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam)
{
	int iInsertAtIndex = (int)pParam;
	if (iSubstrIndex == iInsertAtIndex)
		return MULTISZ_ACTION_INSERT_BEFORE;
	return MULTISZ_ACTION_NONE;
}

// [not exported] RegMultiSzInsertImpl
DWORD RegMultiSzInsertImpl(
	__in LPCTSTR szRegKey,
	__in_opt LPCTSTR szRegValue,
	__in_opt DWORD dwKeyOpenFlags,
	__in LPCTSTR szInsert,
	__in_opt MultiSzCallback fnInsertCallback,
	__in_opt PVOID pInsertParam
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( szRegKey && *szRegKey && szInsert && *szInsert ) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if ( RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_WRITE|KEY_READ|dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				int iAddLen = lstrlen(szInsert);
				DWORD iType, iOldLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iOldLen );
				if ( err == ERROR_SUCCESS && iType == REG_MULTI_SZ && iOldLen > 2 ) {

					// Existing registry value
					int iNewLen = iOldLen/sizeof(TCHAR) + iAddLen + 1;
					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iNewLen * sizeof(TCHAR));
					if ( psz ) {
						iOldLen = iNewLen * sizeof(TCHAR);
						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iOldLen );
						if (err == ERROR_SUCCESS) {

							BOOLEAN bFound;
							TCHAR *p;

							iOldLen /= sizeof(TCHAR);	/// Bytes -> TCHAR

							/// Enumerate all substrings and decide where to insert
							if (fnInsertCallback) {
								int i, iAction;
								for ( p = psz, bFound = FALSE, i = 0; (*p && !bFound); i++ ) {
									iAction = fnInsertCallback( i, p, pInsertParam );
									switch (iAction) {
									case MULTISZ_ACTION_NONE:
										for ( ; *p; p++);	/// Skip current substring
										p++;				/// Skip NULL terminator
										break;
									case MULTISZ_ACTION_INSERT_BEFORE:
										bFound = TRUE;
										break;
									case MULTISZ_ACTION_INSERT_AFTER:
										bFound = TRUE;
										for ( ; *p; p++);	/// Skip current substring
										p++;				/// Skip NULL terminator
										break;
									default:;
										///assert( !"Invalid action" );
									}
								}
							} else {
								p = psz + iOldLen - 1;	/// End of string
							}

							/// Insert substring
							if (TRUE) {
								int iMoveLen = iOldLen - (int)(p - psz);
								memmove( p + iAddLen + 1, p, iMoveLen * sizeof(TCHAR));
								lstrcpy( p, szInsert );
							}
							err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof(TCHAR));
						}
						HeapFree( GetProcessHeap(), 0, psz );

					} else {
						err = ERROR_OUTOFMEMORY;
					}

				} else {

					// New registry value
					int iNewLen = iAddLen + 2;
					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iNewLen * sizeof(TCHAR));
					if ( psz ) {
						lstrcpy( psz, szInsert );
						psz[iNewLen - 1] = _T('\0');
						psz[iNewLen - 2] = _T('\0');
						err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof(TCHAR));
						HeapFree( GetProcessHeap(), 0, psz );
					} else {
						err = ERROR_OUTOFMEMORY;
					}
				}
				RegCloseKey( hKey );
			}
		} else {
			err = ERROR_PATH_NOT_FOUND;		/// RegistryParsePath
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


// [not exported] RegMultiSzDeleteImpl
DWORD RegMultiSzDeleteImpl(
	__in LPCTSTR szRegKey,
	__in_opt LPCTSTR szRegValue,
	__in_opt DWORD dwKeyOpenFlags,
	__in LPCTSTR szDelete,
	__in BOOL bRemoveEmptyValue
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( szRegKey && *szRegKey && szDelete && *szDelete ) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if ( RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_WRITE|KEY_READ|dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				DWORD iType, iOldLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iOldLen );
				if ( err == ERROR_SUCCESS && iType == REG_MULTI_SZ ) {

					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iOldLen * sizeof(TCHAR));
					if ( psz ) {

						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iOldLen );
						if (err == ERROR_SUCCESS) {

							TCHAR *p;
							DWORD iNewLen;

							iOldLen /= sizeof(TCHAR);	/// Bytes -> TCHAR
							iNewLen = iOldLen;

							/// Enumerate all substrings
							for ( p = psz; *p; ) {
								if (CompareString( 0, NORM_IGNORECASE, p, -1, szDelete, -1 ) == CSTR_EQUAL) {
									/// Delete substring
									int iDeleteLen = lstrlen(p) + 1;
									int iMoveLen = iOldLen - iDeleteLen - (int)(p - psz);
									memmove( p, p + iDeleteLen, iMoveLen * sizeof(TCHAR));
									iNewLen -= iDeleteLen;
								} else {
									for ( ; *p; p++);	/// Skip current substring
									p++;				/// Skip NULL terminator
								}
							}

							/// Set the registry value
							if (iNewLen != iOldLen) {
								if (!bRemoveEmptyValue || (iNewLen > 2)) {
									err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof(TCHAR));
								} else {
									err = RegDeleteValue( hKey, szRegValue );
								}
							}
						}
						HeapFree( GetProcessHeap(), 0, psz );

					} else {
						err = ERROR_OUTOFMEMORY;
					}
				}
				RegCloseKey( hKey );
			}
		} else {
			err = ERROR_PATH_NOT_FOUND;		/// RegistryParsePath
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


// [not exported] RegMultiSzReadImpl
DWORD RegMultiSzReadImpl(
	__in LPCTSTR szRegKey,
	__in_opt LPCTSTR szRegValue,
	__in_opt DWORD dwKeyOpenFlags,
	__in int iIndex,
	__out_opt LPTSTR pszOutString,
	__in_opt ULONG iOutStringLen
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( szRegKey && *szRegKey ) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if ( RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_READ|dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				DWORD iType, iLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iLen );
				if ( err == ERROR_SUCCESS && iType == REG_MULTI_SZ ) {

					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iLen * sizeof(TCHAR));
					if ( psz ) {
						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iLen );
						if (err == ERROR_SUCCESS) {

							TCHAR *p;
							int i;

							iLen /= sizeof(TCHAR);	/// Bytes -> TCHAR

							err = ERROR_INVALID_INDEX;
							for ( p = psz, i = 0; *p; i++ ) {
								if ( i == iIndex )
									if (pszOutString && iOutStringLen) {
										lstrcpyn( pszOutString, p, iOutStringLen );
										err = ERROR_SUCCESS;
										break;
									}
									for ( ; *p; p++);	/// Skip current substring
									p++;				/// Skip NULL terminator
							}
						}
						HeapFree( GetProcessHeap(), 0, psz );

					} else {
						err = ERROR_OUTOFMEMORY;
					}
				} else {
					if (iType != REG_MULTI_SZ)
						err = ERROR_DATATYPE_MISMATCH;
				}
				RegCloseKey( hKey );
			}
		} else {
			err = ERROR_PATH_NOT_FOUND;		/// RegistryParsePath
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//
//  [exported] RegMultiSzInsertAfter
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Substring to insert after (optional)
//  Output:
//    [Stack] Win32 error
//  Example:
//    NSutils::RegMultiSzInsertAfter /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" "Line 3"
//    Pop $0	; Win32 error
//
void __declspec(dllexport) RegMultiSzInsertAfter(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255], szInsertAfter[255];
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE(szRegKey));
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE(szRegValue));
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE(szInsert));
					/// Param5: Substring to insert after
					if (popstring( pszBuf ) == 0) {
						lstrcpyn( szInsertAfter, pszBuf, ARRAYSIZE(szInsertAfter));

						// Insert substring
						err = RegMultiSzInsertImpl( szRegKey, szRegValue, dwFlags, szInsert, CallbackMultiSzInsertAfter, szInsertAfter );
					}
				}
			}
		}

		/// Free memory
		GlobalFree( pszBuf );

	} else {
		err = ERROR_OUTOFMEMORY;
	}

	/// Return value
	pushintptr( err );
}


//
//  [exported] RegMultiSzInsertBefore
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Substring to insert before (optional)
//  Output:
//    [Stack] Win32 error
//  Example:
//    NSutils::RegMultiSzInsertBefore /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" "Line 5"
//    Pop $0	; Win32 error
//
void __declspec(dllexport) RegMultiSzInsertBefore(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255], szInsertBefore[255];
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE(szRegKey));
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE(szRegValue));
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE(szInsert));
					/// Param5: Substring to insert before
					if (popstring( pszBuf ) == 0) {
						lstrcpyn( szInsertBefore, pszBuf, ARRAYSIZE(szInsertBefore));

						// Insert substring
						err = RegMultiSzInsertImpl( szRegKey, szRegValue, dwFlags, szInsert, CallbackMultiSzInsertBefore, szInsertBefore );
					}
				}
			}
		}

		/// Free memory
		GlobalFree( pszBuf );

	} else {
		err = ERROR_OUTOFMEMORY;
	}

	/// Return value
	pushintptr( err );
}


//
//  [exported] RegMultiSzInsertAtIndex
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] Registry key (ex: "HKLM\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Zero based index (optional)
//  Output:
//    [Stack] Win32 error
//  Example:
//    NSutils::RegMultiSzInsertBefore [/NOUNLOAD] "HKLM\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" "Line 5"
//    Pop $0	; Win32 error
//
void __declspec(dllexport) RegMultiSzInsertAtIndex(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255];
		int iIndex;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE(szRegKey));
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE(szRegValue));
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE(szInsert));
					/// Param5: Index (zero based)
					iIndex = popint();

					// Insert substring
					err = RegMultiSzInsertImpl( szRegKey, szRegValue, dwFlags, szInsert, CallbackMultiSzInsertAtIndex, (PVOID)iIndex );
				}
			}
		}

		/// Free memory
		GlobalFree( pszBuf );

	} else {
		err = ERROR_OUTOFMEMORY;
	}

	/// Return value
	pushintptr( err );
}


//
//  [exported] RegMultiSzDelete
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to delete
//    [Stack] Remove the registry value if it becomes empty (BOOL)
//  Output:
//    [Stack] Win32 error
//  Example:
//    NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" ${TRUE}
//    Pop $0	; Win32 error
//
void __declspec(dllexport) RegMultiSzDelete(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR pszBuf = NULL;

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szKey[512], szValue[128], szDelStr[255];
		BOOL bRemoveEmptyValue;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szKey, pszBuf, ARRAYSIZE(szKey));
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szValue, pszBuf, ARRAYSIZE(szValue));
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to delete
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szDelStr, pszBuf, ARRAYSIZE(szDelStr));
					/// Param5: Remove empty value (BOOL)
					bRemoveEmptyValue = (BOOL)popint();

					// Delete substring(s)
					err = RegMultiSzDeleteImpl( szKey, szValue, dwFlags, szDelStr, bRemoveEmptyValue );
				}
			}
		}

		/// Free memory
		GlobalFree( pszBuf );

	} else {
		err = ERROR_OUTOFMEMORY;
	}

	/// Return value
	pushintptr( err );
}


//
//  [exported] RegMultiSzRead
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Index (substring to read)
//  Output:
//    [Stack] Win32 error
//    [Stack] The substring at specified index, or an empty string
//  Example:
//    NSutils::RegMultiSzRead /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} 2
//    Pop $0	; Win32 error
//    Pop $1	; Substring at index 2
//
void __declspec(dllexport) RegMultiSzRead(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR pszBuf = NULL;
	TCHAR szSubstr[512];

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	// Init
	szSubstr[0] = 0;

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szKey[512], szValue[128];
		int iIndex;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szKey, pszBuf, ARRAYSIZE(szKey));
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szValue, pszBuf, ARRAYSIZE(szValue));
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring index
				iIndex = popint();

				// Read substring
				err = RegMultiSzReadImpl( szKey, szValue, dwFlags, iIndex, szSubstr, ARRAYSIZE(szSubstr));
			}
		}

		/// Free memory
		GlobalFree( pszBuf );

	} else {
		err = ERROR_OUTOFMEMORY;
	}

	/// Return value
	pushstring( szSubstr );
	pushintptr( err );
}


//
//  [exported] CPUID
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] CPUID Function ID (such as 1, 2, 0x80000001, etc.)
//  Output:
//    [Stack] EAX
//    [Stack] EBX
//    [Stack] ECX
//    [Stack] EEX
//  Example:
//    NSutils::CPUID /NOUNLOAD 1
//    Pop $1	; EAX
//    Pop $2	; EBX
//    Pop $3	; ECX
//    Pop $4	; EDX
//    IntOp $0 $4 & 0x4000000	; Check EDX, bit 26
//    ${If} $0 <> 0
//        DetailPrint "CPU supports SSE2"
//    ${EndIf}
//
void __declspec(dllexport) CPUID(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	UINT iFnId;
	UINT regs[4];	/// {EAX, EBX, ECX, EDX}

	// Cache global structures
	EXDLL_INIT();

	// Check NSIS API compatibility
	if (!IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	// Input
	iFnId = (UINT)popint();

	// CPUID
	if (iFnId < 0x80000000) {
		/// Standard functions
		__cpuid( regs, 0 );
		if (iFnId > 0 && iFnId <= regs[0])
			__cpuidex( regs, iFnId, 0 );
	} else {
		/// Extended functions
		__cpuid( regs, 0x80000000 );
		if (iFnId > 0x80000000 && iFnId <= regs[0])
			__cpuidex( regs, iFnId, 0 );
	}

	// Output
	pushint( regs[3] );	/// EDX
	pushint( regs[2] );	/// ECX
	pushint( regs[1] );	/// EBX
	pushint( regs[0] );	/// EAX
}


//
//  [exported] CompareFiles
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] File1
//    [Stack] File2
//  Output:
//    [Stack] TRUE/FALSE
//
void __declspec(dllexport) CompareFiles(
	HWND hWndParent,
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

	//	Check NSIS API compatibility
	if (!IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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
							StrCat(szSubkey1, _T("\\Paths"));
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
												if (StrStrI( szValue, pszPathSubstring )) {
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
										DWORD err = RegDeleteKeyEx(hKey2, szSubkey2, 0, 0);
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


//
//  [exported] RemoveSoftwareRestrictionPolicies
//  ----------------------------------------------------------------------
//  Input:
//    [Stack] FileSubstring
//    [Stack] LogFile
//  Output:
//    [Stack] Win32/HRESULT
//    [Stack] The number of removed policies
//  Example:
//    NSutils::RemoveSoftwareRestrictionPolicies "MyExecutable.exe" "$EXEDIR\MyLog.txt"
//    Pop $0 ; Win32 error code
//    Pop $1 ; Removed policy count
//
void __declspec(dllexport) RemoveSoftwareRestrictionPolicies(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

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

		wsprintf( pszBuf, _T("%hu"), iRemovedCnt );
		pushstring( pszBuf );

		wsprintf( pszBuf, _T("%hu"), err );
		pushstring( pszBuf );

		/// Free memory
		GlobalFree( pszBuf );
	}
}
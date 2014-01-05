#include <windows.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include "nsiswapi\pluginapi.h"


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

		///	Param1: SrcFileSubstr
		szSubstring[0] = 0;
		if ( popstring( pszBuf ) == 0 )
			lstrcpy( szSubstring, pszBuf );

		///	Param2: SrcFileSubstr
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

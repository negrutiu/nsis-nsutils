
#include <windows.h>
#include "nsiswapi\pluginapi.h"

//++ FindFirstStringFileInfo
BOOL FindFirstStringFileInfo(
	__inout WORD **pp,
	__in LPWSTR pszParentKeyW,
	__out LPWSTR *ppszChildKeyW
	)
{
	#define P (*pp)
	LPWORD pBlock = P;
	WORD iBlockSize = *P;

	WORD iLength, iValueLength, iType;
	LPWSTR pszKeyW;
	LPBYTE pValueBin;
	BOOL bHasChildren;

	iLength = *P++;
	if ( iLength > 0 ) {

		iValueLength = *P++;
		iType = *P++;		/// 0 == binary, 1 == text

		pszKeyW = (LPWSTR)P;
		for ( ; *P != UNICODE_NULL; P++ ); P++;

		(LPBYTE)P += ((LPBYTE)P - (LPBYTE)pBlock) % 4;		/// DWORD padding

		if ( iValueLength > 0 ) {

			pValueBin = (LPBYTE)P;
			if ( iType == 1 ) {
				for ( ; *P != UNICODE_NULL; P++ ); P++;		/// Note: iValueLength is not quite reliable. In some executables (most of them) iValueLength means WCHAR-s, while in others (ResHacker.exe) it means bytes.
			} else {
				(LPBYTE)P += iValueLength;
			}

			(LPBYTE)P += ((LPBYTE)P - (LPBYTE)pBlock) % 4;	/// DWORD padding
		}

		// Find the first child of "StringFileInfo" block
		if ( pszParentKeyW && *pszParentKeyW && lstrcmpW( pszParentKeyW, L"StringFileInfo" ) == 0 ) {
			*ppszChildKeyW = pszKeyW;
			return TRUE;
		}

		// Extend the search to children blocks
		bHasChildren = (
			iValueLength == 0
			|| lstrcmpW( pszKeyW, L"VS_VERSION_INFO" ) == 0
			|| lstrcmpW( pszKeyW, L"StringFileInfo" ) == 0
			|| lstrcmpW( pszKeyW, L"VarFileInfo" ) == 0
			);

		if ( bHasChildren ) {
			while ((LPBYTE)P < (LPBYTE)pBlock + iBlockSize ) {
				if ( FindFirstStringFileInfo( pp, pszKeyW, ppszChildKeyW ))
					return TRUE;
			}
		}
	}
	return FALSE;
}


//++ ReadVersionInfoString
DWORD ReadVersionInfoString(
	__in_opt LPCTSTR szFile,
	__in LPCTSTR szStringName,
	__out LPTSTR szStringValue,
	__in UINT iStringValueLen	/// in TCHAR-s
	)
{
	DWORD err = ERROR_SUCCESS;

	// Validate parameters
	if ( szFile && *szFile && szStringName && *szStringName && szStringValue && ( iStringValueLen > 0 )) {

		DWORD dwVerInfoSize;
		*szStringValue = _T('\0');

		// Read the version information to memory
		dwVerInfoSize = GetFileVersionInfoSize( szFile, NULL );
		if ( dwVerInfoSize > 0 ) {

			HANDLE hMem = GlobalAlloc( GMEM_MOVEABLE, dwVerInfoSize );
			if ( hMem ) {

				LPBYTE pMem = GlobalLock( hMem );
				if ( pMem ) {

					if ( GetFileVersionInfo( szFile, 0, dwVerInfoSize, pMem )) {

						typedef struct _LANGANDCODEPAGE {
							WORD wLanguage;
							WORD wCodePage;
						} LANGANDCODEPAGE;

						LANGANDCODEPAGE *pCodePage;
						UINT iCodePageSize = sizeof( *pCodePage );

						// Determine the language of the version info
						if ( VerQueryValue( pMem, _T("\\VarFileInfo\\Translation"), (LPVOID*)&pCodePage, &iCodePageSize )) {

							TCHAR szTemp[255];
							LPCTSTR szValue = NULL;
							UINT iValueLen = 0;
							BOOL bFound;

							// Read the specified version info string
							wsprintf( szTemp, _T("\\StringFileInfo\\%04x%04x\\%s"), pCodePage->wLanguage, pCodePage->wCodePage, szStringName );
							bFound = VerQueryValue( pMem, szTemp, (LPVOID*)&szValue, &iValueLen );
							if ( !bFound ) {

								/// There might be executables where the "StringFileInfo" sub-block doesn't match the language specified by "Translation"
								/// In such case, we'll manually look for the first "StringFileInfo" translation.
								/// (Reproduced with a FunctionList.dll modified with XNResourceEditor)
								LPWORD p = (LPWORD)pMem;
								LPWSTR pszTranslation = NULL;
								bFound = FindFirstStringFileInfo( &p, NULL, &pszTranslation );
								if ( bFound ) {
									wsprintf( szTemp, _T("\\StringFileInfo\\%ws\\%s"), pszTranslation, szStringName );
									bFound = VerQueryValue( pMem, szTemp, (LPVOID*)&szValue, &iValueLen );
								}
							}

							// Prepare the output
							if ( szValue && *szValue ) {
								lstrcpyn( szStringValue, szValue, iStringValueLen );
								if ( iValueLen > iStringValueLen ) {
									// The output buffer is not large enough
									// We'll return the truncated string, and ERROR_BUFFER_OVERFLOW error code
									err = ERROR_BUFFER_OVERFLOW;
								}

							} else {
								err = ERROR_NOT_FOUND;
							}
						} else {
							err = ERROR_NOT_FOUND;
						}
					} else {
						err = GetLastError();
					}

#if defined (_DEBUG)
					ZeroMemory( pMem, dwVerInfoSize );
					pMem = NULL;
#endif
					GlobalUnlock( hMem );

				} else {
					err = GetLastError();
				}

				GlobalFree( hMem );

			} else {
				err = GetLastError();
			}
		} else {
			err = GetLastError();
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ ReadFixedFileInfo
DWORD ReadFixedFileInfo(
	__in_opt LPCTSTR szFile,
	__out VS_FIXEDFILEINFO *pFfi
	)
{
	DWORD err = ERROR_SUCCESS;

	// Validate parameters
	if ( pFfi && szFile && *szFile ) {

		DWORD dwVerInfoSize;

		// Read the version information to memory
		dwVerInfoSize = GetFileVersionInfoSize( szFile, NULL );
		if ( dwVerInfoSize > 0 ) {

			HANDLE hMem = GlobalAlloc( GMEM_MOVEABLE, dwVerInfoSize );
			if ( hMem ) {

				LPBYTE pMem = GlobalLock( hMem );
				if ( pMem ) {

					if ( GetFileVersionInfo( szFile, 0, dwVerInfoSize, pMem )) {

						// Query
						VS_FIXEDFILEINFO *pMemFfi = NULL;
						UINT iSize = sizeof( pMemFfi );	/// size of pointer!
						if ( VerQueryValue( pMem, _T("\\"), (LPVOID*)&pMemFfi, &iSize )) {

							// Copy the data to the output buffer
							UINT i;
							for ( i = 0; i < iSize; i++ )
								((LPBYTE)pFfi)[i] = ((LPBYTE)pMemFfi)[i];

						} else {
							err = ERROR_NOT_FOUND;
						}
					} else {
						err = GetLastError();
					}

#if defined (_DEBUG)
					ZeroMemory( pMem, dwVerInfoSize );
					pMem = NULL;
#endif
					GlobalUnlock( hMem );

				} else {
					err = GetLastError();
				}

				GlobalFree( hMem );

			} else {
				err = GetLastError();
			}
		} else {
			err = GetLastError();
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ [exported] GetVersionInfoString
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::GetVersionInfoString "$INSTDIR\Test.exe" "LegalCopyright"
//    Pop $0
//    ${If} $0 != ""
//      ;Success: $0 contains the valid version string
//    ${Else}
//      ;Error
//    ${EndIf}

void __declspec(dllexport) GetVersionInfoString(
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
	/// Allocate memory large enough to store an NSIS string
    pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		TCHAR szPath[MAX_PATH];
		TCHAR szString[MAX_PATH];
		DWORD err;

		///	Param1: Executable file path
		*szPath = 0;
		if ( popstring( pszBuf ) == 0 ) {
			lstrcpyn( szPath, pszBuf, ARRAYSIZE( szPath ));
		}
		///	Param2: String name
		if ( popstring( pszBuf ) == 0 ) {
			lstrcpyn( szString, pszBuf, ARRAYSIZE( szString ));
		}

		// Return the string (on the stack)
		err = ReadVersionInfoString( szPath, szString, pszBuf, string_size );
		if ( err == ERROR_SUCCESS ) {
			pushstring( pszBuf );
		} else {
			pushstring( _T(""));
		}

		/// Free memory
		GlobalFree( pszBuf );
	}
}


//++ [exported] GetFileVersion
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::GetFileVersion "$SYSDIR\Notepad.exe"
//    Pop $0
//    ${If} $0 != ""
//      ; Success:
//      ;  - $0 contains a pre-formatted version string like "6.1.7600.16385"
//      ;  - $1, $2, $3, $4 contain each version component (in our case: 6, 1, 7600, 16385)
//    ${Else}
//      ;Error
//    ${EndIf}

void __declspec(dllexport) GetFileVersion(
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

		///	Param1: Executable file path
		if ( popstring( pszBuf ) == 0 ) {

			DWORD err;
			VS_FIXEDFILEINFO ffi;

			err = ReadFixedFileInfo( pszBuf, &ffi );
			if ( err == ERROR_SUCCESS && ffi.dwSignature == VS_FFI_SIGNATURE ) {

				USHORT v1 = HIWORD( ffi.dwFileVersionMS );
				USHORT v2 = LOWORD( ffi.dwFileVersionMS );
				USHORT v3 = HIWORD( ffi.dwFileVersionLS );
				USHORT v4 = LOWORD( ffi.dwFileVersionLS );

				wsprintf( pszBuf, _T("%hu"), v1 );
				setuservariable( INST_1, pszBuf );	/// $1 = v1

				wsprintf( pszBuf, _T("%hu"), v2 );
				setuservariable( INST_2, pszBuf );	/// $2 = v2

				wsprintf( pszBuf, _T("%hu"), v3 );
				setuservariable( INST_3, pszBuf );	/// $3 = v3

				wsprintf( pszBuf, _T("%hu"), v4 );
				setuservariable( INST_4, pszBuf );	/// $4 = v4

				wsprintf( pszBuf, _T("%hu.%hu.%hu.%hu"), v1, v2, v3, v4 );
				pushstring( pszBuf );

			} else {
				pushstring( _T(""));
			}
		}

		/// Free memory
		GlobalFree( pszBuf );
	}
}


//++ [exported] GetProductVersion
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::GetFileVersion "$SYSDIR\Notepad.exe"
//    Pop $0
//    ${If} $0 != ""
//      ; Success:
//      ;  - $0 contains a pre-formatted version string like "6.1.7600.16385"
//      ;  - $1, $2, $3, $4 contain each version component (in our case: 6, 1, 7600, 16385)
//    ${Else}
//      ;Error
//    ${EndIf}

void __declspec(dllexport) GetProductVersion(
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

		///	Param1: Executable file path
		if ( popstring( pszBuf ) == 0 ) {

			DWORD err;
			VS_FIXEDFILEINFO ffi;

			err = ReadFixedFileInfo( pszBuf, &ffi );
			if ( err == ERROR_SUCCESS && ffi.dwSignature == VS_FFI_SIGNATURE ) {

				USHORT v1 = HIWORD( ffi.dwProductVersionMS );
				USHORT v2 = LOWORD( ffi.dwProductVersionMS );
				USHORT v3 = HIWORD( ffi.dwProductVersionLS );
				USHORT v4 = LOWORD( ffi.dwProductVersionLS );

				wsprintf( pszBuf, _T("%hu"), v1 );
				setuservariable( INST_1, pszBuf );	/// $1 = v1

				wsprintf( pszBuf, _T("%hu"), v2 );
				setuservariable( INST_2, pszBuf );	/// $2 = v2

				wsprintf( pszBuf, _T("%hu"), v3 );
				setuservariable( INST_3, pszBuf );	/// $3 = v3

				wsprintf( pszBuf, _T("%hu"), v4 );
				setuservariable( INST_4, pszBuf );	/// $4 = v4

				wsprintf( pszBuf, _T("%hu.%hu.%hu.%hu"), v1, v2, v3, v4 );
				pushstring( pszBuf );

			} else {
				pushstring( _T(""));
			}
		}

		/// Free memory
		GlobalFree( pszBuf );
	}
}

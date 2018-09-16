
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2015/06/13

#include "main.h"


//++ [not exported] RegistryParsePath
BOOLEAN RegistryParsePath(
	__in LPCTSTR szFullPath,		/// ex: "HKLM\Software\Microsoft\Whatever"
	__out PHKEY phRoot,				/// Receives HKLM
	__out LPCTSTR *ppszPath			/// Receives "Software\Microsoft\Whatever"
	)
{
	BOOLEAN bRet = TRUE;
	if (szFullPath && *szFullPath && ppszPath) {

		*phRoot = NULL;
		*ppszPath = NULL;

		if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKLM\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_LOCAL_MACHINE;
			*ppszPath = szFullPath + 5;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKCU\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_CURRENT_USER;
			*ppszPath = szFullPath + 5;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 4, _T( "HKU\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_USERS;
			*ppszPath = szFullPath + 4;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKCR\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_CLASSES_ROOT;
			*ppszPath = szFullPath + 5;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKCC\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_CURRENT_CONFIG;
			*ppszPath = szFullPath + 5;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKDD\\" ), -1 ) == CSTR_EQUAL) {
			*phRoot = HKEY_DYN_DATA;
			*ppszPath = szFullPath + 5;
		} else if (CompareString( 0, NORM_IGNORECASE, szFullPath, 5, _T( "HKPD\\" ), -1 ) == CSTR_EQUAL) {
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
typedef int( *MultiSzCallback )(__in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam);

int CallbackMultiSzInsertBefore( __in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam )
{
	LPCTSTR pszInsertAfter = (LPCTSTR)pParam;
	if (pszInsertAfter && *pszInsertAfter && CompareString( 0, NORM_IGNORECASE, pszSubstr, -1, pszInsertAfter, -1 ) == CSTR_EQUAL)
		return MULTISZ_ACTION_INSERT_BEFORE;
	return MULTISZ_ACTION_NONE;
}

int CallbackMultiSzInsertAfter( __in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam )
{
	LPCTSTR pszInsertAfter = (LPCTSTR)pParam;
	if (pszInsertAfter && *pszInsertAfter && CompareString( 0, NORM_IGNORECASE, pszSubstr, -1, pszInsertAfter, -1 ) == CSTR_EQUAL)
		return MULTISZ_ACTION_INSERT_AFTER;
	return MULTISZ_ACTION_NONE;
}

int CallbackMultiSzInsertAtIndex( __in int iSubstrIndex, __in LPCTSTR pszSubstr, __in PVOID pParam )
{
	int iInsertAtIndex = PtrToInt( pParam );
	if (iSubstrIndex == iInsertAtIndex)
		return MULTISZ_ACTION_INSERT_BEFORE;
	return MULTISZ_ACTION_NONE;
}

//++ [not exported] RegMultiSzInsertImpl
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
	if (szRegKey && *szRegKey && szInsert && *szInsert) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if (RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_WRITE | KEY_READ | dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				int iAddLen = lstrlen( szInsert );
				DWORD iType, iOldLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iOldLen );
				if (err == ERROR_SUCCESS && iType == REG_MULTI_SZ && iOldLen > 2) {

					// Existing registry value
					int iNewLen = iOldLen / sizeof( TCHAR ) + iAddLen + 1;
					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iNewLen * sizeof( TCHAR ) );
					if (psz) {
						iOldLen = iNewLen * sizeof( TCHAR );
						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iOldLen );
						if (err == ERROR_SUCCESS) {

							BOOLEAN bFound;
							TCHAR *p;

							iOldLen /= sizeof( TCHAR );	/// Bytes -> TCHAR

							/// Enumerate all substrings and decide where to insert
							if (fnInsertCallback) {
								int i, iAction;
								for (p = psz, bFound = FALSE, i = 0; (*p && !bFound); i++) {
									iAction = fnInsertCallback( i, p, pInsertParam );
									switch (iAction) {
									case MULTISZ_ACTION_NONE:
										for (; *p; p++);	/// Skip current substring
										p++;				/// Skip NULL terminator
										break;
									case MULTISZ_ACTION_INSERT_BEFORE:
										bFound = TRUE;
										break;
									case MULTISZ_ACTION_INSERT_AFTER:
										bFound = TRUE;
										for (; *p; p++);	/// Skip current substring
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
								memmove( p + iAddLen + 1, p, iMoveLen * sizeof( TCHAR ) );
								lstrcpy( p, szInsert );
							}
							err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof( TCHAR ) );
						}
						HeapFree( GetProcessHeap(), 0, psz );

					} else {
						err = ERROR_OUTOFMEMORY;
					}

				} else {

					// New registry value
					int iNewLen = iAddLen + 2;
					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iNewLen * sizeof( TCHAR ) );
					if (psz) {
						lstrcpy( psz, szInsert );
						psz[iNewLen - 1] = _T( '\0' );
						psz[iNewLen - 2] = _T( '\0' );
						err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof( TCHAR ) );
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


//++ [not exported] RegMultiSzDeleteImpl
DWORD RegMultiSzDeleteImpl(
	__in LPCTSTR szRegKey,
	__in_opt LPCTSTR szRegValue,
	__in_opt DWORD dwKeyOpenFlags,
	__in LPCTSTR szDelete,
	__in BOOL bRemoveEmptyValue
	)
{
	DWORD err = ERROR_SUCCESS;
	if (szRegKey && *szRegKey && szDelete && *szDelete) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if (RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_WRITE | KEY_READ | dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				DWORD iType, iOldLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iOldLen );
				if (err == ERROR_SUCCESS && iType == REG_MULTI_SZ) {

					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iOldLen * sizeof( TCHAR ) );
					if (psz) {

						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iOldLen );
						if (err == ERROR_SUCCESS) {

							TCHAR *p;
							DWORD iNewLen;

							iOldLen /= sizeof( TCHAR );	/// Bytes -> TCHAR
							iNewLen = iOldLen;

							/// Enumerate all substrings
							for (p = psz; *p;) {
								if (CompareString( 0, NORM_IGNORECASE, p, -1, szDelete, -1 ) == CSTR_EQUAL) {
									/// Delete substring
									int iDeleteLen = lstrlen( p ) + 1;
									int iMoveLen = iOldLen - iDeleteLen - (int)(p - psz);
									memmove( p, p + iDeleteLen, iMoveLen * sizeof( TCHAR ) );
									iNewLen -= iDeleteLen;
								} else {
									for (; *p; p++);	/// Skip current substring
									p++;				/// Skip NULL terminator
								}
							}

							/// Set the registry value
							if (iNewLen != iOldLen) {
								if (!bRemoveEmptyValue || (iNewLen > 2)) {
									err = RegSetValueEx( hKey, szRegValue, 0, REG_MULTI_SZ, (LPBYTE)psz, iNewLen * sizeof( TCHAR ) );
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


//++ [not exported] RegMultiSzReadImpl
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
	if (szRegKey && *szRegKey) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if (RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_READ | dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				DWORD iType, iLen = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iLen );
				if (err == ERROR_SUCCESS && iType == REG_MULTI_SZ) {

					LPTSTR psz = (LPTSTR)HeapAlloc( GetProcessHeap(), 0, iLen * sizeof( TCHAR ) );
					if (psz) {
						err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, (LPBYTE)psz, &iLen );
						if (err == ERROR_SUCCESS) {

							TCHAR *p;
							int i;

							iLen /= sizeof( TCHAR );	/// Bytes -> TCHAR

							err = ERROR_INVALID_INDEX;
							for (p = psz, i = 0; *p; i++) {
								if (i == iIndex)
									if (pszOutString && iOutStringLen) {
										lstrcpyn( pszOutString, p, iOutStringLen );
										err = ERROR_SUCCESS;
										break;
									}
								for (; *p; p++);	/// Skip current substring
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


//++ [exported] RegMultiSzInsertAfter
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Substring to insert after (optional)
//+ Output:
//    [Stack] Win32 error
//+ Example:
//    NSutils::RegMultiSzInsertAfter /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" "Line 3"
//    Pop $0	; Win32 error

__declspec(dllexport)
void RegMultiSzInsertAfter(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255], szInsertAfter[255];
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE( szRegKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE( szRegValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE( szInsert ) );
					/// Param5: Substring to insert after
					if (popstring( pszBuf ) == 0) {
						lstrcpyn( szInsertAfter, pszBuf, ARRAYSIZE( szInsertAfter ) );

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


//++ [exported] RegMultiSzInsertBefore
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Substring to insert before (optional)
//+ Output:
//    [Stack] Win32 error
//+ Example:
//    NSutils::RegMultiSzInsertBefore /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" "Line 5"
//    Pop $0	; Win32 error

__declspec(dllexport)
void RegMultiSzInsertBefore(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255], szInsertBefore[255];
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE( szRegKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE( szRegValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE( szInsert ) );
					/// Param5: Substring to insert before
					if (popstring( pszBuf ) == 0) {
						lstrcpyn( szInsertBefore, pszBuf, ARRAYSIZE( szInsertBefore ) );

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


//++ [exported] RegMultiSzInsertAtIndex
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKLM\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to insert
//    [Stack] Zero based index (optional)
//+ Output:
//    [Stack] Win32 error
//+ Example:
//    NSutils::RegMultiSzInsertBefore [/NOUNLOAD] "HKLM\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" 3
//    Pop $0	; Win32 error

__declspec(dllexport)
void RegMultiSzInsertAtIndex(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szRegKey[512], szRegValue[128], szInsert[255];
		int iIndex;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szRegKey, pszBuf, ARRAYSIZE( szRegKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szRegValue, pszBuf, ARRAYSIZE( szRegValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to insert
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szInsert, pszBuf, ARRAYSIZE( szInsert ) );
					/// Param5: Index (zero based)
					iIndex = popint();

					// Insert substring
					err = RegMultiSzInsertImpl( szRegKey, szRegValue, dwFlags, szInsert, CallbackMultiSzInsertAtIndex, ULongToPtr( iIndex ) );
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


//++ [exported] RegMultiSzDelete
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Substring to delete
//    [Stack] Remove the registry value if it becomes empty (BOOL)
//+ Output:
//    [Stack] Win32 error
//+ Example:
//    NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} "Line 4" ${TRUE}
//    Pop $0	; Win32 error

__declspec(dllexport)
void RegMultiSzDelete(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szKey[512], szValue[128], szDelStr[255];
		BOOL bRemoveEmptyValue;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szKey, pszBuf, ARRAYSIZE( szKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szValue, pszBuf, ARRAYSIZE( szValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring to delete
				if (popstring( pszBuf ) == 0) {
					lstrcpyn( szDelStr, pszBuf, ARRAYSIZE( szDelStr ) );
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


//++ [exported] RegMultiSzRead
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Index (substring to read)
//+ Output:
//    [Stack] Win32 error
//    [Stack] The substring at specified index, or an empty string
//+ Example:
//    NSutils::RegMultiSzRead /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} 2
//    Pop $0	; Win32 error
//    Pop $1	; Substring at index 2

__declspec(dllexport)
void RegMultiSzRead(
	HWND parent,
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
	EXDLL_VALIDATE();

	// Init
	szSubstr[0] = 0;

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szKey[512], szValue[128];
		int iIndex;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szKey, pszBuf, ARRAYSIZE( szKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szValue, pszBuf, ARRAYSIZE( szValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Substring index
				iIndex = popint();

				// Read substring
				err = RegMultiSzReadImpl( szKey, szValue, dwFlags, iIndex, szSubstr, ARRAYSIZE( szSubstr ) );
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


//++ [not exported] RegBinaryWriteBufImpl
DWORD RegBinaryWriteBufImpl(
	__in LPCTSTR szRegKey,
	__in_opt LPCTSTR szRegValue,
	__in_opt DWORD dwKeyOpenFlags,
	__in int iOffset,
	__in LPVOID pBuf,
	__in ULONG iBufSize
	)
{
	DWORD err = ERROR_SUCCESS;
	if (szRegKey && *szRegKey && pBuf && iBufSize) {

		LPTSTR pszPath;
		HKEY hRoot, hKey;
		if (RegistryParsePath( szRegKey, &hRoot, (LPCTSTR*)&pszPath )) {

			err = RegCreateKeyEx( hRoot, pszPath, 0, NULL, 0, KEY_READ | KEY_WRITE | dwKeyOpenFlags, NULL, &hKey, NULL );
			if (err == ERROR_SUCCESS) {

				BOOL bFatalError = FALSE;
				LPBYTE pValBuf = NULL;
				ULONG iValBufSize = 0;

				// Read existing value
				DWORD iType, iSize = 0;
				err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, NULL, &iSize );
				if (err == ERROR_SUCCESS) {
					if (iType == REG_BINARY) {
						iValBufSize = iSize;
						iValBufSize = __max( iValBufSize, iOffset + iBufSize );		/// Extend the buffer if necessary
						pValBuf = (LPBYTE)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, iValBufSize );
						if (pValBuf) {
							err = RegQueryValueEx( hKey, szRegValue, NULL, &iType, pValBuf, &iSize );
						} else {
							err = ERROR_OUTOFMEMORY;
							bFatalError = TRUE;		/// We can't continue
						}
					} else {
						err = ERROR_DATATYPE_MISMATCH;
						bFatalError = TRUE;		/// We can't continue
					}
				}

				if (!bFatalError) {

					if (err != ERROR_SUCCESS) {
						// New value
						iValBufSize = iOffset + iBufSize;
						pValBuf = (LPBYTE)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, iValBufSize );
						err = pValBuf ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
					}

					if (err == ERROR_SUCCESS) {
						// Modify
						memmove( pValBuf + iOffset, pBuf, iBufSize );

						// Write
						err = RegSetValueEx( hKey, szRegValue, 0, REG_BINARY, pValBuf, iValBufSize );
					}
				}

				// Cleanup
				if (pValBuf)
					HeapFree( GetProcessHeap(), 0, pValBuf );

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

//++ [exported] RegBinaryInsertString
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] Registry key (ex: "HKCU\Software\MyCompany")
//    [Stack] Registry value. If empty, the default (unnamed) value is set
//    [Stack] Additional key flags (ex: KEY_WOW64_64KEY)
//    [Stack] Offset (bytes)
//    [Stack] The WCS (wide-character string)
//+ Output:
//    [Stack] Win32 error
//+ Example:
//    NSutils::RegBinaryInsertString /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" ${KEY_WOW64_64KEY} 100 "My string"
//    Pop $0; Win32 error

__declspec(dllexport)
void RegBinaryInsertString(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		TCHAR szKey[512], szValue[128];
		int iOffset;
		DWORD dwFlags;

		err = ERROR_INVALID_PARAMETER;

		///	Param1: Registry key
		if (popstring( pszBuf ) == 0) {
			lstrcpyn( szKey, pszBuf, ARRAYSIZE( szKey ) );
			/// Param2: Registry value
			if (popstring( pszBuf ) == 0) {
				lstrcpyn( szValue, pszBuf, ARRAYSIZE( szValue ) );
				/// Param3: Additional key flags
				dwFlags = popint();
				/// Param4: Byte offset
				iOffset = popint();
				/// Param5: String
				if (popstring( pszBuf ) == 0) {

					// Write
#ifdef _UNICODE
					err = RegBinaryWriteBufImpl( szKey, szValue, dwFlags, iOffset, pszBuf, lstrlen( pszBuf ) * sizeof( WCHAR ) );
#else
					/// MBCS -> Unicode
					ULONG iLen = lstrlen( pszBuf );
					LPWSTR pszBufW = (LPWSTR)GlobalAlloc( GPTR, (iLen + 1) * sizeof( WCHAR ) );
					if (pszBuf) {
						iLen = MultiByteToWideChar( CP_ACP, 0, pszBuf, -1, pszBufW, iLen + 1 );
						if (iLen > 0) {
							/// Write
							err = RegBinaryWriteBufImpl( szKey, szValue, dwFlags, iOffset, pszBufW, (iLen - 1)*sizeof( WCHAR ) );
						} else {
							err = GetLastError();
						}
						GlobalFree( pszBufW );
					} else {
						err = GetLastError();
					}
#endif
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

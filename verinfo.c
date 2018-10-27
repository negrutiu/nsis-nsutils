
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/06/11

#include "main.h"


// Forward declarations
DWORD ExtractVersionInfo( __in LPCTSTR szFile, __out BYTE **ppBuf, __out ULONG *piBufSize );		/// Caller must HeapFree(*ppBuf)

//++ MyMemset
void* __cdecl MyMemset( void *dest, int c, size_t count )
{
	LPBYTE p;
	for (p = (LPBYTE)(dest)+(count)-1; p >= (LPBYTE)(dest); p--)
		*p = c;
	return dest;
}

//++ MyMemcpy
void* __cdecl MyMemcpy( void *dest, const void *src, size_t count )
{
	LPBYTE p, q;
	for (p = (LPBYTE)src + count - 1, q = (LPBYTE)dest + count - 1; p >= (LPBYTE)src; p--, q--)
		*q = *p;
	return dest;
}


//++ FindFirstStringFileInfo
BOOL FindFirstStringFileInfo( __inout WORD **pp, __in LPWSTR pszParentKeyW, __out LPWSTR *ppszChildKeyW )
{
	LPWORD P = (LPWORD)*pp;		/// WORD pointer
	LPBYTE p = (LPBYTE)P;		/// BYTE pointer
	LPWORD pBlock = P;
	WORD iBlockSize = *P;

	WORD iLength, iValueLength, iType;
	LPWSTR pszKeyW;
	BOOL bHasChildren;

	iLength = *P++;
	if ( iLength > 0 ) {

		iValueLength = *P++;
		iType = *P++;		/// 0 == binary, 1 == text

		pszKeyW = (LPWSTR)P;
		for ( ; *P != UNICODE_NULL; P++ );
		P++;

		p += ((LPBYTE)P - (LPBYTE)pBlock) % 4;		/// DWORD padding

		if ( iValueLength > 0 ) {

			if ( iType == 1 ) {
				/// Note: iValueLength is not reliable. In most executables iValueLength means WCHAR-s, while in others (ResHacker.exe) it means bytes
				for ( ; *P != UNICODE_NULL; P++ );
				P++;
			} else {
				p += iValueLength;
			}

			p += ((LPBYTE)P - (LPBYTE)pBlock) % 4;	/// DWORD padding
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

	if (szStringValue)
		szStringValue[0] = 0;

	if ( szFile && *szFile && szStringName && *szStringName && szStringValue && ( iStringValueLen > 0 )) {

		LPBYTE pRes;
		DWORD iResSize;

		//? We won't be using GetFileVersionInfoSize(..) and GetFileVersionInfo(..) API functions
		//? They are unable to work with version info blocks that have a different language than the current thread's code page
		//? We'll use our own extraction routines
		//? (e.g. peinsider.exe)
		err = ExtractVersionInfo( szFile, &pRes, &iResSize );
		DebugString( _T("ExtractVersionInfo( File:\"%s\" ) = 0x%x\n"), szFile, err );
		if (err == ERROR_SUCCESS) {

			typedef struct _LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } LANGANDCODEPAGE;
			LANGANDCODEPAGE *pCodePage;
			UINT iCodePageSize = sizeof( *pCodePage );

			// Determine the language of the version info
			if (VerQueryValue( pRes, _T( "\\VarFileInfo\\Translation" ), (LPVOID*)&pCodePage, &iCodePageSize )) {

				TCHAR szTemp[255];
				LPCTSTR szValue = NULL;
				UINT iValueLen = 0;
				BOOL bFound;

				// Read the specified version info string
				wsprintf( szTemp, _T( "\\StringFileInfo\\%04x%04x\\%s" ), pCodePage->wLanguage, pCodePage->wCodePage, szStringName );
				bFound = VerQueryValue( pRes, szTemp, (LPVOID*)&szValue, &iValueLen );
				if (!bFound) {

					/// There might be executables where the "StringFileInfo" sub-block doesn't match the language specified by "Translation"
					/// In such case, we'll manually look for the first "StringFileInfo" translation.
					/// (Reproduced with a FunctionList.dll modified with XNResourceEditor)
					LPWORD p = (LPWORD)pRes;
					LPWSTR pszTranslation = NULL;
					bFound = FindFirstStringFileInfo( &p, NULL, &pszTranslation );
					if (bFound) {
						wsprintf( szTemp, _T( "\\StringFileInfo\\%ws\\%s" ), pszTranslation, szStringName );
						bFound = VerQueryValue( pRes, szTemp, (LPVOID*)&szValue, &iValueLen );
					}
				}

				// Prepare the output
				if (szValue && *szValue) {
					lstrcpyn( szStringValue, szValue, iStringValueLen );
					if (iValueLen > iStringValueLen) {
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

			HeapFree( GetProcessHeap(), 0, pRes );
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ ReadFixedFileInfo
DWORD ReadFixedFileInfo( __in_opt LPCTSTR szFile, __out VS_FIXEDFILEINFO *pFfi )
{
	DWORD err = ERROR_SUCCESS;

	if (pFfi)
		MyMemset( pFfi, 0, sizeof( *pFfi ) );
	if ( pFfi && szFile && *szFile ) {

		LPBYTE pRes;
		DWORD iResSize;

		err = ExtractVersionInfo( szFile, &pRes, &iResSize );
		if (err == ERROR_SUCCESS) {

			VS_FIXEDFILEINFO *ffi = NULL;
			UINT iSize = sizeof( ffi );	/// size of pointer!
			if (VerQueryValue( pRes, _T( "\\" ), (LPVOID*)&ffi, &iSize )) {

				// Return data to caller
				MyMemcpy( pFfi, ffi, iSize );

			} else {
				err = ERROR_NOT_FOUND;
			}

			HeapFree( GetProcessHeap(), 0, pRes );
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

// *******************************************************************************************
// ** Custom VERSIONINFO routines
// *******************************************************************************************

//+ ENUMRESPROC
typedef BOOL (CALLBACK *ENUMRESPROC)(
	__in HMODULE  hModule,
	__in LPCTSTR  pszResType,
	__in LPCTSTR  pszResName,
	__in WORD     iResLang,
	__in LPBYTE   pResPtr,
	__in ULONG    iResSize,
	__in LONG_PTR lParam
	);

//++ (internal) structure ENUM_CONTEXT
typedef struct _ENUM_CONTEXT {
	LPCTSTR pszResType;						/// Resource type to match. Can be NULL
	LPCTSTR pszResName;						/// Resource name to match. Can be NULL
	WORD    iResLang;						/// Resource language to match. Can be 0
	ENUMRESPROC fnCallback;					/// Callback function called for each resource that matches the search parameters
	LONG_PTR    lCallbackParam;
} ENUM_CONTEXT;


//++ (internal) _Callback_EnumResLang
BOOL CALLBACK _Callback_EnumResLang( __in_opt HMODULE hModule, __in LPCTSTR lpszType, __in LPCTSTR lpszName, __in WORD wIDLanguage, __in LONG_PTR lParam )
{
	BOOL bMatch = FALSE;
	ENUM_CONTEXT *ctx = (ENUM_CONTEXT*)lParam;

	// Determine if this resource language fits the enumeration criteria
	if (( ctx->iResLang == LANG_NEUTRAL ) || ( wIDLanguage == ctx->iResLang ))
		bMatch = TRUE;

	if ( bMatch ) {

		DWORD err = ERROR_SUCCESS;
		HRSRC hRsrc = FindResourceEx( hModule, lpszType, lpszName, wIDLanguage );
		if ( hRsrc ) {

			// Resource data
			HGLOBAL hGlobal = LoadResource( hModule, hRsrc );
			if ( hGlobal ) {
				LPVOID pResPtr = LockResource( hGlobal );
				if ( pResPtr ) {

					// Resource data size
					ULONG iResSize = SizeofResource( hModule, hRsrc );

					// Call the user supplied function
					return ctx->fnCallback( hModule, lpszType, lpszName, wIDLanguage, (LPBYTE)pResPtr, iResSize, ctx->lCallbackParam );

				} else {
					err = ERROR_NOT_FOUND;
				}
			} else {
				err = GetLastError();
			}
		} else {
			err = GetLastError();
		}
		UNREFERENCED_PARAMETER( err );
	}

	return TRUE;
}


//++ (internal) _Callback_EnumResName
BOOL CALLBACK _Callback_EnumResName( __in_opt HMODULE hModule, __in LPCTSTR lpszType, __in LPTSTR lpszName, __in LONG_PTR lParam )
{
	BOOL bMatch = FALSE;
	ENUM_CONTEXT *ctx = (ENUM_CONTEXT*)lParam;

	// Determine if this name of resource fits the enumeration criteria
	if ( ctx->pszResName == NULL ) {
		bMatch = TRUE;	/// Any resource name
	} else if ( lpszName == ctx->pszResName ) {
		bMatch = TRUE;	/// Same resource name
	} else if ( !IS_INTRESOURCE(lpszName) && !IS_INTRESOURCE(ctx->pszResName) && ( lstrcmp( lpszName, ctx->pszResName ) == 0 )) {
		bMatch = TRUE;	/// Same (literal) resource name
	}

	// Enumerate resource languages
	if ( bMatch )
		return EnumResourceLanguages( hModule, lpszType, lpszName, _Callback_EnumResLang, lParam );

	return TRUE;
}


//++ (internal) _Callback_EnumResType
BOOL CALLBACK _Callback_EnumResType( __in_opt HMODULE hModule, __in LPTSTR lpszType, __in LONG_PTR lParam )
{
	BOOL bMatch = FALSE;
	ENUM_CONTEXT *ctx = (ENUM_CONTEXT*)lParam;

	// Determine if this type of resource fits the enumeration criteria
	if ( ctx->pszResType == NULL ) {
		bMatch = TRUE;	/// Any resource type
	} else if ( lpszType == ctx->pszResType ) {
		bMatch = TRUE;	/// Same resource type
	} else if ( !IS_INTRESOURCE(lpszType) && !IS_INTRESOURCE(ctx->pszResType) && ( lstrcmp( lpszType, ctx->pszResType ) == 0 )) {
		bMatch = TRUE;	/// Same (literal) resource type
	}

	// Enumerate resource names
	if ( bMatch )
		return EnumResourceNames( hModule, lpszType, _Callback_EnumResName, lParam );

	return TRUE;
}


typedef struct {
	LPBYTE pRes;		/// Must be HeapFree(..)-ed by the caller
	ULONG iResSize;
} EXTRACT_VERSIONINFO_CONTEXT;


//++ ExtractVersionInfoCallback
BOOL CALLBACK ExtractVersionInfoCallback( __in HMODULE hModule, __in LPCTSTR pszResType, __in LPCTSTR pszResName, __in WORD iResLang, __in LPBYTE pResPtr, __in ULONG iResSize, __in LONG_PTR lParam )
{
	if (pszResType == RT_VERSION) {

		// Return a copy of the resource to the caller
		EXTRACT_VERSIONINFO_CONTEXT *pCtx = (EXTRACT_VERSIONINFO_CONTEXT*)lParam;
		pCtx->pRes = (LPBYTE)HeapAlloc( GetProcessHeap(), 0, iResSize );
		if (pCtx->pRes) {
			MyMemcpy( pCtx->pRes, pResPtr, iResSize );
			pCtx->iResSize = iResSize;
			return FALSE;		/// Stop resource enumeration
		}
	}
	return TRUE;	/// Continue resource enumeration
}


//++ ExtractVersionInfo
DWORD ExtractVersionInfo( __in LPCTSTR szFile, __out BYTE **ppBuf, __out ULONG *piBufSize )
{
	DWORD err = ERROR_SUCCESS;
	HMODULE hMod;

	if (!szFile || !*szFile || !ppBuf || !piBufSize)
		return ERROR_INVALID_PARAMETER;
	*ppBuf = NULL, *piBufSize = 0;

	hMod = LoadLibraryEx( szFile, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE );
	if (hMod) {

		// Enumerate RT_VERSION resources
		EXTRACT_VERSIONINFO_CONTEXT victx;
		ENUM_CONTEXT enumctx;

		MyMemset( &victx, 0, sizeof( victx ) );
		MyMemset( &enumctx, 0, sizeof( enumctx ) );

		enumctx.pszResType = RT_VERSION;
		enumctx.fnCallback = ExtractVersionInfoCallback;
		enumctx.lCallbackParam = (LONG_PTR)&victx;

		err = EnumResourceTypes( hMod, _Callback_EnumResType, (LONG_PTR)&enumctx ) ? ERROR_SUCCESS : GetLastError();
		if (err == ERROR_RESOURCE_ENUM_USER_STOP)
			err = ERROR_SUCCESS;

		*ppBuf = victx.pRes;
		*piBufSize = victx.iResSize;

		FreeLibrary( hMod );

	} else {
		err = GetLastError();
	}
	return err;
}

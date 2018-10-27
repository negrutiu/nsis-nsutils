
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2016/06/05

#include "main.h"

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


typedef NTSTATUS( NTAPI *TfnNtQuerySystemInformation )(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
typedef NTSTATUS( NTAPI *TfnNtDuplicateObject )(HANDLE SourceProcessHandle, HANDLE SourceHandle, HANDLE TargetProcessHandle, PHANDLE TargetHandle, ACCESS_MASK DesiredAccess, ULONG Attributes, ULONG Options);
typedef NTSTATUS( NTAPI *TfnNtQueryObject )(HANDLE ObjectHandle, ULONG ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength);


typedef struct _HANDLES_CONTEXT {

	_In_ LPWSTR TargetHandleNameW;							/// File name in system format (Ex: \Device\Harddisk1\Dir\File)

	_Out_ ULONG ProblematicHandleCount;
	_Out_ ULONG ClosedHandleCount;

	// Runtime data
	TfnNtQuerySystemInformation NtQuerySystemInformation;
	TfnNtDuplicateObject NtDuplicateObject;
	TfnNtQueryObject NtQueryObject;

	POBJECT_TYPE_INFORMATION ObjectTypeInfo;
	ULONG ObjectTypeInfoSize;

	PVOID ObjectNameInfo;
	ULONG ObjectNameInfoSize;

	BYTE FileObjectType;									/// Numeric "File" type, dynamically identified at runtime

	PSYSTEM_HANDLE_INFORMATION HandleInfo;
	ULONG HandleInfoSize;
	ULONG i;												/// HandleInfo->Handles iterator

} HANDLES_CONTEXT;


//++ mymemset
void* __cdecl mymemset( void *dest, int c, size_t count )
{
	char *p;
	for (p = (char*)dest + count - 1; p >= (char*)dest; p--)
		*p = c;
	return dest;
}


//++ DosPathToSystemPath
DWORD DosPathToSystemPath( __inout LPTSTR pszPath, __in DWORD iPathLen )
{
	DWORD err = ERROR_SUCCESS;
	if (pszPath && iPathLen && *pszPath) {
		if (pszPath[1] == _T( ':' )) {

			TCHAR szSysPath[128];
			TCHAR szLetter[3] = {pszPath[0], pszPath[1], 0};

			ULONG iLen = QueryDosDevice( szLetter, szSysPath, ARRAYSIZE( szSysPath ) );
			if (iLen > 0) {
				int iOriginalLen = lstrlen( pszPath );
				iLen = lstrlen( szSysPath );	/// Recompute string length. The length returned by QueryDosDevice is not reliable, because it adds multiple trailing NULL terminators
				if (iOriginalLen - 2 + iLen < iPathLen) {
					int i;
					for (i = iOriginalLen; i >= 2; i--)
						pszPath[i + iLen - 2] = pszPath[i];
					for (i = 0; szSysPath[i]; i++)
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


//++ CloseFileHandlesThread
DWORD WINAPI CloseFileHandlesThread( _In_  HANDLES_CONTEXT *ctx )
{
	NTSTATUS status;

	ULONG iProcessID = 0;
	HANDLE hProcess = NULL;

	/// Continue enumeration where we left off
	for (; ctx->i < ctx->HandleInfo->HandleCount; ctx->i++)
	{
		SYSTEM_HANDLE handle = ctx->HandleInfo->Handles[ctx->i];
		HANDLE dupHandle = NULL;
		PUNICODE_STRING pObjectName;
		ULONG returnLength;

		/* Only interested in files */
		if ((ctx->FileObjectType != 0) && (handle.ObjectTypeNumber != ctx->FileObjectType))
			continue;

		/* Open the process */
		/* Keep it open until a handle from another process is encountered. Minimize OpenProcess(..) calls */
		if (iProcessID != handle.ProcessId) {
			if (hProcess)
				CloseHandle( hProcess );
			iProcessID = handle.ProcessId;
			hProcess = OpenProcess( PROCESS_DUP_HANDLE, FALSE, iProcessID );
			/*printf( "%u. OpenProcess( Pid:%u ) == 0x%x\n", i, iProcessID, hProcess ? ERROR_SUCCESS : GetLastError());*/
		}
		if (!hProcess)
			continue;

		/* Duplicate the handle so we can query it. */
		status = ctx->NtDuplicateObject( hProcess, ULongToHandle( handle.Handle ), GetCurrentProcess(), &dupHandle, 0, 0, 0 );
		if (NT_SUCCESS( status )) {

			/* Query the object type */
			status = ctx->NtQueryObject( dupHandle, ObjectTypeInformation, ctx->ObjectTypeInfo, ctx->ObjectTypeInfoSize, NULL );
			if (NT_SUCCESS( status )) {

				/* Dynamically identify FILE object type (once!) */
				if (ctx->FileObjectType == 0) {
					if ((ctx->ObjectTypeInfo->Name.Length == 8) && (CompareStringW( 0, NORM_IGNORECASE, ctx->ObjectTypeInfo->Name.Buffer, 4, L"File", 4 ) == CSTR_EQUAL))
						ctx->FileObjectType = handle.ObjectTypeNumber;
					else {
						CloseHandle( dupHandle );
						continue;
					}
				}

				/* Query the object name */
				status = ctx->NtQueryObject( dupHandle, ObjectNameInformation, ctx->ObjectNameInfo, ctx->ObjectNameInfoSize, &returnLength );
				if (NT_SUCCESS( status )) {

					pObjectName = (PUNICODE_STRING)ctx->ObjectNameInfo;
					if (pObjectName->Length)
					{
						if (CompareStringW( 0, NORM_IGNORECASE, pObjectName->Buffer, pObjectName->Length / 2, ctx->TargetHandleNameW, -1 ) == CSTR_EQUAL)
						{
							HANDLE dupCloseHandle;
							status = ctx->NtDuplicateObject( hProcess, ULongToHandle( handle.Handle ), GetCurrentProcess(), &dupCloseHandle, 0, 0, DUPLICATE_CLOSE_SOURCE );
							if (NT_SUCCESS( status ))
								CloseHandle( dupCloseHandle );
							/*printf("NtDuplicateObject( pid:%u, handle:%u, type:%.*S, name:%.*S, DUPLICATE_CLOSE_SOURCE ) == 0x%x\n", handle.ProcessId, (ULONG)handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer, pObjectName->Length / 2, pObjectName->Buffer, status);*/
							ctx->ClosedHandleCount++;
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
				/*printf("[%#x] NtQueryObject = 0x%x\n", handle.Handle, status);*/
			}

			CloseHandle( dupHandle );

		} else {
			/*printf("[%#x] NtDuplicateObject == 0x%x\n", handle.Handle, status);*/
		}
	} /// for

	if (hProcess)
		CloseHandle( hProcess );

	return ERROR_SUCCESS;
}


//++ CloseFileHandlesImpl
ULONG CloseFileHandlesImpl(
	__in LPCTSTR pszHandleName,
	__out_opt PULONG piClosedCount
)
{
	NTSTATUS status = ERROR_SUCCESS;
	HANDLES_CONTEXT ctx;

	// Validate input
	if (!pszHandleName || !*pszHandleName)
		return ERROR_INVALID_PARAMETER;

	// Init
	mymemset( &ctx, 0, sizeof(ctx) );
	ctx.NtQuerySystemInformation = (TfnNtQuerySystemInformation)GetProcAddress( GetModuleHandle( _T( "ntdll" ) ), "NtQuerySystemInformation" );
	ctx.NtDuplicateObject = (TfnNtDuplicateObject)GetProcAddress( GetModuleHandle( _T( "ntdll" ) ), "NtDuplicateObject" );
	ctx.NtQueryObject = (TfnNtQueryObject)GetProcAddress( GetModuleHandle( _T( "ntdll" ) ), "NtQueryObject" );

	// UNICODE file name
#if defined (_UNICODE)
	ctx.TargetHandleNameW = (LPWSTR)pszHandleName;
#else
	ctx.TargetHandleNameW = (LPWSTR)HeapAlloc( GetProcessHeap(), 0, (lstrlen( pszHandleName ) + 128) * sizeof( WCHAR ) );
	MultiByteToWideChar( CP_ACP, 0, pszHandleName, -1, ctx.TargetHandleNameW, HeapSize( GetProcessHeap(), 0, ctx.TargetHandleNameW ) / sizeof( WCHAR ) );
#endif

	// Allocate working structures
	ctx.ObjectTypeInfoSize = 0x80000;		/// 512 KiB
	ctx.ObjectTypeInfo = (POBJECT_TYPE_INFORMATION)HeapAlloc( GetProcessHeap(), 0, ctx.ObjectTypeInfoSize );

	ctx.ObjectNameInfoSize = 0x80000;		/// 512 KiB
	ctx.ObjectNameInfo = HeapAlloc( GetProcessHeap(), 0, ctx.ObjectNameInfoSize );

	ctx.HandleInfoSize = 0x10000;			/// 65 KiB
	ctx.HandleInfo = (PSYSTEM_HANDLE_INFORMATION)HeapAlloc( GetProcessHeap(), 0, ctx.HandleInfoSize );

	if (ctx.ObjectTypeInfo && ctx.ObjectNameInfo && ctx.HandleInfo)
	{
		/* NtQuerySystemInformation won't give us the correct buffer size, so we guess by doubling the buffer size. */
		while ((status = ctx.NtQuerySystemInformation( SystemHandleInformation, ctx.HandleInfo, ctx.HandleInfoSize, NULL )) == STATUS_INFO_LENGTH_MISMATCH) {
			ctx.HandleInfoSize *= 2;
			ctx.HandleInfo = (PSYSTEM_HANDLE_INFORMATION)HeapReAlloc( GetProcessHeap(), 0, ctx.HandleInfo, ctx.HandleInfoSize );
		}

		/* NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH. */
		if (NT_SUCCESS( status ))
		{
			/// MESSAGE IN A BOTTLE:
			/// It is well known that NtQueryObject(..) calls might hang on certain handles, such as named pipes.
			/// There is no way to predict/prevent the hangs, therefore we'll do all the work from an (expandable) worker thread.
			/// If the thread becomes unresponsive it'll be terminated, and the handle that caused NtQueryObject to hang will be skipped.
			/// (Negrutiu)

			while (ctx.i < ctx.HandleInfo->HandleCount) {
				HANDLE hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)CloseFileHandlesThread, &ctx, 0, NULL );
				if (WaitForSingleObject( hThread, 1000 ) == WAIT_TIMEOUT) {
					TerminateThread( hThread, ERROR_TIMEOUT );
					///{
					///	TCHAR szTemp[255];
					///	wsprintf(
					///		szTemp,
					///		_T( "Timeout\n\ni = %u\npid = %u\nhandle = 0x%x\nflags = 0x%x\naccess = 0x%x" ),
					///		ctx.i, ctx.HandleInfo->Handles[ctx.i].ProcessId,
					///		(ULONG)ctx.HandleInfo->Handles[ctx.i].Handle,
					///		(ULONG)ctx.HandleInfo->Handles[ctx.i].Flags,
					///		(ULONG)ctx.HandleInfo->Handles[ctx.i].GrantedAccess
					///	);
					///	MessageBox( 0, szTemp, 0, MB_ICONERROR );
					///}
					ctx.ProblematicHandleCount++;
					ctx.i++;	/// Skip the problematic handle
				}
				CloseHandle( hThread );
			}
		} else {
			/*printf("NtQuerySystemInformation(...) == 0x%x\n", status);*/
		}
	} else {
		status = ERROR_OUTOFMEMORY;
	}

	if (piClosedCount)
		*piClosedCount = ctx.ClosedHandleCount;

	if (ctx.ObjectTypeInfo)
		HeapFree( GetProcessHeap(), 0, ctx.ObjectTypeInfo );
	if (ctx.ObjectNameInfo)
		HeapFree( GetProcessHeap(), 0, ctx.ObjectNameInfo );
	if (ctx.HandleInfo)
		HeapFree( GetProcessHeap(), 0, ctx.HandleInfo );

#if !defined (_UNICODE)
	HeapFree( GetProcessHeap(), 0, ctx.TargetHandleNameW );
#endif

	return status;
}


//++ [exported] CloseFileHandles
//  ----------------------------------------------------------------------
//+ Input:
//    [Stack] File/folder path
//+ Output:
//    [Stack] Number of closed handles
//+ Example:
//    NSutils::CloseFileHandles [/NOUNLOAD] "C:\Windows\System32\drivers\etc\hosts"
//    Pop $0	; The number of closed handles

void __declspec(dllexport) CloseFileHandles(
	HWND parent,
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
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store any NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof( TCHAR ) * string_size );
	if (pszBuf) {

		DWORD err = ERROR_SUCCESS;

		///	Param1: file path
		if (popstring( pszBuf ) == 0)
		{
			err = DosPathToSystemPath( pszBuf, string_size );
			err = CloseFileHandlesImpl( pszBuf, &iClosedCount );
		}

		/// Free memory
		GlobalFree( pszBuf );
		UNREFERENCED_PARAMETER( err );
	}

	/// Return value
	pushintptr( iClosedCount );
}


_______________________________________________________________________________

Functions exported by NSutils
[marius.negrutiu@gmail.com]
_______________________________________________________________________________

[GetVersionInfoString]
Extracts a specific string from an executable's version information block.
Be mindful of the file system redirection on 64-bit platforms!

Syntax:
	NSutils::GetVersionInfoString [/NOUNLOAD] ExeName VersionInfoString

Return value:
	The string will be returned on the stack.
	An empty string will be returned in case of errors.

Example:
	${DisableX64FSRedirection}	; Optional. On 64-bit platforms, read from System32 rather than SysWOW64
	NSutils::GetVersionInfoString /NOUNLOAD "$SYSDIR\Notepad.exe" "LegalCopyright"
	Pop $0
	${If} "$0" != ""
		; Valid string
		MessageBox MB_ICONINFORMATION 'LegalCopyright = "$0"'
	${Else}
		; Some error. Probably the string hasn't been found
	${EndIf}
	${EnableX64FSRedirection}
_______________________________________________________________________________

[GetFileVersion]
Extracts the numeric file version from an executable's version information block
Be mindful of the file system redirection on 64-bit platforms!

Syntax:
	NSutils::GetFileVersion [/NOUNLOAD] ExeName

Return values:
	If the operation is successful, the function will return a pre-formatted
	version string (like "v1.v2.v3.v4") on the stack. In addition, $1, $2, $3 and $4
	will store each version component.
	If the operation is unsuccessful, an empty string will be returned on the
	stack. $1..$4 will *not* be set.

Example:
	${DisableX64FSRedirection}	; Optional. On 64-bit platforms, read from System32 rather than SysWOW64
	NSutils::GetFileVersion /NOUNLOAD "$SYSDIR\Notepad.exe"
	Pop $0
	${If} "$0" != ""
		; $1, $2, $3, $4 are valid
		MessageBox MB_ICONINFORMATION "FileVersion: $0 ($1, $2, $3, $4)"
	${Else}
		; Error
	${EndIf}
	${EnableX64FSRedirection}
_______________________________________________________________________________

[GetProductVersion]
Extracts the numeric product version from an executable's version information block
Be mindful of the file system redirection on 64-bit platforms!

Syntax:
	NSutils::GetProductVersion [/NOUNLOAD] ExeName

Return values:
	If the operation is successful, the function will return a pre-formatted
	version string (like "v1.v2.v3.v4") on the stack. In addition, $1, $2, $3 and $4
	will store each version component.
	If the operation is unsuccessful, an empty string will be returned on the
	stack. $1..$4 will *not* be set.

Example:
	${DisableX64FSRedirection}	; Optional. On 64-bit platforms, read from System32 rather than SysWOW64
	NSutils::GetFileVersion /NOUNLOAD "$SYSDIR\Notepad.exe"
	Pop $0
	${If} "$0" != ""
		; $1, $2, $3, $4 are valid
		MessageBox MB_ICONINFORMATION "ProductVersion: $0 ($1, $2, $3, $4)"
	${Else}
		; Error
	${EndIf}
	${EnableX64FSRedirection}
_______________________________________________________________________________

[ExecutePendingFileRenameOperations]
The routine examines the "PendingFileRenameOperations" registry value and
executes the operations on files that match a specified search pattern.
The operations will be removed from the registry value, therefore they won't
be executed during the next reboot...

Syntax:
	NSutils::ExecutePendingFileRenameOperations [/NOUNLOAD] "FileSubstring"

Return values:
	Two error codes are returned on the stack.
	1. Win32 error code
	2. Win32 error code of the first failed operation

Example:
	NSutils::ExecutePendingFileRenameOperations [/NOUNLOAD] "AppData\Local\Temp"
	Pop $0
	Pop $1
	${If} $0 = ${ERROR_SUCCESS}
		${If} $1 = ${ERROR_SUCCESS}
			; Success. All file operations were successful, as well
		${Else}
			; Success, but some file operations failed...
		${EndIf}
	${Else}
	${EndIf}
_______________________________________________________________________________

[DisableProgressStepBack]
[RestoreProgressStepBack]
Don't allow the progress bar to step back.
It happens when you have loops in the script.
NSutils must remain loaded between calls, therefore /NOUNLOAD is mandatory!

Syntax:
	NSutils::DisableProgressStepBack /NOUNLOAD ProgressBarHandle
	NSutils::RestoreProgressStepBack [/NOUNLOAD] ProgressBarHandle

Return value:
	None

Example:
	NSutils::DisableProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar
	${For} $R0 1 10
		Sleep 1000
	${Next}
	NSutils::RestoreProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar
_______________________________________________________________________________


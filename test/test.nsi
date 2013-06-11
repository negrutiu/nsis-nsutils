
!include "MUI2.nsh"
;!include "LogicLib.nsh"

# The folder where NSutils.dll is
!ifdef NSIS_UNICODE
	!AddPluginDir "..\ReleaseW"
!else
	!AddPluginDir "..\ReleaseA"
!endif

# Prepare strings
!insertmacro MUI_LANGUAGE "English"

# Installer details
!ifdef NSIS_UNICODE
	Name "NSutilsW"
	OutFile "NSutilsW.exe"
!else
	Name "NSutilsA"
	OutFile "NSutilsA.exe"
!endif

XPStyle on
RequestExecutionLevel user ; don't require UAC elevation

Function .onInit

	Var /GLOBAL FNAME
	StrCpy $FNAME "$SYSDIR\Notepad.exe"
	StrCpy $5 "$FNAME$\n-----------------------------------------$\n"

	NSutils::GetFileVersion /NOUNLOAD "$FNAME"
	Pop $0
	StrCpy $5 "$5FileVersion: $0 ($1,$2,$3,$4)$\n"

	NSutils::GetProductVersion /NOUNLOAD "$FNAME"
	Pop $0
	StrCpy $5 "$5ProductVersion: $0 ($1,$2,$3,$4)$\n"

	StrCpy $5 "$5-----------------------------------------$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "CompanyName"
	Pop $0
	StrCpy $5 "$5CompanyName: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "FileDescription"
	Pop $0
	StrCpy $5 "$5FileDescription: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "FileVersion"
	Pop $0
	StrCpy $5 "$5FileVersion: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "InternalName"
	Pop $0
	StrCpy $5 "$5InternalName: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "LegalCopyright"
	Pop $0
	StrCpy $5 "$5LegalCopyright: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "OriginalFilename"
	Pop $0
	StrCpy $5 "$5OriginalFilename: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "ProductName"
	Pop $0
	StrCpy $5 "$5ProductName: $0$\n"

	NSutils::GetVersionInfoString /NOUNLOAD "$FNAME" "ProductVersion"
	Pop $0
	StrCpy $5 "$5ProductVersion: $0$\n"

	MessageBox MB_OK|MB_ICONINFORMATION "$5"
	
	NSutils::ExecutePendingFileRenameOperations "Update\Flags\Pending"
	Pop $0	; Win32 error code
	Pop $1	; File operations Win32 error code
	MessageBox MB_OK 'ExecutePendingFileRenameOperations( "Update\Flags\Pending" )$\nError: $0$\nFile Operation Error: $1'

	NSutils::ExecutePendingFileRenameOperations "PendingBullGuardUpdate"
	Pop $0
	Pop $1
	MessageBox MB_OK 'ExecutePendingFileRenameOperations( "PendingBullGuardUpdate" )$\nError: $0$\nFile Operation Error: $1'

	Quit
FunctionEnd

Section -
SectionEnd


!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

# The folder where NSutils.dll is
!ifdef NSIS_UNICODE
	!AddPluginDir "..\ReleaseW"
!else
	!AddPluginDir "..\ReleaseA"
!endif

SpaceTexts "none"
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_INSTFILES

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
ShowInstDetails show

!define Print DetailPrint

!ifndef TRUE
	!define TRUE 1
!endif
!ifndef FALSE
	!define FALSE 0
!endif

Function .onInit

/*
	NSutils::ExecutePendingFileRenameOperations "Update\Flags\Pending"
	Pop $0	; Win32 error code
	Pop $1	; File operations Win32 error code
	MessageBox MB_OK 'ExecutePendingFileRenameOperations( "Update\Flags\Pending" )$\nError: $0$\nFile Operation Error: $1'

	NSutils::ExecutePendingFileRenameOperations "PendingBullGuardUpdate"
	Pop $0
	Pop $1
	MessageBox MB_OK 'ExecutePendingFileRenameOperations( "PendingBullGuardUpdate" )$\nError: $0$\nFile Operation Error: $1'
*/
FunctionEnd

Function PrintFileVersion

	Pop $R0		; The file name

	${Print} "$R0"

	NSutils::GetFileVersion /NOUNLOAD "$R0"
	Pop $0
	${Print} "    FileVersion: $0 ($1,$2,$3,$4)"

	NSutils::GetProductVersion /NOUNLOAD "$R0"
	Pop $0
	${Print} "    ProductVersion: $0 ($1,$2,$3,$4)"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "CompanyName"
	Pop $0
	${Print} "    CompanyName: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "FileDescription"
	Pop $0
	${Print} "    FileDescription: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "FileVersion"
	Pop $0
	${Print} "    FileVersion: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "InternalName"
	Pop $0
	${Print} "    InternalName: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "LegalCopyright"
	Pop $0
	${Print} "    LegalCopyright: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "OriginalFilename"
	Pop $0
	${Print} "    OriginalFilename: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "ProductName"
	Pop $0
	${Print} "    ProductName: $0"

	NSutils::GetVersionInfoString /NOUNLOAD "$R0" "ProductVersion"
	Pop $0
	${Print} "    ProductVersion: $0"

FunctionEnd


Section /o "Test version info"

	${Print} "--------------------------------------------------------------"
	${GetSize} "$SYSDIR" "/M=Notepad.exe /S=0K /G=0" $0 $1 $2
	${Print} "    File system redirection: Enabled"
	${Print} "    FileSize: $0 KB"
	Push "$SYSDIR\Notepad.exe"
	Call PrintFileVersion

	${DisableX64FSRedirection}
	${Print} "--------------------------------------------------------------"
	${Print} "    File system redirection: Disabled"
	${GetSize} "$SYSDIR" "/M=Notepad.exe /S=0K /G=0" $0 $1 $2
	${Print} "    FileSize: $0 KB"
	Push "$SYSDIR\Notepad.exe"
	Call PrintFileVersion
	${EnableX64FSRedirection}

SectionEnd


Section /o "Test progress bar (default, steping back)"

	${Print} "--------------------------------------------------------------"
	${Print} "Looping with the default progress bar settings..."
	${For} $R0 1 10
		${Print} "    Step $R0/10"
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
	${Next}

SectionEnd


Section /o "Test progress bar (fixed, no step back)"

	NSutils::DisableProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar
	${Print} "--------------------------------------------------------------"
	${Print} "Looping with DisableProgressStepBack..."
	${For} $R0 1 10
		${Print} "    Step $R0/10"
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 100
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
		Sleep 10
	${Next}
	NSutils::RestoreProgressStepBack /NOUNLOAD $mui.InstFilesPage.ProgressBar

SectionEnd


Section /o "Test PendingFileRenameOperations (requires Admin)"

	${Print} "--------------------------------------------------------------"

	System::Call 'kernel32::CopyFile( t "$SYSDIR\Notepad.exe", t "$DESKTOP\MyNotepad.exe", i 0 ) i.r0'
	${Print} 'CopyFile( "Notepad.exe", "DESKTOP\MyNotepad.exe" ) == $0'

	!define MOVEFILE_DELAY_UNTIL_REBOOT 0x4
	System::Call 'kernel32::MoveFileEx( t "$DESKTOP\MyNotepad.exe", t "$DESKTOP\MyNotepad2.exe", i ${MOVEFILE_DELAY_UNTIL_REBOOT} ) i.r0'
	${Print} 'MoveFileEx( "DESKTOP\MyNotepad.exe", "DESKTOP\MyNotepad2.exe", MOVEFILE_DELAY_UNTIL_REBOOT ) == $0'

	NSutils::ExecutePendingFileRenameOperations /NOUNLOAD "MyNotepad"
	Pop $0
	Pop $1

	${If} ${FileExists} "$DESKTOP\MyNotepad2.exe"
		${Print} "[SUCCESS] ExecutePendingFileRenameOperations ($$0 = $0, $$1 = $1)"
	${Else}
		${Print} "[ERROR] ExecutePendingFileRenameOperations ($$0 = $0, $$1 = $1)"
		${If} $0 = 5
			${Print} "[ERROR] Access is denied. Run as administrator!"
		${EndIf}
	${EndIf}

	Delete "$DESKTOP\MyNotepad.exe"
	Delete "$DESKTOP\MyNotepad2.exe"

SectionEnd


Section /o "Test string table manipulation"

	${Print} "--------------------------------------------------------------"

	System::Call 'kernel32::CopyFile( t "$EXEPATH", t "$DESKTOP\MyTest.exe", i 0 ) i.r0'
	${Print} 'CopyFile( "$EXEPATH", "DESKTOP\MyTest.exe" ) == $0'

	NSutils::ReadResourceString /NOUNLOAD "$DESKTOP\MyUser32.dll" 100 1033
	Pop $0
	${If} $0 == ""
		${Print} 'String #10: "$0". Ok!'
	${Else}
		${Print} 'String #10: "$0". Should have been empty!'
	${EndIf}

	NSutils::WriteResourceString /NOUNLOAD "$DESKTOP\MyTest.exe" 100 1033 "Dela beat cârciumă vin / Merg pe gard, de drum mă țin"
	Pop $0
	${If} $0 = ${FALSE}
		StrCpy $0 "ERROR"
	${Else}
		StrCpy $0 "SUCCESS"
	${EndIf}
	${Print} 'Write #100: $0'

	NSutils::ReadResourceString /NOUNLOAD "$DESKTOP\MyTest.exe" 100 1033
	Pop $0
	${If} $0 != ""
		${Print} 'String #100: "$0". Ok!'
	${Else}
		${Print} 'String #100: "". Should have been valid!'
	${EndIf}

	NSutils::WriteResourceString /NOUNLOAD "$DESKTOP\MyTest.exe" 100 1033 ""
	Pop $0
	${If} $0 = ${FALSE}
		StrCpy $0 "ERROR"
	${Else}
		StrCpy $0 "SUCCESS"
	${EndIf}
	${Print} 'Delete #100: $0'

	NSutils::ReadResourceString /NOUNLOAD "$DESKTOP\MyTest.exe" 100 1033
	Pop $0
	${If} $0 == ""
		${Print} 'String #10: "$0". Ok!'
	${Else}
		${Print} 'String #10: "$0". Should have been empty!'
	${EndIf}

	Delete "$DESKTOP\MyTest.exe"

SectionEnd


Section "-Cleanup"
	; Make sure NSutils is not loaded (in case all previous calls were made with /NOUNLOAD)
	NSutils::DisableProgressStepBack 0	; Dummy call. No effect.
SectionEnd

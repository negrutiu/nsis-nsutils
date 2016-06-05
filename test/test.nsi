
!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "Win\WinNT.nsh"

;!define DEBUGGING_ENABLED

# The folder where NSutils.dll is
!ifdef NSIS_UNICODE
	!define NSUTILS "$EXEDIR\..\DebugW\NSutils.dll"		; DEBUGGING_ENABLED
	!AddPluginDir "..\ReleaseW-mingw"
!else
	!define NSUTILS "$EXEDIR\..\DebugA\NSutils.dll"		; DEBUGGING_ENABLED
	!AddPluginDir "..\ReleaseA-mingw"
!endif

!define ERROR_SUCCESS 0

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
ManifestDPIAware true
ManifestSupportedOS all
ShowInstDetails show

!define Print DetailPrint

!ifndef TRUE
	!define TRUE 1
!endif
!ifndef FALSE
	!define FALSE 0
!endif

Function .onInit
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


Section /o "Test progress bar (fixed, no stepping back)"

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

!ifdef DEBUGGING_ENABLED
	Push "$EXEDIR\PendingFileRename.log"
	Push "MyNotepad"
	CallInstDLL "${NSUTILS}" ExecutePendingFileRenameOperations
!else
	NSutils::ExecutePendingFileRenameOperations /NOUNLOAD "MyNotepad" "$EXEDIR\PendingFileRename.log"
!endif
	Pop $0	; Win32 error code
	Pop $1	; Win32 error code of the first failed operation

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


Section /o "Test FindFileRenameOperations"

	${Print} "--------------------------------------------------------------"

	StrCpy $R0 "temp"	; Substring to find
!ifdef DEBUGGING_ENABLED
	Push $R0
	CallInstDLL "${NSUTILS}" FindPendingFileRenameOperations
!else
	NSutils::FindPendingFileRenameOperations /NOUNLOAD $R0
!endif
	Pop $0
	${Print} 'FindPendingFileRenameOperations( "$R0" ) == "$0"'


	StrCpy $R0 "*"		; Substring to find
!ifdef DEBUGGING_ENABLED
	Push $R0
	CallInstDLL "${NSUTILS}" FindPendingFileRenameOperations
!else
	NSutils::FindPendingFileRenameOperations /NOUNLOAD $R0
!endif
	Pop $0
	${Print} 'FindPendingFileRenameOperations( "$R0" ) == "$0"'

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


Section "Test close file handles"

	${Print} "--------------------------------------------------------------"
	${DisableX64FSRedirection}

	!define CREATE_NEW				1
	!define CREATE_ALWAYS			2
	!define OPEN_EXISTING			3
	!define OPEN_ALWAYS				4

	!define INVALID_HANDLE_VALUE	-1

	;!define TESTFILE "$TEMP\test_close_handles.txt"
	!define TESTFILE "$SYSDIR\drivers\etc\hosts"
	${Print} "TESTFILE = ${TESTFILE}"

	; Open test file (handle1)
	System::Call 'kernel32::CreateFile( t "${TESTFILE}", i ${GENERIC_READ}, i ${FILE_SHARE_READ}, p 0, i ${OPEN_ALWAYS}, i ${FILE_ATTRIBUTE_NORMAL}, p 0 ) i.r10 ? e'
	Pop $0	; GetLastError
	${If} $R0 p<> ${INVALID_HANDLE_VALUE}
		StrCpy $0 0
	${EndIf}
	${Print} 'CreateFile( #1, TESTFILE ) = $0'

	; Open test file (handle2)
	System::Call 'kernel32::CreateFile( t "${TESTFILE}", i ${GENERIC_READ}, i ${FILE_SHARE_READ}, p 0, i ${OPEN_ALWAYS}, i ${FILE_ATTRIBUTE_NORMAL}, p 0 ) i.r11 ? e'
	Pop $0	; GetLastError
	${If} $R1 p<> ${INVALID_HANDLE_VALUE}
		StrCpy $0 0
	${EndIf}
	${Print} 'CreateFile( #2, TESTFILE ) = $0'

	; ----------------------------------

	${Print} 'Close TESTFILE file handles'
!ifdef DEBUGGING_ENABLED
	Push "${TESTFILE}"
	CallInstDLL "${NSUTILS}" CloseFileHandles
!else
	NSutils::CloseFileHandles /NOUNLOAD "${TESTFILE}"
!endif
	Pop $0
	${Print} '  $0 handles closed'

	; ----------------------------------

	; Close test file (handle1)
	System::Call 'kernel32::CloseHandle( p r10 ) i.r1 ? e'
	Pop $0	; GetLastError
	${If} $1 <> ${FALSE}
		StrCpy $0 0
	${EndIf}
	${If} $0 = 6
		${Print} 'CloseHandle( #1, TESTFILE ) = $0 [CORRECT]'
	${Else}
		${Print} 'CloseHandle( #1, TESTFILE ) = $0 [INCORRECT]'
	${EndIf}

	; Close test file (handle2)
	System::Call 'kernel32::CloseHandle( p r11 ) i.r1 ? e'
	Pop $0	; GetLastError
	${If} $1 <> ${FALSE}
		StrCpy $0 0
	${EndIf}
	${If} $0 = 6
		${Print} 'CloseHandle( #2, TESTFILE ) = $0 [CORRECT]'
	${Else}
		${Print} 'CloseHandle( #2, TESTFILE ) = $0 [INCORRECT]'
	${EndIf}

	;Delete "${TESTFILE}"

	${EnableX64FSRedirection}

SectionEnd


Section /o "Test REG_MULTI_SZ operations"

	${Print} "--------------------------------------------------------------"
	${Print} "REG_MULTI_SZ tests"

	SetRegView 64
	DeleteRegValue HKCU "Software\MyCompany" "MyValue"
	DeleteRegKey HKCU "Software\MyCompany"

	; Insert
	NSutils::RegMultiSzInsertAfter /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "ccc" ""
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ccc" after "" ) = $0'

	NSutils::RegMultiSzInsertBefore /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "bbb" "ccc"
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "bbb" before "ccc" ) = $0'

	NSutils::RegMultiSzInsertAfter /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "ddd" "ccc"
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ddd" after "ccc" ) = $0'

	NSutils::RegMultiSzInsertAtIndex /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "aaa" 0
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ddd" at index 0 ) = $0'

	NSutils::RegMultiSzInsertAtIndex /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "eee" 4
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "eee" at index 4 ) = $0'

	; Verify
	${For} $1 0 1000
		NSutils::RegMultiSzRead /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 $1
		Pop $0	; Win32 error
		Pop $2	; The substring
		IntFmt $0 "0x%x" $0
		StrCpy $3 ""
		${If} $0 = ${ERROR_SUCCESS}
			${If} $1 = 0
				StrCpy $3 "[CORRECT]"
				StrCmp $2 "aaa" +2 +1
					StrCpy $3 "[INCORRECT]"
			${ElseIf} $1 = 1
				StrCpy $3 "[CORRECT]"
				StrCmp $2 "bbb" +2 +1
					StrCpy $3 "[INCORRECT]"
			${ElseIf} $1 = 2
				StrCpy $3 "[CORRECT]"
				StrCmp $2 "ccc" +2 +1
					StrCpy $3 "[INCORRECT]"
			${ElseIf} $1 = 3
				StrCpy $3 "[CORRECT]"
				StrCmp $2 "ddd" +2 +1
					StrCpy $3 "[INCORRECT]"
			${ElseIf} $1 = 4
				StrCpy $3 "[CORRECT]"
				StrCmp $2 "eee" +2 +1
					StrCpy $3 "[INCORRECT]"
			${EndIf}
		${EndIf}
		${Print} '  RegMultiSzRead( $1 ) = $0: "$2" $3'
		IntCmp $0 ${ERROR_SUCCESS} +2 +1 +1
			${Break}
	${Next}

	; Delete
	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "ccc" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "ccc" ) = $0'

	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "bbb" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "bbb" ) = $0'

	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "aaa" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "aaa" ) = $0'

	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "ddd" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "ddd" ) = $0'

	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "eee" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "eee" ) = $0'

	NSutils::RegMultiSzDelete /NOUNLOAD "HKCU\Software\MyCompany" "MyValue" 0 "xxx" ${TRUE}
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "xxx" ) = $0'

	DeleteRegValue HKCU "Software\MyCompany" "MyValue"
	DeleteRegKey HKCU "Software\MyCompany"
	SetRegView 32

SectionEnd


Section /o "Test REG_BINARY operations"

	${Print} "--------------------------------------------------------------"
	${Print} "REG_BINARY tests"

	!define REGKEY "Software\MyCompany"
	!define REGVAL "MyValue"

	SetRegView 64
	DeleteRegValue HKCU "${REGKEY}" "${REGVAL}"
	DeleteRegKey HKCU "${REGKEY}"

	; Write 1
	!define STRING "abc"
	!define OFFSET 10
	!define FLAGS 0
!ifdef DEBUGGING_ENABLED
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
!else
	NSutils::RegBinaryInsertString "HKCU\${REGKEY}" "${REGVAL}" ${FLAGS} ${OFFSET} "${STRING}"
!endif
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} "  RegBinaryInsertString( HKCU\${REGKEY}[${REGVAL}], ${OFFSET}, ${STRING} ) == $0"
	!undef STRING
	!undef OFFSET
	!undef FLAGS

	; Write 2
	!define STRING "abcdef"
	!define OFFSET 10
	!define FLAGS 0
!ifdef DEBUGGING_ENABLED
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
!else
	NSutils::RegBinaryInsertString "HKCU\${REGKEY}" "${REGVAL}" ${FLAGS} ${OFFSET} "${STRING}"
!endif
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} "  RegBinaryInsertString( HKCU\${REGKEY}[${REGVAL}], ${OFFSET}, ${STRING} ) == $0"
	!undef STRING
	!undef OFFSET
	!undef FLAGS

	; Write 3
	!define STRING "XY"
	!define OFFSET 14
	!define FLAGS 0
!ifdef DEBUGGING_ENABLED
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
!else
	NSutils::RegBinaryInsertString "HKCU\${REGKEY}" "${REGVAL}" ${FLAGS} ${OFFSET} "${STRING}"
!endif
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} "  RegBinaryInsertString( HKCU\${REGKEY}[${REGVAL}], ${OFFSET}, ${STRING} ) == $0"
	!undef STRING
	!undef OFFSET
	!undef FLAGS

	; TODO: Do it automatically
	MessageBox MB_ICONINFORMATION 'Manually verify HKCU\${REGKEY}[${REGVAL}]$\nMust be "...abXYef"'

	DeleteRegValue HKCU "${REGKEY}" "${REGVAL}"
	DeleteRegKey HKCU "${REGKEY}"
	SetRegView 32

	!undef REGKEY
	!undef REGVAL

SectionEnd


Section /o "Test compare files"
	${Print} "--------------------------------------------------------------"
	${Print} "Test compare files"

!ifdef DEBUGGING_ENABLED
	Push "C:\Windows\inf\setupapi.setup.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
!else
	NSutils::CompareFiles /NOUNLOAD "C:\Windows\inf\setupapi.dev.log" "C:\Windows\inf\setupapi.setup.log"
!endif
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.setup.log ) == (BOOL)$0"

!ifdef DEBUGGING_ENABLED
	Push "C:\Windows\inf\setupapi.dev.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
!else
	NSutils::CompareFiles /NOUNLOAD "C:\Windows\inf\setupapi.dev.log" "C:\Windows\inf\setupapi.dev.log"
!endif
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.dev.log ) == (BOOL)$0"

!ifdef DEBUGGING_ENABLED
	Push "C:\Windows\inf\setupapi.invalid.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
!else
	NSutils::CompareFiles /NOUNLOAD "C:\Windows\inf\setupapi.dev.log" "C:\Windows\inf\setupapi.invalid.log"
!endif
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.invalid.log ) == (BOOL)$0"
SectionEnd


!macro CPUID_PRINT_FEATURE _Feature _Reg _Mask
	IntOp $0 ${_Reg} & ${_Mask}
	${If} $0 <> 0
		${Print} '  [*] ${_Feature}'
	${Else}
		${Print} '  [ ] ${_Feature}'
	${EndIf}
!macroend

Section /o "CPUID"
	${Print} "--------------------------------------------------------------"
	${Print} "CPUID tests"

	!define MASK_MMX	0x800000		; EDX1
	!define MASK_SSE	0x2000000		; EDX1
	!define MASK_SSE2	0x4000000		; EDX1
	!define MASK_SSE3	0x1				; ECX1
	!define MASK_SSSE3	0x200			; ECX1
	!define MASK_SSE41	0x80000			; ECX1
	!define MASK_SSE42	0x100000		; ECX1
	!define MASK_3DNOW	0x80000000		; EDX81
	!define MASK_3DNOWX	0x40000000		; EDX81
	!define MASK_LM		0x20000000		; EDX81

	NSutils::CPUID /NOUNLOAD 1
	Pop $1	; EAX
	Pop $2	; EBX
	Pop $3	; ECX
	Pop $4	; EDX

	!insertmacro CPUID_PRINT_FEATURE "MMX" $4 ${MASK_MMX}
	!insertmacro CPUID_PRINT_FEATURE "SSE" $4 ${MASK_SSE}
	!insertmacro CPUID_PRINT_FEATURE "SSE2" $4 ${MASK_SSE2}
	!insertmacro CPUID_PRINT_FEATURE "SSE3" $3 ${MASK_SSE3}
	!insertmacro CPUID_PRINT_FEATURE "Supplemental SSE3" $3 ${MASK_SSSE3}
	!insertmacro CPUID_PRINT_FEATURE "SSE4.1" $3 ${MASK_SSE41}
	!insertmacro CPUID_PRINT_FEATURE "SSE4.2" $3 ${MASK_SSE42}

	NSutils::CPUID /NOUNLOAD 0x80000001
	Pop $1	; EAX
	Pop $2	; EBX
	Pop $3	; ECX
	Pop $4	; EDX

	!insertmacro CPUID_PRINT_FEATURE "3DNow!" $4 ${MASK_3DNOW}
	!insertmacro CPUID_PRINT_FEATURE "Extended 3DNow!" $4 ${MASK_3DNOWX}
	!insertmacro CPUID_PRINT_FEATURE "64BIT" $4 ${MASK_LM}

SectionEnd


Section "-Cleanup"
	; Make sure NSutils is not loaded (in case all previous calls were made with /NOUNLOAD)
	NSutils::DisableProgressStepBack 0	; Dummy call. No effect.
SectionEnd

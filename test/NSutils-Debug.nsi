
!ifdef AMD64
	Target amd64-unicode
!else ifdef ANSI
	Target x86-ansi
!else
	Target x86-unicode	; Default
!endif

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "Win\WinNT.nsh"

# NSutils.dll debug location
!ifdef NSIS_AMD64
	!define NSUTILS "$EXEDIR\..\Debug-amd64-unicode\NSutils.dll"
!else ifdef NSIS_UNICODE
	!define NSUTILS "$EXEDIR\..\Debug-x86-unicode\NSutils.dll"
!else
	!define NSUTILS "$EXEDIR\..\Debug-x86-ansi\NSutils.dll"
!endif

!define /ifndef TRUE 1
!define /ifndef FALSE 0
!define /ifndef ERROR_SUCCESS 0

# GUI settings
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install-nsis.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-nsis.bmp"

SpaceTexts "none"
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_INSTFILES

# Prepare strings
!insertmacro MUI_LANGUAGE "English"

# Installer details
!ifdef NSIS_AMD64
	Name    "NSutils-Debug-amd64-unicode"
	OutFile "NSutils-Debug-amd64-unicode.exe"
!else ifdef NSIS_UNICODE
	Name    "NSutils-Debug-x86-unicode"
	OutFile "NSutils-Debug-x86-unicode.exe"
!else
	Name    "NSutils-Debug-x86-ansi"
	OutFile "NSutils-Debug-x86-ansi.exe"
!endif

XPStyle on
RequestExecutionLevel user ; don't require UAC elevation
ManifestDPIAware true
ManifestSupportedOS all
ShowInstDetails show

!define Print DetailPrint


Function .onInit
FunctionEnd

Function PrintFileVersion

	Pop $R0		; The file name

	${Print} "$R0"

	Push $R0
	CallInstDLL "${NSUTILS}" GetFileVersion
	Pop $0
	${Print} "    FileVersion: $0 ($1,$2,$3,$4)"

	Push $R0
	CallInstDLL "${NSUTILS}" GetProductVersion
	Pop $0
	${Print} "    ProductVersion: $0 ($1,$2,$3,$4)"

	Push "CompanyName"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    CompanyName: $0"

	Push "FileDescription"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    FileDescription: $0"

	Push "FileVersion"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    FileVersion: $0"

	Push "InternalName"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    InternalName: $0"

	Push "LegalCopyright"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    LegalCopyright: $0"

	Push "OriginalFilename"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    OriginalFilename: $0"

	Push "ProductName"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
	Pop $0
	${Print} "    ProductName: $0"

	Push "ProductVersion"
	Push $R0
	CallInstDLL "${NSUTILS}" GetVersionInfoString
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
		${Print} "    Step $R0/10 (default behavior, the progress bar is stepping back)"
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

	Push $mui.InstFilesPage.ProgressBar
	CallInstDLL "${NSUTILS}" DisableProgressStepBack
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
	Push $mui.InstFilesPage.ProgressBar
	CallInstDLL "${NSUTILS}" RestoreProgressStepBack

SectionEnd


Section /o "Test PendingFileRenameOperations (requires Admin)"

	${Print} "--------------------------------------------------------------"

	System::Call 'kernel32::CopyFile( t "$SYSDIR\Notepad.exe", t "$DESKTOP\MyNotepad.exe", i 0 ) i.r0'
	${Print} 'CopyFile( "Notepad.exe", "DESKTOP\MyNotepad.exe" ) == $0'

	!define MOVEFILE_DELAY_UNTIL_REBOOT 0x4
	System::Call 'kernel32::MoveFileEx( t "$DESKTOP\MyNotepad.exe", t "$DESKTOP\MyNotepad2.exe", i ${MOVEFILE_DELAY_UNTIL_REBOOT} ) i.r0'
	${Print} 'MoveFileEx( "DESKTOP\MyNotepad.exe", "DESKTOP\MyNotepad2.exe", MOVEFILE_DELAY_UNTIL_REBOOT ) == $0'

	Push "$EXEDIR\PendingFileRename.log"
	Push "MyNotepad"
	CallInstDLL "${NSUTILS}" ExecutePendingFileRenameOperations
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
	Push $R0
	CallInstDLL "${NSUTILS}" FindPendingFileRenameOperations
	Pop $0
	${Print} 'FindPendingFileRenameOperations( "$R0" ) == "$0"'


	StrCpy $R0 "*"		; Substring to find
	Push $R0
	CallInstDLL "${NSUTILS}" FindPendingFileRenameOperations
	Pop $0
	${Print} 'FindPendingFileRenameOperations( "$R0" ) == "$0"'

SectionEnd


Section /o "Test string table manipulation"

	${Print} "--------------------------------------------------------------"

	System::Call 'kernel32::CopyFile( t "$EXEPATH", t "$DESKTOP\MyTest.exe", i 0 ) i.r0'
	${Print} 'CopyFile( "$EXEPATH", "DESKTOP\MyTest.exe" ) == $0'

	Push 1033
	Push 100
	Push "$DESKTOP\MyUser32.dll"
	CallInstDLL "${NSUTILS}" ReadResourceString
	Pop $0
	${If} $0 == ""
		${Print} 'String #10: "$0". Ok!'
	${Else}
		${Print} 'String #10: "$0". Should have been empty!'
	${EndIf}

	Push "Dela beat cârciumă vin / Merg pe gard, de drum mă țin"
	Push 1033
	Push 100
	Push "$DESKTOP\MyTest.exe"
	CallInstDLL "${NSUTILS}" WriteResourceString
	Pop $0
	${If} $0 = ${FALSE}
		StrCpy $0 "ERROR"
	${Else}
		StrCpy $0 "SUCCESS"
	${EndIf}
	${Print} 'Write #100: $0'

	Push 1033
	Push 100
	Push "$DESKTOP\MyTest.exe"
	CallInstDLL "${NSUTILS}" ReadResourceString
	Pop $0
	${If} $0 != ""
		${Print} 'String #100: "$0". Ok!'
	${Else}
		${Print} 'String #100: "". Should have been valid!'
	${EndIf}

	Push ""
	Push 1033
	Push 100
	Push "$DESKTOP\MyTest.exe"
	CallInstDLL "${NSUTILS}" WriteResourceString
	Pop $0
	${If} $0 = ${FALSE}
		StrCpy $0 "ERROR"
	${Else}
		StrCpy $0 "SUCCESS"
	${EndIf}
	${Print} 'Delete #100: $0'

	Push 1033
	Push 100
	Push "$DESKTOP\MyTest.exe"
	CallInstDLL "${NSUTILS}" ReadResourceString
	Pop $0
	${If} $0 == ""
		${Print} 'String #10: "$0". Ok!'
	${Else}
		${Print} 'String #10: "$0". Should have been empty!'
	${EndIf}

	Delete "$DESKTOP\MyTest.exe"

SectionEnd


Section /o "Test close file handles"

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
	System::Call 'kernel32::CreateFile( t "${TESTFILE}", i ${GENERIC_READ}, i ${FILE_SHARE_READ}, p 0, i ${OPEN_ALWAYS}, i ${FILE_ATTRIBUTE_NORMAL}, p 0 ) p.r10 ? e'
	Pop $0	; GetLastError
	${If} $R0 p<> ${INVALID_HANDLE_VALUE}
		StrCpy $0 0
	${EndIf}
	${Print} 'CreateFile( #1, TESTFILE ) = $0'

	; Open test file (handle2)
	System::Call 'kernel32::CreateFile( t "${TESTFILE}", i ${GENERIC_READ}, i ${FILE_SHARE_READ}, p 0, i ${OPEN_ALWAYS}, i ${FILE_ATTRIBUTE_NORMAL}, p 0 ) p.r11 ? e'
	Pop $0	; GetLastError
	${If} $R1 p<> ${INVALID_HANDLE_VALUE}
		StrCpy $0 0
	${EndIf}
	${Print} 'CreateFile( #2, TESTFILE ) = $0'

	; ----------------------------------

	${Print} 'Close TESTFILE file handles'
	Push "${TESTFILE}"
	CallInstDLL "${NSUTILS}" CloseFileHandles
	Pop $0
	${Print} '  $0 handles closed'

	; ----------------------------------

	; NOTE: CloseHandle() will raise a first-chance exception when called with invalid (already closed) handles

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
	Push ""
	Push "ccc"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzInsertAfter
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ccc" after "" ) = $0'

	Push "ccc"
	Push "bbb"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzInsertBefore
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "bbb" before "ccc" ) = $0'

	Push "ccc"
	Push "ddd"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzInsertAfter
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ddd" after "ccc" ) = $0'

	Push 0
	Push "aaa"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzInsertAtIndex
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "ddd" at index 0 ) = $0'

	Push 4
	Push "eee"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzInsertAtIndex
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzInsert( "eee" at index 4 ) = $0'

	; Verify
	${For} $1 0 1000
		Push $1
		Push 0
		Push "MyValue"
		Push "HKCU\Software\MyCompany"
		CallInstDLL "${NSUTILS}" RegMultiSzRead
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
	Push ${TRUE}
	Push "ccc"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "ccc" ) = $0'

	Push ${TRUE}
	Push "bbb"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "bbb" ) = $0'

	Push ${TRUE}
	Push "aaa"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "aaa" ) = $0'

	Push ${TRUE}
	Push "ddd"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "ddd" ) = $0'

	Push ${TRUE}
	Push "eee"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
	Pop $0
	IntFmt $0 "0x%x" $0
	${Print} '  RegMultiSzDelete( "eee" ) = $0'

	Push ${TRUE}
	Push "xxx"
	Push 0
	Push "MyValue"
	Push "HKCU\Software\MyCompany"
	CallInstDLL "${NSUTILS}" RegMultiSzDelete
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
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
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
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
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
	Push "${STRING}"
	Push ${OFFSET}
	Push ${FLAGS}
	Push "${REGVAL}"
	Push "HKCU\${REGKEY}"
	CallInstDLL "${NSUTILS}" RegBinaryInsertString
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

	Push "C:\Windows\inf\setupapi.setup.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.setup.log ) == (BOOL)$0"

	Push "C:\Windows\inf\setupapi.dev.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.dev.log ) == (BOOL)$0"

	Push "C:\Windows\inf\setupapi.invalid.log"
	Push "C:\Windows\inf\setupapi.dev.log"
	CallInstDLL "${NSUTILS}" CompareFiles
	Pop $0
	${Print} "  CompareFiles ( setupapi.dev.log, setupapi.invalid.log ) == (BOOL)$0"
SectionEnd


Section /o "Test SSD"
	${Print} "--------------------------------------------------------------"
	${Print} "Test SSD"

	StrCpy $9 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	${For} $R0 0 25
		StrCpy $R1 $9 1 $R0

		Push "$R1:"
		CallInstDLL "${NSUTILS}" DriveIsSSD
		Pop $0
		${Print} "  DriveIsSSD ( $R1: ) == (BOOL)$0"

	${Next}

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

	Push 1
	CallInstDLL "${NSUTILS}" CPUID
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

	Push 0x80000001
	CallInstDLL "${NSUTILS}" CPUID
	Pop $1	; EAX
	Pop $2	; EBX
	Pop $3	; ECX
	Pop $4	; EDX

	!insertmacro CPUID_PRINT_FEATURE "3DNow!" $4 ${MASK_3DNOW}
	!insertmacro CPUID_PRINT_FEATURE "Extended 3DNow!" $4 ${MASK_3DNOWX}
	!insertmacro CPUID_PRINT_FEATURE "64BIT" $4 ${MASK_LM}

SectionEnd

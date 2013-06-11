@echo off
SetLocal
set BUILD_SUCCESSFUL=0

if defined PROGRAMFILES(X86) set PF=%PROGRAMFILES(X86)%
if not defined PROGRAMFILES(X86) set PF=%PROGRAMFILES%
set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat

if not exist "%VCVARSALL%" echo ERROR: Can't find Visual C++ 2012 && pause && goto :EOF

call "%VCVARSALL%" x86

if not exist "%~dp0\ReleaseW" mkdir "%~dp0\ReleaseW"
if not exist "%~dp0\ReleaseW\temp" mkdir "%~dp0\ReleaseW\temp"

set CL=/nologo /O1 /Ob2 /Os /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_WINDLL" /D "_UNICODE" /D "UNICODE" /GF /FD /MT /LD /GS- /Fo".\ReleaseW\temp\\" /Fd".\ReleaseW\temp\\" /Fe".\ReleaseW\NSutils" /W3
set LINK=/OUT:"NSutils.dll" /INCREMENTAL:NO /MANIFEST:NO /NODEFAULTLIB /ENTRY:"DllMain" kernel32.lib user32.lib version.lib advapi32.lib shlwapi.lib
cl.exe "main.c" "verinfo.c" "utils.c" "nsiswapi\pluginapi.c" && set BUILD_SUCCESSFUL=1

if %BUILD_SUCCESSFUL%==1 (
	echo Success!
	rem pause
) else (
	pause
)
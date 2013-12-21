@echo off
SetLocal

set OUTDIR=ReleaseA
set OUTNAME=NSutils

set BUILD_SUCCESSFUL=0

if defined PROGRAMFILES(X86) set PF=%PROGRAMFILES(X86)%
if not defined PROGRAMFILES(X86) set PF=%PROGRAMFILES%

set VCVARSALL=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

echo ERROR: Can't find Visual Studio 2010/2012
pause
goto :EOF

:BUILD
call "%VCVARSALL%" x86

if not exist "%~dp0\%OUTDIR%" mkdir "%~dp0\%OUTDIR%"
if not exist "%~dp0\%OUTDIR%\temp" mkdir "%~dp0\%OUTDIR%\temp"

set CL=/nologo /O1 /Ob2 /Os /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_WINDLL" /D "_MBCS" /GF /FD /MT /LD /GS- /Fo".\%OUTDIR%\temp\\" /Fd".\%OUTDIR%\temp\\" /Fe".\%OUTDIR%\%OUTNAME%" /W3
set LINK=/INCREMENTAL:NO /MANIFEST:NO /NODEFAULTLIB /ENTRY:"DllMain" kernel32.lib user32.lib version.lib advapi32.lib shlwapi.lib gdi32.lib ole32.lib uuid.lib oleaut32.lib msimg32.lib ".\%OUTDIR%\temp\NSutils.res"
rc.exe /Fo ".\%OUTDIR%\temp\NSutils.res" "NSutils.rc"
cl.exe "main.c" "verinfo.c" "utils.c" "strblock.c" "gdi.c" "nsiswapi\pluginapi.c" && set BUILD_SUCCESSFUL=1

if %BUILD_SUCCESSFUL%==1 (
	echo Success!
	rem pause
) else (
	pause
)
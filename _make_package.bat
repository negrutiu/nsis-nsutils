@echo off

cd /d "%~dp0"

if not exist Release-mingw-amd64-unicode\NSutils.dll	echo ERROR: Missing Release-mingw-amd64-unicode\NSutils.dll && pause && exit /B 2
if not exist Release-mingw-x86-ansi\NSutils.dll			echo ERROR: Missing Release-mingw-x86-ansi\NSutils.dll      && pause && exit /B 2
if not exist Release-mingw-x86-unicode\NSutils.dll		echo ERROR: Missing Release-mingw-x86-unicode\NSutils.dll   && pause && exit /B 2

set Z7=%PROGRAMFILES%\7-Zip\7z.exe
if not exist "%Z7%" echo ERROR: Missing %Z7% && pause && exit /B 2

REM :: Read version from the .rc file
for /f usebackq^ tokens^=3^ delims^=^"^,^  %%f in (`type NSutils.rc ^| findstr /r /c:"\s*\"FileVersion\"\s*"`) do set RCVER=%%f

rmdir /S /Q _Package > NUL 2> NUL
mkdir _Package
mkdir _Package\amd64-unicode
mkdir _Package\x86-unicode
mkdir _Package\x86-ansi

mklink /H _Package\amd64-unicode\NSutils.dll		Release-mingw-amd64-unicode\NSutils.dll
mklink /H _Package\x86-unicode\NSutils.dll			Release-mingw-x86-unicode\NSutils.dll
mklink /H _Package\x86-ansi\NSutils.dll				Release-mingw-x86-ansi\NSutils.dll
mklink /H _Package\NSutils.Readme.txt				NSutils.Readme.txt

pushd _Package
"%Z7%" a "..\NSutils-%RCVER%.7z" * -r
popd

echo.
pause

rmdir /S /Q _Package > NUL 2> NUL

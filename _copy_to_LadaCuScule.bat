@echo off
set DST=%~dp0\..\LadaCuScule
set SRCA=%~dp0\ReleaseA-mingw
set SRCW=%~dp0\ReleaseW-mingw

if not exist "%DST%\NSIS.extra\NSutils" mkdir "%DST%\NSIS.extra\NSutils"
echo on
copy /Y "%SRCA%\NSutils.dll" "%DST%\NSIS.extra\NSutils"
copy /Y "%SRCA%\NSutils.pdb" "%DST%\NSIS.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSIS.extra\NSutils"
@echo off

if not exist "%DST%\NSISw.extra\NSutils" mkdir "%DST%\NSISw.extra\NSutils"
echo on
copy /Y "%SRCW%\NSutils.dll" "%DST%\NSISw.extra\NSutils"
copy /Y "%SRCW%\NSutils.pdb" "%DST%\NSISw.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSISw.extra\NSutils"
@echo off

pause
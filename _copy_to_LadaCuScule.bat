@echo off

REM :: Note: mingw binaries are the most backward compatible (NT4+)
set DST=%~dp0\..\..\LadaCuScule
set SRCA=%~dp0\ReleaseA-mingw
set SRCW=%~dp0\ReleaseW-mingw
set SRC64=%~dp0\ReleaseW-mingw-amd64


set DSTA=%DST%\NSIS.extra\NSutils
mkdir "%DSTA%" 2> NUL
echo on
	copy "%SRCA%\NSutils.dll" "%DSTA%"
	copy "%SRCA%\NSutils.pdb" "%DSTA%"
	copy "%~dp0\NSutils.Readme.txt" "%DSTA%"
@echo off

set DSTW=%DST%\NSISw.extra\NSutils
mkdir "%DSTW%" 2> NUL
echo on
	copy "%SRCW%\NSutils.dll" "%DSTW%"
	copy "%SRCW%\NSutils.pdb" "%DSTW%"
	copy "%~dp0\NSutils.Readme.txt" "%DSTW%"
@echo off

set DST64=%DST%\NSISamd64.extra\NSutils
mkdir "%DST64%" 2> NUL
echo on
	copy "%SRC64%\NSutils.dll" "%DST64%"
	copy "%SRC64%\NSutils.pdb" "%DST64%"
	copy "%~dp0\NSutils.Readme.txt" "%DST64%"
@echo off

pause
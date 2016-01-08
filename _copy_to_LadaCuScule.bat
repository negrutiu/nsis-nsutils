@set DST=%~dp0\..\LadaCuScule
@set SRCA=%~dp0\ReleaseA-nocrt
@set SRCW=%~dp0\ReleaseW-nocrt

@if not exist "%DST%\NSIS.extra\NSutils" mkdir "%DST%\NSIS.extra\NSutils"
copy /Y "%SRCA%\NSutils.dll" "%DST%\NSIS.extra\NSutils"
copy /Y "%SRCA%\NSutils.pdb" "%DST%\NSIS.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSIS.extra\NSutils"

@if not exist "%DST%\NSISw.extra\NSutils" mkdir "%DST%\NSISw.extra\NSutils"
copy /Y "%SRCW%\NSutils.dll" "%DST%\NSISw.extra\NSutils"
copy /Y "%SRCW%\NSutils.pdb" "%DST%\NSISw.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSISw.extra\NSutils"

pause
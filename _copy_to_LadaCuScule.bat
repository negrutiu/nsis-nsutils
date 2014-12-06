@set DST=%~dp0\..\LadaCuScule

@if not exist "%DST%\NSIS.extra\NSutils" mkdir "%DST%\NSIS.extra\NSutils"
copy /Y "%~dp0\ReleaseA\NSutils.dll" "%DST%\NSIS.extra\NSutils"
copy /Y "%~dp0\ReleaseA\NSutils.pdb" "%DST%\NSIS.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSIS.extra\NSutils"

@if not exist "%DST%\NSISw.extra\NSutils" mkdir "%DST%\NSISw.extra\NSutils"
copy /Y "%~dp0\ReleaseW\NSutils.pdb" "%DST%\NSISw.extra\NSutils"
copy /Y "%~dp0\NSutils.Readme.txt" "%DST%\NSIS.extra\NSutils"

pause
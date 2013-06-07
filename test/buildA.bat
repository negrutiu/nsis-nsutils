@rem NSIS_PATH must point to the NSIS installation folder (usually C:\Program Files\NSIS)
@set NSIS_PATH=%~dp0\..\..\..\NSIS
"%NSIS_PATH%\makensis.exe" "%~dp0\test.nsi"
@if %ERRORLEVEL% neq 0 pause
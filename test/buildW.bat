:: NSIS3 (or newer) is required
@echo off
set NSIS_PATH=%~dp0\..\..\NSISbin
"%NSIS_PATH%\makensis.exe" /v4 "%~dp0\test.nsi"
if %ERRORLEVEL% neq 0 pause

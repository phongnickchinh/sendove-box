@echo off
setlocal

set "ROOT=%~dp0"

echo Starting Sendlove services...
echo.

start "Sendlove Backend" cmd /k "cd /d ""%ROOT%sendlove_backend"" && npm run serve"
start "Sendlove Web" cmd /k "cd /d ""%ROOT%sendlove_web"" && npm run dev"

echo Backend and web services are starting in separate windows.
echo You can close this launcher window now.

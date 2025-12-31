@echo off
pushd "%~dp0" >nul
set "PYTHONPATH=%~dp0devops;%PYTHONPATH%"
py -3 -m devtool.cli %*
set EXITCODE=%ERRORLEVEL%
popd >nul
exit /b %EXITCODE%

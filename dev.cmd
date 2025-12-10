@echo off
pushd "%~dp0" >nul
py -3 -m devtool.cli %*
set EXITCODE=%ERRORLEVEL%
popd >nul
exit /b %EXITCODE%

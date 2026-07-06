@echo off
set ROOT_DIR=%~dp0..
set BUILD_DIR=%ROOT_DIR%\build

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

pushd "%ROOT_DIR%"
"%BUILD_DIR%\Release\ai_on_a_stick.exe"

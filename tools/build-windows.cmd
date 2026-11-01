@echo off
rem Builds the client with the "ninja" preset from a plain command prompt.
rem Usage: tools\build-windows.cmd [Release|Debug] [install dir]
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
set PATH=D:\dev\tools\ninja;%PATH%
rem vcpkg's WinHTTP downloader wants host:port, without the scheme.
if defined HTTPS_PROXY set HTTPS_PROXY=%HTTPS_PROXY:http://=%
if defined HTTP_PROXY set HTTP_PROXY=%HTTP_PROXY:http://=%
set VCPKG_DEFAULT_BINARY_CACHE=D:\dev\vcpkg-cache
if not exist "%VCPKG_DEFAULT_BINARY_CACHE%" mkdir "%VCPKG_DEFAULT_BINARY_CACHE%"
cd /d "%~dp0.."
if not exist vcpkg\vcpkg.exe call vcpkg\bootstrap-vcpkg.bat -disableMetrics || exit /b 1
if not "%~2"=="" (set INSTALL_ARG=-DNEXTCLIENT_INSTALL_DIR=%~2) else (set INSTALL_ARG=)
if not exist build\ninja\CMakeCache.txt (cmake --preset ninja %INSTALL_ARG% || exit /b 1)
cmake --build build\ninja --config %CONFIG% --target BUILD_ALL

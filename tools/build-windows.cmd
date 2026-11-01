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
rem Downloads go through tools\vcpkg-fetch.ps1 (.NET), which works behind HTTP proxies where
rem vcpkg's WinHTTP downloader does not.
set X_VCPKG_ASSET_SOURCES=x-script,powershell -NoProfile -ExecutionPolicy Bypass -File %~dp0vcpkg-fetch.ps1 {url} {dst}
set VCPKG_DEFAULT_BINARY_CACHE=D:\dev\vcpkg-cache
if not exist "%VCPKG_DEFAULT_BINARY_CACHE%" mkdir "%VCPKG_DEFAULT_BINARY_CACHE%"
cd /d "%~dp0.."
if not exist vcpkg\vcpkg.exe call vcpkg\bootstrap-vcpkg.bat -disableMetrics || exit /b 1
rem Arguments are quoted: install and CEF paths may contain spaces.
cmake --preset ninja || exit /b 1
if not "%~2"=="" (cmake build\ninja "-DNEXTCLIENT_INSTALL_DIR=%~2" || exit /b 1)
rem EPICENTRE_CEF_ROOT: an extracted CEF 150 windows32 minimal; empty = download during configure
if defined EPICENTRE_CEF_ROOT (cmake build\ninja "-DEPICENTRE_CEF_ROOT=%EPICENTRE_CEF_ROOT%" || exit /b 1)
cmake --build build\ninja --config %CONFIG% --target BUILD_ALL

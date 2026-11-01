@echo off
rem Standalone build + unit tests of nextclient\chromehtml (no vcpkg needed).
rem Usage: tools\build-chromehtml-tests.cmd <extracted CEF 150 windows32 minimal dir>
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
set PATH=D:\dev\tools\ninja;%PATH%
cd /d "%~dp0.."
if not exist build\chromehtml\CMakeCache.txt (cmake -S nextclient\chromehtml -B build\chromehtml -G Ninja -DCMAKE_BUILD_TYPE=Release "-DEPICENTRE_CEF_ROOT=%~1" || exit /b 1)
cmake --build build\chromehtml || exit /b 1
build\chromehtml\chromehtml-tests.exe

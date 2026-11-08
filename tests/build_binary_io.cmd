@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /I include tests\binary_io.cpp /Fo:build\ /Fe:build\binary_io_test.exe
exit /b %errorlevel%

@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\smooth-lighting mkdir build\smooth-lighting
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /UNDEBUG /I include tests\smooth_lighting.cpp src\SmoothLighting.cpp /Fo:build\smooth-lighting\ /Fe:build\smooth-lighting\smooth_lighting_test.exe
exit /b %errorlevel%

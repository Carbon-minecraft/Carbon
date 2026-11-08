@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\smooth-lighting mkdir build\smooth-lighting
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /I include /I third_party src\*.cpp /Fo:build\smooth-lighting\ /Fe:build\smooth-lighting\carbon_smooth_lighting.exe /link opengl32.lib windowscodecs.lib ole32.lib user32.lib gdi32.lib ws2_32.lib
exit /b %errorlevel%

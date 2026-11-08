@echo off
setlocal
cd /d "%~dp0\.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /I include /I third_party tests\render_smoke.cpp src\ClientRenderer.cpp src\BlockModels.cpp src\PlayableWorld.cpp src\GuiItemRenderer.cpp src\SmoothLighting.cpp /Fo:build\ /Fe:build\render_smoke.exe /link opengl32.lib windowscodecs.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
build\render_smoke.exe ..\JavaMinecraft\decompiled build\render-smoke.ppm
exit /b %errorlevel%



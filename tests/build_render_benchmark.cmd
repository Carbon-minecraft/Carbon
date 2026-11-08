@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\render-benchmark mkdir build\render-benchmark
cl /nologo /std:c++20 /EHsc /W4 /permissive- /O2 /fp:strict /I include /I third_party tests\render_smoke.cpp src\ClientRenderer.cpp src\BlockModels.cpp src\PlayableWorld.cpp src\GuiItemRenderer.cpp src\SmoothLighting.cpp /Fo:build\render-benchmark\ /Fe:build\render-benchmark\render_benchmark.exe /link opengl32.lib windowscodecs.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
build\render-benchmark\render_benchmark.exe ..\JavaMinecraft\decompiled build\render-benchmark\frame.ppm
exit /b %errorlevel%

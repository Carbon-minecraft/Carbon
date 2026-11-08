@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /UNDEBUG /I include /I third_party tests\gui_item_renderer.cpp src\GuiItemRenderer.cpp /Fo:build\ /Fe:build\gui_item_renderer_test.exe
if errorlevel 1 exit /b %errorlevel%
build\gui_item_renderer_test.exe ..\JavaMinecraft\decompiled
exit /b %errorlevel%

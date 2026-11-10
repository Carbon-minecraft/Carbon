@echo off
setlocal
cd /d "%~dp0\.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /UNDEBUG /I include /I third_party tests\render_geometry.cpp src\ClientRenderer.cpp src\BlockModels.cpp src\PlayableWorld.cpp src\GuiItemRenderer.cpp src\SmoothLighting.cpp src\SourceFluidRenderer.cpp src\SeededOverworldChunkProvider.cpp src\OverworldBiomeSource.cpp src\AquiferTerrain.cpp src\BlendedTerrainNoise.cpp src\OverworldDensity.cpp src\OverworldNoise.cpp src\OverworldSurfaceRules.cpp src\LegacyRandomSource.cpp /Fo:build\ /Fe:build\render_geometry_test.exe /link opengl32.lib windowscodecs.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
build\render_geometry_test.exe ..\JavaMinecraft\decompiled
exit /b %errorlevel%

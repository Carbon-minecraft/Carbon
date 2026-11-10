@echo off
setlocal
cd /d "%~dp0\.."
if not defined VCToolsInstallDir call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /W4 /permissive- /fp:strict /I include /I third_party tests\stairs_block.cpp src\PlayableWorld.cpp src\SeededOverworldChunkProvider.cpp src\OverworldBiomeSource.cpp src\AquiferTerrain.cpp src\BlendedTerrainNoise.cpp src\OverworldDensity.cpp src\OverworldNoise.cpp src\OverworldSurfaceRules.cpp src\LegacyRandomSource.cpp /Fo:build\ /Fe:build\stairs_block_test.exe
if errorlevel 1 exit /b %errorlevel%
build\stairs_block_test.exe

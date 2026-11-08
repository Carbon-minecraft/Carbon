cmake_minimum_required(VERSION 3.20)
project(Carbon VERSION 0.1.0 LANGUAGES CXX)
add_library(carbon_core
  src/LegacyRandomSource.cpp
  src/Identifier.cpp
  src/DirectoryResources.cpp
  src/BitStorage.cpp
  src/DataLayer.cpp
  src/LevelHeightAccessor.cpp
  src/NbtIO.cpp
  src/PlayableWorld.cpp
  src/ClientRenderer.cpp
  src/BlockModels.cpp
  src/GuiItemRenderer.cpp
  src/SmoothLighting.cpp
  src/Multiplayer.cpp
  src/AquiferTerrain.cpp
  src/BlendedTerrainNoise.cpp
  src/OverworldDensity.cpp
  src/OverworldNoise.cpp
  src/OverworldSurfaceRules.cpp
)
target_include_directories(carbon_core PUBLIC include third_party)
target_compile_features(carbon_core PUBLIC cxx_std_20)
if(WIN32)
  target_link_libraries(carbon_core PUBLIC opengl32 windowscodecs ole32 user32 gdi32 ws2_32)
endif()
if(MSVC)
  target_compile_options(carbon_core PUBLIC /W4 /permissive- /fp:strict)
else()
  target_compile_options(carbon_core PUBLIC -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
add_executable(carbon src/main.cpp)
target_link_libraries(carbon PRIVATE carbon_core)
target_compile_definitions(carbon PRIVATE CARBON_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
if(WIN32)
  target_link_libraries(carbon PRIVATE opengl32 windowscodecs ole32 user32 gdi32)
endif()

include(CTest)
if(BUILD_TESTING)
  foreach(component IN ITEMS random_sources coordinates binary_io nbt math resources bit_storage nbt_io geometry direction noise palette integration playable_world client_controls creative_hotbar light_storage terrain_noise_stack paletted_container scheduled_ticks block_models chunk_sections advanced_noise state_properties voxel_shapes hit_placement render_geometry slab_block gui_item_renderer fence_gameplay smooth_lighting xoroshiro_worldgen_random aquifer_terrain blended_terrain_noise overworld_density overworld_noise)
    add_executable(${component}_test tests/${component}.cpp)
    target_link_libraries(${component}_test PRIVATE carbon_core)
    # These tests use assert; keep checks active in Release builds as well.
    if(MSVC)
      target_compile_options(${component}_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(${component}_test PRIVATE -UNDEBUG)
    endif()
  endforeach()
  add_test(NAME random_sources COMMAND random_sources_test)
  add_test(NAME coordinates COMMAND coordinates_test)
  add_test(NAME binary_io COMMAND binary_io_test)
  add_test(NAME nbt COMMAND nbt_test)
  add_test(NAME math COMMAND math_test)
  add_test(NAME resources COMMAND resources_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME bit_storage COMMAND bit_storage_test)
  add_test(NAME nbt_io COMMAND nbt_io_test)
  add_test(NAME geometry COMMAND geometry_test)
  add_test(NAME direction COMMAND direction_test)
  add_test(NAME noise COMMAND noise_test)
  add_test(NAME palette COMMAND palette_test)
  add_test(NAME integration COMMAND integration_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME playable_world COMMAND playable_world_test)
  add_test(NAME client_controls COMMAND client_controls_test)
  add_test(NAME creative_hotbar COMMAND creative_hotbar_test)
  add_test(NAME light_storage COMMAND light_storage_test)
  add_test(NAME terrain_noise_stack COMMAND terrain_noise_stack_test)
  add_test(NAME paletted_container COMMAND paletted_container_test)
  add_test(NAME scheduled_ticks COMMAND scheduled_ticks_test)
  add_test(NAME block_models COMMAND block_models_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME chunk_sections COMMAND chunk_sections_test)
  add_test(NAME advanced_noise COMMAND advanced_noise_test)
  add_test(NAME xoroshiro_worldgen_random COMMAND xoroshiro_worldgen_random_test)
  add_test(NAME state_properties COMMAND state_properties_test)
  add_test(NAME voxel_shapes COMMAND voxel_shapes_test)
  add_test(NAME hit_placement COMMAND hit_placement_test)
  add_test(NAME render_geometry COMMAND render_geometry_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME slab_block COMMAND slab_block_test)
  add_test(NAME gui_item_renderer COMMAND gui_item_renderer_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME fence_gameplay COMMAND fence_gameplay_test)
  add_test(NAME smooth_lighting COMMAND smooth_lighting_test)
  add_test(NAME aquifer_terrain COMMAND aquifer_terrain_test)
  add_test(NAME blended_terrain_noise COMMAND blended_terrain_noise_test)
  add_test(NAME overworld_density COMMAND overworld_density_test "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled")
  add_test(NAME overworld_noise COMMAND overworld_noise_test)
  if(WIN32)
    add_executable(multiplayer_test tests/multiplayer.cpp)
    target_link_libraries(multiplayer_test PRIVATE carbon_core ws2_32)
    if(MSVC)
      target_compile_options(multiplayer_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(multiplayer_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME multiplayer COMMAND multiplayer_test)
  endif()
  if(WIN32)
    add_executable(render_smoke tests/render_smoke.cpp)
    target_link_libraries(render_smoke PRIVATE carbon_core)
    add_test(NAME render_smoke COMMAND render_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../JavaMinecraft/decompiled" "${CMAKE_CURRENT_BINARY_DIR}/render-smoke.ppm")
  endif()
endif()






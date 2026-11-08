# Carbon

Native C++ port targeting the supplied **Minecraft Java Edition 26.3** source.
Java Edition behavior, saves, protocol, resource formats and mechanics are the
compatibility target. All resources come directly from
`../JavaMinecraft/decompiled`; there is no replacement asset pack.

## Current state

This is a buildable native port foundation with a **limited playable client slice**. It is not a complete port.
The reference tree contains 7,301 Java source files. Implemented so far:

- `LegacyRandomSource`, primitive `BitRandomSource` operations and
  `MarsagliaPolarGaussian`, with Java integer wraparound and signed `nextLong`.
- Legacy positional seed calculation, UTF-16 Java string hashing and positional
  factory methods.
- Native command-line diagnostics reading original version metadata and inventorying
  original resource and data files in place.
- Win32/WGL first-person client using original block PNGs and HUD sprites, WIC texture
  decoding, and a source-backed Java `CLASSIC_FLAT` base with grounded creative walking,
  jumping, falling, landing, sprinting, crouching, step-up and block collision. Creative
  flight is available through Java's double-tap jump toggle. The client also supports
  nine-slot hotbar selection, supported-block pick-block, breaking and placement.
  Source block models render supported source faces and supported dry slab/stair
  variants plus oak-fence states with source-based collision and placement. Hotbar
  icons use original item JSON, inherited GUI transforms, and 3D source model geometry.
- Additional single-threaded RNG and xoroshiro generator core; integer coordinate
  packing; selected math helpers and Vec3 arithmetic; source-backed AABB
  construction, bounds operations and segment clipping; binary IO; NBT values
  and binary IO; packed integer storage; identifier parsing and guarded
  original-directory resource access; source-backed direction/axis/rotation and
  mirror operations; simplex noise and octave stacks; block palettes and
  palette-backed storage; lazy light-layer storage and build-height indexing;
  and a source-derived scheduled tick queue data subset; chunk section and heightmap
  storage/lifecycle helpers, advanced terrain noise, block state properties,
  voxel-shape boolean/collision operations, and a resource block-model loader and model renderer.

See [PORT_STATUS.md](PORT_STATUS.md) for tested scope and remaining work.

This client is an intentionally narrow slice, not full Java parity. It has no
registry bootstrap/reload, generated structures or light propagation, entities, survival,
full inventory, disk saves, sounds, online authentication, gameplay networking,
integrated server or full set of Java screens. Its native UI includes title, world
selection, flat-world creation, multiplayer pregame, and pause screens. World data is
kept only for the running session. Multiplayer supports server status ping and an
offline-profile login attempt, but stops when a server requires encryption,
authentication, or configuration/gameplay handoff. The hotbar
contains only supported block items; remaining slots are empty, and pick-block carries
block-item identity only. The current item loader handles the `minecraft:model`
item-model form and source GUI transforms; alternate item-model dispatch and full
inventory data are not implemented.
The client uses the supplied Java `CLASSIC_FLAT` layer stack and Plains grass color,
with the flat base represented procedurally and local edits held in memory. The
renderer resolves supplied model parents, textures, faces and blockstate variants
for supported blocks, including dry stone slab, stair and oak-fence states. Blockstate
quarter turns, UV locking, position-seeded weighted alternatives, multipart model
composition and rotated model elements are supported in the exercised subset. The HUD
uses directional face shading for 3D item icons; world AO and full world lighting,
fluids and animated/transparent geometry are not implemented.
RNG translation is a prerequisite, not an implementation of general world generation.

## Play the native slice

Run `build\\carbon.exe` (or `build.cmd` first) from this directory. The default
launch opens the native client. Controls: **WASD** walk, **Space** jump, double-tap
**Space** to toggle creative flight, **Left Shift** crouch or descend while flying,
**Left Ctrl** sprint, mouse to look, **1–9 / wheel** select a block,
middle click to pick a supported block item, left click to break, right click to place,
and **Esc** to release the mouse or resume.
`--diagnostics` runs the noninteractive bootstrap check used by `tests/run_all.py`;
`--random-vectors <seed>` remains available for RNG parity checks.

## Renderer performance sample

`tests\build_render_benchmark.cmd` builds the fixed-scene WGL benchmark with
MSVC `/O2 /fp:strict`. The 2026-11-07 run used a hidden 960×540 WGL window on
Intel Iris Plus Graphics (GL 4.6.0, driver 31.0.101.2140). At legacy radius 24,
after 8 warmup frames, 40 unchanged-scene frames measured CPU render-through-
`SwapBuffers` p50/p95 of 0.682/0.799 ms; camera turns measured 0.677/0.722 ms.
The harness compiled 67,349 source quads, culled 61,363 source-cullface quads,
and submitted the remaining 5,986 through 47 world draw calls. It also checks
the hotbar, 3D item icons, crosshair, block edits, and WGL framebuffer pixels.

The client keeps a complete 5×5 chunk-section window around the player’s current
16-block section. This covers the existing radius-24 view as the player moves
within that section. The sample measured 1.423/3.882 ms p50/p95 during within-
window movement; crossing into the next section rebuilt its new edge meshes in
72.108 ms after 2.405 ms of section-window extraction. Meshes use source model
variants, section/neighbor/sky fingerprints, per-state cull support caching,
and OpenGL display-list batches grouped by texture. Resource JSON and selected
local geometry are memoized for the active resource root.

These are CPU timings through the WGL swap call from a hidden test window, not
GPU-completion measurements or visible-play-session FPS. This OpenGL 1.1
compatible path does not request GPU timer queries. The older, uninstrumented
smoke binary took about 31 seconds for 32 presents plus setup and two captures;
it had no per-frame timings, so that wall total is not a direct p50/p95
comparison. This renderer result is a measured native slice, not a Sodium FPS
claim. Full light propagation, terrain generation and most Java systems remain
unimplemented.

## Build and validate on this machine

From this directory:

```powershell
.\build.cmd
.\build\carbon.exe
python .\tests\parity.py
python .\tests\run_all.py
```

The build script uses the installed Visual Studio 2026 Build Tools. CMake is also
supported with a C++20 compiler (`cmake -S . -B build/cmake`, then
`cmake --build build/cmake --config Release`). CMake is not required by the script.
The script-built executable locates the supplied sibling `JavaMinecraft` tree
relative to its own path, so it can launch from another working directory. Use
`--source-root <absolute-directory>` to select a different tree. CMake builds embed
the source path. Resources are neither copied nor altered.

RNG parity checks compile source slices in an isolated directory with minimal
dependency stubs and use the byte-for-byte `BitRandomSource.class` from the
supplied `26.3.jar`. The decompiled interface's `nextDouble` float literal does
not match its class bytecode. A separate raw-JAR regression checks 2,560 exact
rows for Legacy and SingleThreaded RNGs across ten seeds; each row includes two
double bit patterns and a subsequent bounded-int result to verify state
consumption. The source-derived Gaussian comparison allows `1e-14` error for
platform math and does not establish bitwise Gaussian parity. Noise JVM checks
use supplied noise sources with a handwritten RNG model reconciled to the jar's
verified arithmetic; they do not execute the complete noise classes from the
jar. Direction/rotation checks exercise 2,340 JVM oracle cases, including
negative and extreme IDs, axis transforms, nearest-direction ties and mirror
arithmetic. Noise checks compare 244 exact float and wrap results.
Palette checks cover identity-based lookup, resizing and serialization. Lighting
storage and level-height math are compared with the supplied Java source; this
does not implement light propagation. Terrain-stack vectors compare exact float
bits and construction RNG state.
Scheduled tick tests compare queue data behavior with a dependency-free JVM
fixture model derived from the supplied Java 26.3 classes; they do not execute
Minecraft's classes or implement codecs and world-wide ticking.
Block model tests read original resource JSON in place and verify decoded parent,
texture, geometry and variant semantics; this is a tested loader subset, not a
renderer integration or a full Gson/codec behavior port. The JSON parser is the
pinned nlohmann/json 3.12.0 single header from the project's official release
source; its MIT license is in `third_party/nlohmann/LICENSE.MIT`.

## Porting rules and next steps

Keep each translated subsystem traceable to its Java source. Preserve Java
overflow, evaluation order, UTF-16, float precision and serialization semantics.
Do not introduce Bedrock behavior or substitute approximate gameplay. Compare
translated behavior to the supplied Java implementation before building on it.

Next dependencies: complete random-source variants, NBT codecs, registries,
resource-pack loading, chunk storage and world generation. Client and integrated server must ultimately share
the translated simulation. Track remaining work honestly rather than treating
the bootstrap as the game.



# Java Edition 26.3 port status

Reference: `../JavaMinecraft/decompiled`, `version.json` protocol 777, world
version 5023. Implementations target this supplied tree, including its exact
numeric expressions; they must not substitute behavior from earlier Java
releases or Bedrock Edition. Where decompiled source conflicts with a method in
the supplied `26.3.jar`, verified bytecode is authoritative for that method.
The raw-JAR RNG regression below checks only the named RNG primitives; source
and hybrid noise oracles remain limited to their exercised methods.

This is a native core library, diagnostic executable, and limited playable client
slice. It does not establish full Java Edition parity. A passing component test
establishes the tested subset only.

| Component | Implementation | Validation and remaining scope |
| --- | --- | --- |
| Legacy RNG | `LegacyRandomSource.hpp/.cpp` | 2,560 bit-exact raw-jar rows for Legacy and SingleThreaded sources across 10 seeds, including signed limits and values around 2^53; rows exercise two doubles followed by bounded-int consumption. Gaussian remains tolerance-based in the source-derived suite. |
| Single-threaded RNG and xoroshiro core | `RandomSources.hpp`, `XoroshiroWorldgenNoise.hpp` | Legacy/single-threaded source suite compiles against the byte-for-byte jar `BitRandomSource.class`. Xoroshiro core state checks plus 768 source-adapted double/state rows; its raw `XoroshiroRandomSource` cannot initialize jar-only because the jar omits its external serialization `Codec` dependency. Its `nextDouble` bytecode is verified as `l2d; dmul`. |
| Integer coordinates | `Coordinates.hpp` | 261 JVM cases; arithmetic/packing only, no codecs/iterators/mutable types |
| Math and Vec3 | `Math.hpp`, `Vec3.hpp` | 4,445 JVM math cases plus native vector checks; selected helpers only |
| Identifiers and directory resources | `Identifier.hpp`, `DirectoryResources.hpp` and matching `.cpp` | Original-file reads and path tests; no pack priority stack or reload manager |
| Binary IO | `BinaryIO.hpp` | JVM byte comparison and native checks; malformed standard UTF-8 currently rejected instead of Netty replacement decoding |
| NBT values | `Nbt.hpp` | Native checks; value/collection subset, no SNBT/visitors/codecs |
| NBT binary IO | `NbtIO.hpp/.cpp` | Native checks plus a supplied structure fixture; gzip streams are unsupported, compound output follows `std::map` order rather than Java `HashMap`, and quota edge cases may differ |
| Collision boxes | `AABB.hpp` | Extracted Java-method checks plus native edge cases for strict touching, negative/degenerate boxes, distance, directional growth, signed zero/NaN, segment clipping, and Java int wrap; no world collision or simulation |
| Packed integer storage | `BitStorage.hpp/.cpp` | 167 Java `SimpleBitStorage` cases, `ZeroBitStorage`, and constructor validation; remaining palette/storage consumers are not implemented |
| Directions, axes, rotations and mirrors | `Direction.hpp`, `Rotation.hpp` | 2,340 JVM oracle vectors for enum ordering, ID wrap, legacy ID conversion, yaw, nearest-direction ties, axial rotations, block rotations and mirrors; no codecs, entities, planes, JOML transforms or localized mirror symbols |
| Simplex terrain noise | `Noise.hpp` | 224 exact float samples plus 20 exact wrapping checks using supplied noise source with a handwritten RNG model reconciled to jar bytecode; not whole-class raw-JAR execution, no density functions or world generation |
| Perlin noise and terrain stacks | `PerlinNoise.hpp` | 336 exact JVM checks for 2D/3D samples, ranges, and construction RNG state using supplied noise source and handwritten RNG model reconciled to jar bytecode; not whole-class raw-JAR execution, no density functions or world generation |
| Block palettes and container storage | `Palette.hpp`, `PalettedContainer.hpp` | Native/JVM checks for single, linear, hash-map and global palettes, identity, resizing, serialization and paletted storage; no chunk lifecycle or world integration |
| Lighting nibble storage and height access | `DataLayer.hpp/.cpp`, `LevelHeightAccessor.hpp/.cpp` | 6 JVM cases for default/materialized behavior, copy/fill, out-of-range uniform values and negative/extreme section math; no propagation or light engine |
| Scheduled tick queue data | `ScheduledTicks.hpp` | Native checks plus comparison with a source-derived JVM fixture model for priority IDs/order, identity+position uniqueness, signed tick extremes, saved-delay wrap, pending hydration, callbacks, removals and blackhole access; this is not execution of Minecraft classes, codecs or world-wide tick simulation |
| Advanced terrain noise | `AdvancedNoise.hpp` | `NormalNoise` octave construction/sample subset with supplied-source JVM checks and bytecode-reconciled RNG model; not whole-class raw-JAR execution, no density functions or world generation |
| Chunk sections and heightmaps | `ChunkSection.hpp`, `Heightmap.hpp` | Native lifecycle, storage, ticking counts, copying and heightmap edge checks; not integrated into `PlayableWorld` chunk loading |
| Block state properties | `StateProperties.hpp` | Native/JVM checks for boolean, bounded integer and enum properties, lexicographic state ordering, transitions and cached identity; no block registry integration |
| Voxel shapes | `VoxelShapes.hpp` | Native checks plus selected JVM collision parity for occupancy, boolean joins, bounds and axis collision; no world collision integration |
| Resource block models and blockstate variants | `BlockModels.hpp/.cpp`, `ClientRenderer.hpp/.cpp` | Reads original JSON in place; resolves parent chains, textures, faces, UV/cull/tint and source variants. Supports blockstate quadrant X/Y/Z rotations, UV locking, position-seeded weighted alternatives, multipart states, and rotated model elements for the exercised models. WGL rendering covers supported stone, grass, slab, stair and oak-fence shapes. Uses pinned nlohmann/json 3.12.0 (MIT), source and license in `third_party/nlohmann` |
| World mesh batching and cache | `ClientRenderer.hpp/.cpp`, `PlayableWorld.hpp/.cpp`, `main.cpp` | Batches section meshes by source texture, performs source-cullface/support-shape culling, caches selected local geometry and smooth-light snapshots, fingerprints one-cell AO/cull and sky-column dependencies, and invalidates on world edits. The main loop holds a complete 5×5 chunk-section window around the player’s current section, covering the prior radius-24 view while moving within it. Focused WGL sample on Intel Iris Plus (GL 4.6.0, 31.0.101.2140): fixed-scene CPU render-through-swap p50/p95 0.682/0.799 ms; within-window movement 1.423/3.882 ms; next-section edge rebuild 72.108 ms. These are hidden-window CPU timings, not GPU completion or visible gameplay FPS. Display lists are used for the current compatibility renderer; GPU timer queries and full light propagation remain absent. |
| Hit results | `HitResult.hpp` | Immutable Java-source subset for type, location, block position, face, miss state, inside/world-border flags, and copy-style `withDirection`, `withPosition`, `hitBorder`; constructor behavior is covered by native source-derived assertions, not a Java runtime differential. Entity distance/codecs omitted. |
| Slab state, shape and placement | `SlabBlock.hpp`, `PlayableWorld.hpp/.cpp` | Dry stone slab `TYPE` states, source half-height voxel geometry, shape-aware collision/raycast, half-height placement and same-slab merging. Source-derived assertions cover thresholds and geometry. Fluids/waterlogging and general block registry are unsupported. |
| Stair state, shape and placement | `PlayableWorld.hpp/.cpp`, `StairsBlock.hpp` | Supported dry stone stair properties, Java-facing and half placement rules, neighbor-derived straight/inner/outer shapes, voxel geometry, collision and raycast. Waterlogging and fluid scheduling remain unsupported. |
| Creative hotbar and pick-block | `CreativeHotbar.hpp`, `GuiItemRenderer.hpp/.cpp`, `ClientRenderer.hpp/.cpp`, `main.cpp` | Nine-slot selection, source-sign mouse-wheel wrap, number keys 1–9, and supported block-item pick behavior. Existing items are selected; missing items use an empty slot or replace the selected slot if all nine are full. Air slots stay empty. 3D icons resolve the current `assets/minecraft/items/*.json` `minecraft:model` form, inherited model `display.gui` transforms and original model textures/geometry. GUI directional shading is a native approximation; alternate item-model dispatch types, slab/stair property state, and block-entity data are unsupported. Native source-asset transform tests and WGL smoke cover the implemented path; these tests are not JVM differential parity. No 36-slot inventory, stack counts or item codecs. |
| Native client screens and multiplayer pregame | `main.cpp`, `ClientRenderer.hpp/.cpp`, `Multiplayer.hpp/.cpp` | Native title, world selection, flat-world creation, multiplayer pregame and pause screens. World data lasts only for the current session; there are no disk saves or complete Java screen flow. Multiplayer parses status responses and sends an offline-profile login start; it stops when encryption, online authentication or configuration/gameplay handoff is required. `tests/multiplayer.cpp` exercises status handling against a handwritten loopback server; it is not a vanilla-server differential or gameplay connection test. UI flow has no automated parity test. |
| Playable flat creative slice | `PlayableWorld.hpp/.cpp`, `ClientRenderer.hpp/.cpp`, `main.cpp` | Source-backed `CLASSIC_FLAT` layers (-64 bedrock, -63/-62 dirt, -61 grass), grounded creative walking/jumping/gravity/drag, on-ground state, sprint, crouch and ledge sneak, step-up and shape-aware collision/raycast; Java double-tap creative flight toggle; nine-slot supported-block hotbar, pick-block, 5-block reach edits and WGL rendering of supported source block models. Native movement checks cover grounded traction, sprint, jump/land, flight toggle, ledge sneak and slab step-up. Main visibility uses complete chunk-section windows with bounded mesh retention. The world remains classic flat while the supplied 26.3 density/aquifer/material pipeline is incomplete. Survival systems such as health, hunger, breaking progress and fall damage are not implemented. |

## Next batches

Full light propagation, chunk lifecycle, and higher
world-generation layers remain useful next dependencies. Storage, height,
palette-backed containers and terrain noise stacks have independent source checks.

## Still required for a complete port

Registries, codecs and data fixes; resource packs and reload lifecycle; complete
world generation; chunks, lighting and saves; block/entity/item behavior; physics
and tick simulation; inventory and recipes; commands and text; packet protocol,
authentication and services; integrated/dedicated server; native rendering,
window/input/audio; screens and accessibility; platform integration; full-game
parity and performance testing. Listing these is not a claim that any exist yet.

## Verification

The default `build/carbon.exe` launch opens the native client. Use **WASD** to
move, **Space** to ascend, **Left Shift** to descend, **Left Ctrl** to sprint, mouse
to look, **1–9 or mouse wheel** to select, middle click to pick a supported block item,
left click to break, right click to place, and **Esc** to release/resume. `--diagnostics` is the noninteractive bootstrap mode
used by the combined test runner. Local block edits are in memory only.

`python tests/run_all.py` builds the native program and component drivers, then
runs their JVM comparison scripts. This entry point uses the installed Windows
MSVC and JDK paths used by the component scripts. CMake/CTest provide a separate
native build route; JVM scripts remain Windows development tools at present.


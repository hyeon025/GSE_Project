# GSE_Project

## Actor and Scene Graph

- `SimpleGame/Actor.h`: common identity, local transform, visibility, activation,
  render layer and update/draw hooks. `VisualActor<T>` owns a render snapshot.
- `SimpleGame/SceneGraph.h`: owns actors with stable graph-local IDs, parent/child
  relationships, transform composition, streaming and deferred subtree destruction.
- `SimpleGame/WorldActors.h`: player movement and NPC movement.
- `SimpleGame/CombatActors.h`: the ordered combat simulation step. Collision,
  damage and spawning remain together in the existing LevelOne rules.
- `GameScene` and `LevelOneView`: synchronize world/combat models into reusable
  actor nodes. Terrain, scenery, characters, loot, shots and effects use this path.
  HUD rendering is hosted by one screen-space actor after HDR post-processing;
  its existing widgets are still drawn together.

### Ownership and Frame Order

Game::World and LevelOne::Session remain authoritative for gameplay and saving.
Runtime actor IDs and graphs are not serialized. Changes to gameplay entities go
through these models and their actor update hooks; destroying a presentation node
alone does not delete the saved entity and synchronization can recreate it.

Update runs before presentation synchronization. BeginSync/GetOrCreate/EndSync
retains visited nodes and removes unvisited streamed nodes. Keys use persistent
entity IDs or tile coordinates, never indices into erasable combat vectors.
Restarting/loading a combat session invalidates its old presentation graph.

Rendering flattens the hierarchy and sorts by layer, explicit order, projected
ground depth and stable ID. Parent visibility and activation propagate separately.
Position, height, XY rotation (radians) and positive uniform scale compose through
parents. Character/scenery artwork remains camera-facing in the 2.5D projection.
The procedural terrain background and HUD are screen-space passes.

### Extending Scenes

Derive from Actor and override OnUpdate/OnDraw, or reuse VisualActor<T> for a
snapshot-backed visual. Add the node through SceneGraph::GetOrCreate with a unique
ActorKey and a parent ID (0 means no parent). Use Group for non-rendering parents.
Reparent optionally preserves the world transform and rejects cycles.

Destroy marks a subtree immediately and releases it after traversal. Nodes added
during a traversal begin participating at the next traversal. Reparent, Clear and
streaming synchronization must run outside update/draw callbacks. Raw Actor
references must not outlive their graph node; retain IDs and use Find instead.

### Regression Tests

`tests/SceneGraphTests.cpp` is a standalone C++14 test executable, separate from
the game entry point. It covers transforms, reparenting, visibility, render order,
stable reuse, streaming, callback-time deletion and combat restart/update behavior.
Build and run it explicitly when validating changes; it is not run automatically.

## Open-World Safety

Unprovoked player raids now have a 60-second session grace period, an 85-unit
acquisition radius and a six-second pursuit limit. Entering a village ends an
unprovoked chase. Disengaged raiders wait 20 seconds before retargeting; an
eight-second shared respite also prevents another raider immediately taking over.
Only one NPC can start an unprovoked player pursuit at a time. Player-first attacks
still provoke retaliation, including guards. Open-world hits have a shared
1.5-second recovery window; unprovoked hits deal 3-6 damage at 2.5-second intervals.
Respawning clears existing player targets and grants 30 seconds of raid grace.
These safety timers reset on loading and do not change the save-file format.
The periodic World log includes playerPursuers and playerRaidGrace.

## Rendering Performance

2026-11-01: Actual saved-world play now prepares animation poses on a bounded CPU
worker, culls off-screen character meshes, limits large-mesh batch padding and
reuses spatial simulation calculations. The production Update/Display/HUD/swap
test reduced moving-scene p95 from 77.63ms to 16.87ms. See
[actual gameplay validation and limits](docs/GameplayOptimization_2026-11-01.md).
Use tools/Measure-Gameplay.ps1 with a save copy for repeatable play-loop tests.

2026-10-31: GPU meshes now use bounded LRU range reuse instead of clearing the
entire arena. Modular parts share skeleton evaluation and the pose cache uses LRU.
The 48-character render benchmark improved from 44.96ms to 6.10ms after warmup
without reducing model or post-processing quality. This excludes game simulation.
See [measurements and validation](docs/RendererOptimization_2026-10-31.md).
Run the built `x64/Release/SimpleGame.exe` for optimized gameplay.

The renderer queues ordered instances backed by disk-cached unit/static meshes.
Text shares atlas pages with ordered geometry batches. Frame CSV and cache events
are written under %LOCALAPPDATA%/GSE_Project/RenderLogs with bounded rotation.
See [the performance report](docs/RendererPerformanceReport.md) for the baseline,
batching tradeoffs, cache format, metric definitions and comparison procedure.
Use tools/Summarize-RenderProfile.ps1 to summarize a session's frame CSV files.
Standalone cache and batching tests are in tests/RenderBatchTests.cpp.

## Character Presentation

Blender Art assets now load through the catalog-driven GLB importer and skeletal
pose cache. See docs/ArtLoader.md for export rules, F8 reload, saved appearance
identity, animation APIs and supported material/mesh features.

CharacterMotion maps isometric displacement to front/back/left/right facing and
retains the last heading when idle. Walking advances with actual travel, not input
alone. World and combat player movement both use this state.
CharacterMesh builds lit, rotated 3D body surfaces projected into the existing
2.5D renderer. Direction/pose variants are cached as mesh kind 7, with an independent
appearance revision in the variant key; animation does not regenerate warm meshes.
Ground relief and object grain are procedural shading, not collision geometry or
photorealistic scanned assets. UI primitives retain their original shading.
Meadow shading uses blended green patches and reduced fine grain. Jittered, tapered
grass tufts replace regular rows of strokes in both world and combat views; 16
mesh variants (kind 9) share the existing disk cache and instanced draw path.
tests/CharacterVisualTests.cpp links with SimpleGame/CharacterMesh.cpp to cover
direction selection, idle/blocked movement, pose keys and deterministic geometry.

## Elevated Isometric View

`SimpleGame/IsometricView.h` owns the fixed oblique projection, screen anchor and
default zoom. Ground depth is now 0.65 instead of 0.5, with a wider initial view.
The terrain shader receives these values from the renderer; actors, cached body
surfaces, village buildings and combat blocks use the same projection.
Buildings and ruined walls have projected side/top faces. Foreground scenery
fades near the player, and buildings/blocks sort at their front ground edge.
The camera now uses a 22.5-degree azimuth so the diagonal village roads no longer
project as vertical/horizontal lines. World coordinates, saves and collision
geometry are preserved. Terrain receives the CPU inverse-projection basis;
both minimaps, WASD input and character facing share the same view conversion.
Changed character/building/wall/block variants use mesh revision 3 so old disk
meshes are not reused. This remains a painter-ordered 2.5D renderer, not a new 3D
engine; building interiors, free camera rotation and exact per-pixel occlusion
are not implemented. Collision footprints and screen-relative controls are unchanged.
`tests/IsometricViewTests.cpp` is a standalone C++14 regression test for projection,
inverse projection, zoom/viewport anchoring and scenery fading. Run explicitly.

## Local World Monitor

The world contains seven settlements with fifty initial residents each. Each has
a 640-unit radius, sixty private housing plots and a market. Adjacent centers are
1,700 units apart: twenty seconds at the 85-unit/second base walking speed.
Letters and visiting residents carry news; migration uses received reports and
changes home affiliation only upon arrival. See docs/SettlementExpansion.md.
NPC names
are stable derivatives of saved IDs; home affiliation is distinct from current
location and travel destination. GSE_WORLD 5 / SOCIETY 4 saves track settlement seeding,
housing, migration and mail. Older saves receive the new layout and population
expansion once while keeping existing residents, deaths and markets. Game labels and monitor JSON expose names
and affiliation; monitor filters follow the received village catalog.

The game exports read-only JSON snapshots once per second to
`%LOCALAPPDATA%/GSE_Project/Monitor/snapshot.json` through a background file writer.
Run `powershell -File .\tools\Start-Monitor.ps1` (Python 3.9+) and open
`http://127.0.0.1:8765/` for NPC status, village aggregates, live combat mob counts,
events and a settlement map. No external packages or network services are required.
The server binds only to loopback; use `-Port 8766` when the default port is busy.
See [the monitoring guide](docs/WorldMonitor.md) for the JSON contract, count
definitions, connection states, limitations and optional test commands.

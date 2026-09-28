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

## Rendering Performance

The renderer queues ordered instances backed by disk-cached unit/static meshes.
Text shares atlas pages with ordered geometry batches. Frame CSV and cache events
are written under %LOCALAPPDATA%/GSE_Project/RenderLogs with bounded rotation.
See [the performance report](docs/RendererPerformanceReport.md) for the baseline,
batching tradeoffs, cache format, metric definitions and comparison procedure.
Use tools/Summarize-RenderProfile.ps1 to summarize a session's frame CSV files.
Standalone cache and batching tests are in tests/RenderBatchTests.cpp.

## Character Presentation

CharacterMotion maps isometric displacement to front/back/left/right facing and
retains the last heading when idle. Walking advances with actual travel, not input
alone. World and combat player movement both use this state.
CharacterMesh builds lit, rotated 3D body surfaces projected into the existing
2.5D renderer. Direction/pose variants are cached as mesh kind 7, with an independent
appearance revision in the variant key; animation does not regenerate warm meshes.
Ground relief and object grain are procedural shading, not collision geometry or
photorealistic scanned assets. UI primitives retain their original shading.
tests/CharacterVisualTests.cpp links with SimpleGame/CharacterMesh.cpp to cover
direction selection, idle/blocked movement, pose keys and deterministic geometry.

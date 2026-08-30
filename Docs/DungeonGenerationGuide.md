# Dungeon Generation & Room System — Developer Guide

How the procedural dungeon works, organized by feature. Each section explains what the feature does, where it lives, and how to extend it. If something here contradicts the editor, trust the editor and fix this doc.

---

## The three core actors

| Blueprint | Role |
|---|---|
| `BP_DungeonGeneratorNEW` | Procedurally lays out and spawns rooms at level start. Runs once, then gets out of the way. |
| `BP_RoomWrapper` | A lightweight runtime proxy spawned **per room**. Scans its room's streamed sub-level for doors/spawn points/volumes, reports the room in to the manager, and owns that room's combat encounter. |
| `BP_DungeonManager` | The single source of truth for room state, adjacency, and door open/close. One instance, placed in the persistent level. |

All three are plain `Actor` (no C++ parent) under `Content/Blueprints/Procedural/`. There's also an old `BP_DungeonGenerator.uasset` (no "NEW" suffix) sitting in the same folder — **deprecated, ignore it**, `BP_DungeonGeneratorNEW` is the live one.

Rooms are stored as parallel arrays on `BP_DungeonManager` (`RoomIDs`, `RoomStates`, `RoomTypes`, `RoomEnemyCounts`, `RoomPlayerPresent`, `RoomLocations` — all index-aligned, no struct). `FindRoomIndex(RoomID)` is the lookup used everywhere to go from a `RoomID` to the shared array index. If you add a new per-room field, it has to be another parallel array kept in sync the same way.

---

## Feature: Dungeon generation

**What it does:** builds the dungeon layout at `BeginPlay` by recursively spawning rooms off unused exits until the target room count is hit.

**Where:** `BP_DungeonGeneratorNEW`, mainly the `SpawnNextRoom` custom event (in `EventGraph`) and its helper functions `DetermineRoomType`, `SpawnRoomOfType`, `checkOverlap`, `AddExits`, `LinkConnectorTransforms`.

**How it works:**
1. `EventBeginPlay` → `StartDungeonGeneration` → sets up the manager ref, seeds the random `Stream`, appends `definiveRooms` into the working `RoomsPDAList`, then kicks off `SpawnNextRoom`.
2. `SpawnNextRoom`, per iteration:
   - Picks a random unused exit from `exitsList`.
   - `DetermineRoomType` picks an `E_RoomType` + candidate PDA, based on position in the sequence: **this room is the first one spawned** (`Index == 0`) → `NewEnumerator3` (Start); **this is the last room left to spawn** (`TotalRoomNumber == 1`) → `NewEnumerator2` (treated as the boss slot, see below); otherwise → `NewEnumerator0` (random pick from `RoomsPDAList`).
   - `checkOverlap` retries with a 0.2s delay if the candidate would overlap something already placed.
   - `SpawnRoomOfType` switches on the `E_RoomType` and loads the right `.umap` via `LoadLevelInstance`. The `NewEnumerator2` (boss) case hardcodes `Room_Building_SquareMedium_Convert` instead of reading the PDA — that's a real TODO if you ever want boss rooms to be selectable/varied (see Traps below).
   - Spawns a `BP_RoomWrapper` for the room, computes its `EETHRoomState`-style room-type byte via `DetermineRoomTypeByte()` (Boss=5 / Start=1 / Generic=2, mirroring the same positional logic above) and stores it on the wrapper via `SetRoomType`, then calls `InitializeRoom` on the wrapper (through the `BPI_DungeonRoom` interface — see the Room States section for why that matters).
   - Adds the wrapper to `RoomWrappers`, links parent/child adjacency via `DungeonManager.AddAdjacentRoom`, decrements the remaining count, and loops.
3. Meanwhile each room's `BP_RoomWrapper.OnLevelContentReady` fires independently as its sub-level streams in (see Room Registration below).
4. Once every spawned room has registered, `BP_DungeonManager.EventBP_OnAllRoomsReady` fires and calls the generator's `LinkConnectorTransforms` — an O(n²) proximity check (< 800 units) between every pair of rooms' connectors that creates the door connections (see Doors below).

**How to add a new room to the pool:**
1. Add a `.umap` under `Content/Blueprints/Procedural/Dungeon/Rooms_Templates/Level/`.
2. Add a `PDA_RoomDefinition` data asset under `Content/Blueprints/Procedural/PDA/` pointing `LevelReference` at that map, with a `Connectors` array (one `FStruct_RoomConnector` per entrance/exit — local transform used for door placement and adjacency detection) and a `SelectionWeight`.
3. Place doors in the `.umap` at each connector's position, matching the PDA's `Connectors` array.
4. Add the new PDA to `definiveRooms` on `BP_DungeonGeneratorNEW`.
5. If the room should have enemies, place `BP_EnemySpawnPoint` actors in it — see Combat & Enemy Spawning below.

`BP_RoomWrapper` is fully generic and needs no per-room modification — it scans whatever the sub-level contains.

**Traps:**
- Room type (Start/Boss/Generic) is decided purely by **spawn order**, not by PDA/tags. A new PDA added to `definiveRooms` can only ever become the boss room by landing last in the sequence — there's no way to pin a specific room as "always the boss."
- `PDA_RoomDefinition.RoomTags` exists as a field but is empty on every PDA and unused everywhere — don't expect it to do anything yet.

---

## Feature: Room states & doors

**What it does:** every room has a state byte that drives both gameplay (is it in combat, is it cleared) and its doors (open vs. blocking).

**Where:** all state logic lives in `BP_DungeonManager`. Door logic lives there too, **not** in `BP_RoomWrapper`.

### States

| Value | State | Doors |
|---|---|---|
| 0 | Undiscovered | Blocking |
| 1 | Discovered | **Open** |
| 2 | Active | Blocking |
| 3 | Combat | Blocking |
| 4 | Cleared | **Open** |
| 5 | Locked | Blocking |
| 6 | Boss | Blocking |

Both `Discovered` and `Cleared` count as open — not just `Cleared`. `IsOpenState(State)` returns true for `{1, 4}`, `IsBlockingState(State)` for everything else. These two functions are the *only* thing that decides door behavior.

### How state changes

Everything funnels through `BP_DungeonManager.SetRoomState(RoomID, NewState)`. No-op if the room doesn't exist or the state isn't actually changing. On a real change it broadcasts `OnRoomStateChanged`, calls `RefreshDoorsForRoom` (never call that yourself), and — only if the new state is `Cleared` — calls `DiscoverAdjacentRooms` to reveal neighbors.

Call sites:
- `RegisterRoom` — sets the initial state (`Locked` for Locked-type rooms, `Discovered` otherwise).
- `OnPlayerEnterRoom(RoomID)` — checked in order: `RoomTypes[idx] == Boss(5)` → `Boss`; else `RoomEnemyCounts[idx] > 0` → `Combat`; else → `Active`.
- `OnEnemyDied(RoomID)` — decrements enemy count; at zero, `Cleared` if player present, else `Discovered`.
- `UnlockRoom(RoomID)` — only affects `Locked` rooms, unlocks to `Active`/`Discovered`.
- `DiscoverAdjacentRooms(RoomID)` — flips neighboring `Undiscovered` rooms to `Discovered`, skips `Locked` rooms.

### How to add a new state

There's no enum asset backing this in Blueprint — state comparisons are raw byte literals. To add one:
1. Pick an unused byte (7+). Also add it to the C++ `EETHRoomState` enum in `Source/EthericConcrete/ETHDungeonManager.h` (still referenced by `BP_RoomWrapper.GetRoomState()` for display) so it stays in sync.
2. Add it to exactly one of `IsOpenState`/`IsBlockingState` — leaving it out of both means doors just keep whatever state they were already in.
3. Add (or extend) a call site that calls `SetRoomState(RoomID, YourNewState)` at the right trigger. Don't call `RefreshDoorsForRoom` yourself.
4. If it should behave like `Locked` for adjacency purposes, add it to `DiscoverAdjacentRooms`'s exclusion check.
5. Test with `Debug_ForceRoomState()` (see Debug Tools) instead of reproducing the real trigger.

### Doors

`RefreshDoorsForRoom(RoomID)` finds every door connection touching `RoomID`, and for each: opens the door if **at least one side is open AND neither side is blocking**, closes it otherwise. It casts the door actor to `BP_BaseDoor_01` and calls `AnimateOpen()`/`AnimateClose()` directly. It's called automatically by `SetRoomState`.

`BP_RoomWrapper.OpenDoors/CloseDoors/LockDoors/UnlockDoors` are dead code — nothing calls them, don't build on them.

A door *connection* (logical A↔B link) and the physical door *actor* are separate: `RegisterPendingDoorConnection` records the link, `AssignDoorActorToConnection` matches an actual door actor to it by proximity. Both are driven by the generator's `LinkConnectorTransforms` (see Dungeon Generation above).

**Trap:** the `BPI_DungeonRoom` interface declares `SetRoomState`/`GetRoomState`/etc. but nothing calls those interface functions except `InitializeRoom` (see below) and the wrapper's own `GetRoomState()`. If you're tracing "who calls X" and X is one of those interface functions, stop — the real integration point everywhere else is a direct reference to `DungeonManagerRef`.

---

## Feature: Room registration

**What it does:** each spawned room reports itself into `BP_DungeonManager`'s tracking arrays once its content is actually loaded.

**Where:** `BP_RoomWrapper.OnLevelContentReady` (bound to the room's level-streaming `OnLevelShown`) → `BP_DungeonManager.RegisterRoom`.

**How it works:**
1. Pulls the sub-level's actor list via `UETHLevelLibrary::GetActorsInLevel` (`Source/EthericConcrete/ETHLevelLibrary.h/.cpp` — needed because `GetAllActorsOfClass` doesn't work on streamed sub-levels).
2. Casts each actor: `BP_BaseDoor_01` → `RegisteredDoors`; `BP_EnemySpawnPoint` → `EnemySpawnPoints`; `BP_RoomVolume` → `RoomVolumeActors` (and binds its overlap delegates to `VolumeEntered`/`VolumeExited`).
3. Calls `DungeonManager.RegisterRoom(RoomID, self, RoomType, AdjacentRoomIDs, WorldLocation, RegisteredDoors.Length)`, reading `RoomType` from the wrapper's own variable (set earlier by the generator, see Dungeon Generation above).

`VolumeEntered`/`VolumeExited` are also the player-tracking triggers — they call `DungeonManager.OnPlayerEnterRoom`/`OnPlayerExitRoom` directly, and `VolumeEntered` kicks off `BeginEncounter()` when the overlapping actor is the player. There's no separate "player enter/exit" interface event.

`BP_DungeonManager.ReconcileMissingRooms` runs on an unconditional 1s repeating timer as a safety net — it scans the generator's `RoomWrappers` for anything not yet in the manager's arrays and re-registers it (reading the same `GetRoomType` off the wrapper). Not expected to normally do anything.

**Trap — `InitializeRoom` and the interface:** `BP_RoomWrapper.InitializeRoom` implements `BPI_DungeonRoom.InitializeRoom`, and the generator calls it through the interface (a `K2Node_Message`), not a direct function call — even though the generator holds a concretely-typed reference. This matters because **a Message call's pins are fixed to the interface's declared signature**, not whatever the implementing function graph contains. If you add a parameter to the function graph without also adding it to the `BPI_DungeonRoom` interface asset, the call site will never expose or feed it — no compile error, it just silently gets the default value. If you need to pass new data into a room at init time, prefer a plain direct `SetXxx` call on the wrapper (like `SetRoomType`) instead of extending `InitializeRoom`'s signature.

---

## Feature: Combat encounters & enemy spawning

**What it does:** spawns enemies into a room in waves when the player enters, and reports the room clear once they're all dead.

**Where:** entirely owned by `BP_RoomWrapper` — the manager only ever sees the resulting `RoomEnemyCounts` number, it has no idea how enemies are actually spawned.

| Blueprint | Role |
|---|---|
| `BP_EnemySpawnPoint` | Placed-in-level marker actor. `EnemyTemplateActor` = the enemy class to spawn here; `WaveIndex` = which wave it belongs to (0-based). |
| `BP_EnemyPlaceholder` | The enemy actor spawned at runtime. `RoomWrapperRef` (back-reference to report its own death), `CachedPlayerActor`, `MoveSpeed`, `AttackRange`, `FireCooldown`. Currently a placeholder — follows the player and fires at range. |
| `BP_Projectile_EnemyBasic` | The projectile the placeholder enemy fires. |

**How it works:**
1. Player overlaps the room volume → `VolumeEntered` → `BeginEncounter()`, then `DungeonManager.OnPlayerEnterRoom` (in that order, so the enemy count below is already set before the manager decides Active vs. Combat).
2. `BeginEncounter()` — first time only (`bEncounterStarted` gate) — calls `SpawnWave(0)`.
3. `SpawnWave(WaveIndex)` spawns `EnemyTemplateActor` at every `EnemySpawnPoints` entry matching that wave index, adds each to `RegisteredEnemies`, sets its `RoomWrapperRef`, then calls `DungeonManager.SetRoomEnemyCount(RoomID, RegisteredEnemies.Length)` — **this is what `OnPlayerEnterRoom` reads** to decide `Combat`.
4. Each enemy, on death, is expected to call `BP_RoomWrapper.NotifyEnemyDied(self)`.
5. `NotifyEnemyDied` removes it from `RegisteredEnemies`. If enemies remain, just tells the manager (`OnEnemyDied`, decrements its counter). If none remain, checks for a spawn point at `CurrentWaveIndex + 1` (`CheckHasSpawnPointForWave`): if one exists, `SpawnWave` fires the next wave; otherwise `DungeonManager.OnEnemyDied` runs, which at zero transitions the room to `Cleared`/`Discovered`.

**How to set up a room's encounter:**
1. Place `BP_EnemySpawnPoint` actors in the room's `.umap` where you want enemies to appear.
2. Set `EnemyTemplateActor` on each to the enemy class to spawn there.
3. Set `WaveIndex` on each — all points sharing `WaveIndex = 0` spawn together as wave one; `WaveIndex = 1` spawns once every wave-0 enemy is dead, and so on. No spawn points at `WaveIndex = 0` means the room never enters combat.

Nothing else needs wiring per-room — `BP_RoomWrapper` picks up whatever spawn points exist, same as doors.

**Traps:**
- `SpawnWave` silently no-ops for any spawn point whose `EnemyTemplateActor` is unset — if a room "won't spawn enemies," check every spawn point's template first (this also happens if a referenced template actor gets deleted from the level, which zeroes out every point still pointing at it).
- An enemy that never calls `NotifyEnemyDied` (dies some other way — falls out of the world, gets destroyed directly, etc.) leaves the room stuck in `Combat` forever.
- `bEncounterStarted` never resets — a room's encounter is a one-shot. Making it repeatable needs a manual reset added by you.
- Room type (Boss/etc.) and enemy waves are independent — being the boss room doesn't auto-configure multiple waves or a special roster, that's all per-room via spawn points like any other room.

---

## Debug tools

`BP_DungeonManager.bDebugMode` — note `EventBeginPlay` hardcodes it to `true` unconditionally, overriding whatever you set on the instance. To disable, remove/disable that node.

With debug mode active:
- `DebugDrawRoomStates` — 0.5s repeating timer, draws a colored sphere per room (green=open, red=blocking) + label, and a line per door connection.
- `DebugAutoClearRoom` — whenever a room transitions to `Active`, waits 4s then force-sets it to `Cleared`. Useful for iterating on state logic without real enemies; disable if you're testing enemy-driven clearing specifically.
- `Debug_ForceRoomState()` — `CallInEditor` function, force-sets `DebugTargetRoomID` to `DebugTargetState`. Set both on the instance in the Details panel, click the button.

Independent of `bDebugMode`: `ReconcileMissingRooms` (see Room Registration above) always runs.

---

## Orphaned / dead code (don't waste time debugging these)

- `Source/EthericConcrete/ETHDungeonManager.h/.cpp` (`AETHDungeonManager`) — the class is dead, nothing instantiates it. **Keep the file though** — it still declares `EETHRoomState`/`EETHRoomType`, which are live-referenced by `BP_RoomWrapper.GetRoomState()` and `ETHMinimapWidget`.
- `Source/EthericConcrete/ETHDungeonDebugger.h/.cpp` (`AETHDungeonDebugger`) — fully orphaned, `BP_DungeonManager` has its own native debug draw now. Safe to delete after a final reference check.
- `Content/Blueprints/Procedural/BP_DungeonGenerator.uasset` and `Dungeon/dungeonGenerator.uasset` — deprecated predecessors of `BP_DungeonGeneratorNEW`.
- `BP_DungeonManager.T3D`, `BP_DungeonGeneratorNEW.T3D`, `BP_RoomWrapper.T3D` — stale clipboard/export dumps, not live state, safe to remove from source control.
- `WBP_Minimap` / `ETHMinimapWidget` — the minimap is currently broken: it hard-depends on finding a live `AETHDungeonManager` actor via `TObjectIterator`, but the manager is a plain `Actor` now, so it never finds one and silently never updates. Needs a real port to reference `BP_DungeonManager` directly (or a Blueprint rebuild) — not a quick fix.
- `BPI_DungeonRoom`'s `SetRoomState`/`GetRoomState`/`NotifyEnemyDied`/`SetDoorState`/`OnPlayerEnter`/`OnPlayerExit`, and `BPI_Door.SetDoorState` — unused scaffolding, except `InitializeRoom` (see Room Registration above).
- `Content/Blueprints/Procedural/Placeholders/Room_CorridorM_T/BP_Room_Building_CorridorM_TèPlaceholder.uasset` — mojibake filename, duplicate of the correctly-named file next to it.

---

## Quick reference — "where do I look for X?"

| Question | Look at |
|---|---|
| Why is this door open/closed? | `BP_DungeonManager.RefreshDoorsForRoom`, `IsOpenState`/`IsBlockingState` |
| Why did/didn't this room's state change? | `BP_DungeonManager.SetRoomState` and its callers (`OnPlayerEnterRoom`, `OnEnemyDied`, `UnlockRoom`) |
| How do I add a new room state? | Room States & Doors → "How to add a new state" |
| How do rooms get spawned? | `BP_DungeonGeneratorNEW.SpawnNextRoom` |
| How do I add a new room to the pool? | Dungeon Generation → "How to add a new room to the pool" |
| How does a room report itself in? | `BP_RoomWrapper.OnLevelContentReady` → `RegisterRoom` |
| How do rooms find their neighbors / doors get linked? | `LinkConnectorTransforms`, called from `EventBP_OnAllRoomsReady` |
| How do enemies get spawned into a room? | Combat & Enemy Spawning → "How to set up a room's encounter" |
| Why won't this room spawn enemies / clear after a fight? | Combat & Enemy Spawning → "Traps" |
| Minimap not updating? | Orphaned/dead code — needs a real port, not a quick fix |

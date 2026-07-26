# Dungeon Generation & Room System — Onboarding Guide

Audience: a new programmer on EthericConcrete who has never touched the procedural dungeon system. This document explains how a dungeon gets generated, how a room's state machine and doors work, and how to add a new room to the pool. It also lists known-broken/dead code so you don't waste time debugging things that are already understood problems.

Ground truth verified directly against the live Blueprints on 2026-07-25. If something here contradicts what you see in the editor, trust the editor and update this doc.

---

## 1. The three actors, in one sentence each

| Blueprint | Role |
|---|---|
| `BP_DungeonGeneratorNEW` | Procedurally lays out and spawns rooms at level start. Runs once, then gets out of the way. |
| `BP_RoomWrapper` | A lightweight runtime proxy spawned **per room**. Scans its room's streamed sub-level for doors/volumes and reports the room in to the manager. |
| `BP_DungeonManager` | The single source of truth for room state, adjacency, and door open/close. One instance, placed in the persistent level. |

All three are plain `Actor` (no C++ parent) under `Content/Blueprints/Procedural/`:
- `BP_DungeonGeneratorNEW.uasset`
- `BP_DungeonManager.uasset`
- `Utils/BP_RoomWrapper.uasset`

There is also an old `BP_DungeonGenerator.uasset` (no "NEW" suffix) sitting in the same folder — **deprecated, do not use it**, `BP_DungeonGeneratorNEW` is the live one.

### Why no C++ manager?

Up until 2026-07-23 the room state machine lived in C++ (`AETHDungeonManager`, `Source/EthericConcrete/ETHDungeonManager.h/.cpp`). It was ported line-for-line into `BP_DungeonManager` and the Blueprint was reparented to plain `Actor`. The C++ files are still on disk but the `AETHDungeonManager` class itself is no longer instantiated anywhere — see [§7.1](#71-orphaned-c-and-content).

One consequence of the port: the old C++ manager stored rooms in a `TMap<int32, FETHRoomEntry>`. Blueprint has no clean equivalent, so `BP_DungeonManager` stores every per-room field as its own **parallel array** (`RoomIDs`, `RoomStates`, `RoomTypes`, `RoomEnemyCounts`, `RoomPlayerPresent`, `RoomLocations` — all index-aligned). `FindRoomIndex(RoomID)` is the lookup function used everywhere to go from a `RoomID` to the shared array index. If you add a new per-room field, it has to be a new parallel array kept in sync the same way — there's no struct-of-room-data to extend.

---

## 2. Room state machine

States are byte-coded 0–6 (mirrors the C++ enum `EETHRoomState`, still declared in `ETHDungeonManager.h` and still referenced by Blueprints even though the class that used to own it is dead):

| Value | State | Doors |
|---|---|---|
| 0 | Undiscovered | Blocking (closed) |
| 1 | Discovered | **Open** |
| 2 | Active | Blocking (closed) |
| 3 | Combat | Blocking (closed) |
| 4 | Cleared | **Open** |
| 5 | Locked | Blocking (closed) |
| 6 | Boss | Blocking (closed) |

This trips people up: **both `Discovered` and `Cleared` count as "open"**, not just `Cleared`. `BP_DungeonManager.IsOpenState(State)` returns true for `{1, 4}`; `IsBlockingState(State)` returns true for `{0, 2, 3, 5, 6}`.

### What changes state

All state changes funnel through `BP_DungeonManager.SetRoomState(RoomID, NewState)`. It's a no-op unless the room exists (`FindRoomIndex >= 0`) and the new state actually differs from the current one. On an actual change it:
1. Broadcasts the `OnRoomStateChanged` dispatcher.
2. Calls `RefreshDoorsForRoom(RoomID)` (see §3).
3. If `NewState == Cleared`, also calls `DiscoverAdjacentRooms(RoomID)` — this is how clearing a room reveals its neighbors.

Triggers that call `SetRoomState` (directly or via a wrapper function):
- `RegisterRoom` — sets the initial state when a room first registers (see §4).
- `OnPlayerEnterRoom(RoomID)` — Boss-type rooms → `Boss`; rooms with enemies remaining → `Combat`; otherwise → `Active`.
- `OnEnemyDied(RoomID)` — decrements the room's enemy count; at zero, sets `Cleared` if the player is present, else `Discovered`.
- `UnlockRoom(RoomID)` — only affects rooms currently `Locked`; unlocks to `Active` or `Discovered` depending on player presence.
- `DiscoverAdjacentRooms(RoomID)` — flips neighboring `Undiscovered` rooms to `Discovered` (skips `Locked`-type rooms).

---

## 3. Doors

Doors are **not** controlled by `BP_RoomWrapper`. All door open/close logic lives in `BP_DungeonManager.RefreshDoorsForRoom(RoomID)`, which:
1. Finds every door connection touching `RoomID` (the `DoorConnRoomA`/`DoorConnRoomB`/`DoorConnActor`/`DoorConnPos` parallel arrays).
2. For each connection, checks both sides' states: door opens if **at least one side is open AND neither side is blocking** — i.e. `(IsOpenState(A) OR IsOpenState(B)) AND NOT (IsBlockingState(A) OR IsBlockingState(B))`.
3. Casts the door actor to `BP_BaseDoor_01` and calls `AnimateOpen()` / `AnimateClose()` directly.

`RefreshDoorsForRoom` is called automatically from `SetRoomState` — you never need to call it manually.

**`BP_RoomWrapper.OpenDoors/CloseDoors/LockDoors/UnlockDoors` are dead code.** They still exist and compile, but nothing calls them — they predate the centralized manager-driven door system. Do not build new features on top of them; they will silently do nothing useful because the manager's `RefreshDoorsForRoom` will just re-close/re-open the door on the next state change anyway.

### Door connections vs. door actors

A door *connection* (the logical link between two rooms) and the physical door *actor* are registered separately:
- `RegisterPendingDoorConnection(RoomID_A, RoomID_B, ConnectorWorldPos)` — records the logical A↔B link with a world position, before any door actor is necessarily known.
- `AssignDoorActorToConnection(RoomID, DoorActor, DoorWorldPos, Tolerance)` — matches an actual door actor to a pending connection by room ID + proximity, then triggers `RefreshDoorsForRoom`.

Both are driven by the generator's `LinkConnectorTransforms`, called once from `BP_DungeonManager`'s `EventBP_OnAllRoomsReady` handler — see §5 step 6.

---

## 4. `BP_RoomWrapper` — what a room actually does at runtime

Each spawned room gets one `BP_RoomWrapper` actor. Its real entry point is `OnLevelContentReady()` (bound to the room's level-streaming `OnLevelShown`), which:
1. Pulls the sub-level's actor list via `UETHLevelLibrary::GetActorsInLevel(GetLoadedLevel(LevelStreamingRef))` — a small C++ helper (`Source/EthericConcrete/ETHLevelLibrary.h/.cpp`) that exists because `GetAllActorsOfClass` does not work on streamed sub-levels.
2. For each actor: casts to `BP_BaseDoor_01` → adds to `RegisteredDoors`; on cast failure, casts to `BP_RoomVolume` → adds to `RoomVolumeActors` and dynamically binds its overlap delegates to `VolumeEntered`/`VolumeExited`.
3. Calls `DungeonManager.RegisterRoom(RoomID, self, RoomType, AdjacentRoomIDs, WorldLocation, RegisteredDoors.Length)`.

`VolumeEntered`/`VolumeExited` are the actual player-tracking events — they call `DungeonManager.OnPlayerEnterRoom`/`OnPlayerExitRoom` directly. There is no separate "player enter/exit" interface event; the room volume overlap **is** the trigger.

⚠️ **`RegisterRoom` is currently called with a hardcoded `RoomType` literal (`2`, generic/Active) from `OnLevelContentReady`**, not derived from the room's `PDA_RoomDefinition`/tags. `ReconcileMissingRooms` (§6) makes the same hardcoded assumption when re-registering a missed room. In practice this means Boss/Locked room *types* only get their special initial state through `SpawnNextRoom`'s separate room-type-determination path at spawn time — if that path and `OnLevelContentReady`'s registration ever disagree, a Boss or Locked room could register as a plain room. Worth double-checking if you're debugging a room that isn't behaving like its intended type.

### Dead scaffolding to ignore

`BP_RoomWrapper` does **not** implement the `BPI_DungeonRoom` interface (`Content/Blueprints/Procedural/Interface/BPI_DungeonRoom.uasset`), despite that interface declaring `InitializeRoom`, `SetRoomState`, `GetRoomState`, `NotifyEnemyDied`, `SetDoorState`, `OnPlayerEnter`, `OnPlayerExit`. Those look like a planned decoupling layer that was scaffolded and abandoned. `BPI_Door` (`SetDoorState`) is similarly unused. If you're tracing "who calls X" and X is one of these interface functions, stop — it's not wired to anything real. The real integration point everywhere is a direct reference to `DungeonManagerRef`.

`GetRoomState()` on the wrapper is real and implemented: it calls `DungeonManager.GetRoomState(RoomID)` (returns a byte) and converts it to the C++ enum `EETHRoomState` for display/Blueprint-switch convenience.

---

## 5. Generation flow, start to finish

```
EventBeginPlay (BP_DungeonGeneratorNEW)
  → StartDungeonGeneration
      CurrentParentRoomID = -1
      → SetDefaultValues     (finds the BP_DungeonManager in the world, resets counters)
      → SetSeed               (seeds Stream from Seed var, or randomizes if Seed == -1)
      → StartDungeonTimer     (1s fail-safe timer → CheckDungeonComplete; currently a no-op safety net)
      → setDefinitiveRooms    (RoomsPDAList += definiveRooms)
      → SpawnNextRoom         (recursive loop, see below)
```

`SpawnNextRoom`, per iteration:
1. Randomly pick an unused exit from `exitsList` (via the seeded `Stream`); `CurrentParentRoomID` = that exit's owning room.
2. `DetermineRoomType` → pick a room type / candidate PDA.
3. `checkOverlap` — if the candidate room would overlap something already placed, `Delay 0.2s` and retry.
4. `SpawnRoomOfType` → `LoadLevelInstance` on the PDA's `LevelReference` (loads the `.umap`), returns the loaded room + its PDA.
5. `SpawnActorFromClass BP_RoomWrapper` — spawns the runtime proxy for this room.
6. Calls `InitializeRoom` on the level-side room base class (`masterRoom`/`BP_RoomBase_Master`), passing the wrapper, PDA, spawned room, and room index. (Note: this is a *different* `InitializeRoom` from the unused `BPI_DungeonRoom.InitializeRoom` mentioned in §4 — don't confuse the two.)
7. Adds the wrapper to `RoomWrappers`, sets `OwnerID`, calls `AddExits` (adds this room's unused connectors to `exitsList` for future iterations).
8. If this room has a parent, calls `DungeonManager.AddAdjacentRoom(parent, thisRoom)`.
9. Decrements the remaining room count and loops until 0. On the last room: clears the fail-safe timer and calls `DungeonManager.SetTotalExpectedRooms(RoomWrappers.Length)`.

Meanwhile, as each spawned room's sub-level finishes streaming in, its `BP_RoomWrapper.OnLevelContentReady` fires independently (§4) and calls `RegisterRoom`, incrementing `BP_DungeonManager.ReadyRoomsCount`.

6. Once `ReadyRoomsCount == TotalExpectedRooms`, `BP_DungeonManager` fires its `EventBP_OnAllRoomsReady` custom event, which calls the generator's `LinkConnectorTransforms` (O(n²) proximity check between every pair of rooms' connectors, distance < 800 units = adjacent) — this is what populates the door connections described in §3, **after** generation is otherwise done.

### Adding a new room to the pool

- Designer-facing list: `definiveRooms` on `BP_DungeonGeneratorNEW` (array of `PDA_RoomDefinition`). Add your new PDA here — it gets appended into the live working array (`RoomsPDAList`) at generation start.
- A room = three things:
  1. A `.umap` under `Content/Blueprints/Procedural/Dungeon/Rooms_Templates/Level/` (existing examples: `Room_Building_Hangar.umap`, `Room_LShape/Room_LShape_Hangar_01.umap`, `Room_SquareMedium/Room_Building_SquareMedium.umap`, `Room_CorridorMedium/Room_CorridorMedium_Structure.umap`).
  2. A `PDA_RoomDefinition` data asset under `Content/Blueprints/Procedural/PDA/` (existing examples: `PDA_Room_Hangar_01`, `PDA_Room_SquareMedium_01`, `PDA_Room_SquareMedium_2Exits_01`, `PDA_Room_CorridorM_Empty_01`, `PDA_Room_Office_01`). Key fields: `LevelReference` (soft ref to the `.umap`), `Connectors` (array of `FStruct_RoomConnector` — one per entrance/exit, each carrying a local transform used to place doors and detect adjacency), `SelectionWeight`, `RoomTags`.
  3. Doors placed in the `.umap` at each connector's position, matching the `Connectors` array in the PDA.
- `BP_RoomWrapper` is generic and does not need modification per-room — it scans whatever the sub-level contains.

---

## 6. Debug tools

`BP_DungeonManager` has a `bDebugMode` variable, but note: **`EventBeginPlay` hardcodes `bDebugMode = true` unconditionally** on every play session, overriding whatever you set on the instance/CDO. If you need debug mode off, you currently have to remove/disable that node rather than just unchecking the variable.

With debug mode active:
- `DebugDrawRoomStates` runs on a 0.5s repeating timer — draws a colored sphere per room (green=open state, red=blocking) plus a text label, and a line per door connection colored by the same open/blocking logic `RefreshDoorsForRoom` uses.
- `DebugAutoClearRoom`: whenever `SetRoomState` transitions a room to `Active`, it stashes that room's ID in `DebugAutoClearRoomID` and fires this event, which waits 4 seconds then force-sets the room to `Cleared`. Useful for iterating on downstream state logic without needing real enemies. Remove/disable this if you're specifically testing enemy-driven clearing.
- `Debug_ForceRoomState()` — a `CallInEditor` function that force-sets `DebugTargetRoomID` to `DebugTargetState`. Set both variables on the instance in the Details panel, then click the button.

Independent of `bDebugMode`: `ReconcileMissingRooms` runs on a **1.0s repeating timer unconditionally** from `EventBeginPlay` — it scans the generator's `RoomWrappers` for any wrapper whose `RoomID` isn't yet in the manager's arrays and re-registers it (with the same hardcoded `RoomType = 2` caveat as §4). This exists as a recovery mechanism for a room-registration race that was fixed this session (see §7.2) — it's a safety net, not expected to normally do anything.

---

## 7. Known issues / traps

### 7.1 Orphaned C++ and content

- `Source/EthericConcrete/ETHDungeonManager.h/.cpp` (`AETHDungeonManager`) — the Actor class is dead (nothing derives from it anymore), **but the file also declares the `EETHRoomState`/`EETHRoomType` enums, which are still live-referenced** by `BP_RoomWrapper.GetRoomState()` and by `ETHMinimapWidget` (below). **Do not delete this header outright** — it would break both.
- `Source/EthericConcrete/ETHDungeonDebugger.h/.cpp` (`AETHDungeonDebugger`) — genuinely orphaned. `BP_DungeonManager` no longer has a `DebuggerRef` and has its own native `DebugDrawRoomStates`. Safe to consider fully dead, but do a final reference search before deleting.
- `Content/Blueprints/Procedural/BP_DungeonGenerator.uasset` (no "NEW" suffix) and `Content/Blueprints/Procedural/Dungeon/dungeonGenerator.uasset` — deprecated predecessors, not used by anything live.
- `BP_DungeonManager.T3D`, `BP_DungeonGeneratorNEW.T3D`, `BP_RoomWrapper.T3D` sitting next to the real `.uasset` files — these are **stale clipboard/export dumps** (the `BP_DungeonManager.T3D` still shows the pre-reparent `ParentClass=AETHDungeonManager`). Not live state, not standard Unreal-managed files — don't use them as reference, consider removing from source control.

### 7.2 Minimap is currently broken

`Source/EthericConcrete/ETHMinimapWidget.h/.cpp` hard-depends on finding a live `AETHDungeonManager` actor (via `TObjectIterator`) and binds to its `OnRoomStateChanged`/`OnRoomChanged` delegates. `Content/UI/Widgets/WBP_Minimap.uasset` is parented to this class. Since the 2026-07-23 reparent, **no actor of class `AETHDungeonManager` exists in the level anymore** (`BP_DungeonManager` is a plain `Actor`), so `TryFindDungeonManager()` never succeeds and the minimap silently never updates. This regression predates and is unrelated to today's room-registration/door fixes — it needs its own fix (either port `ETHMinimapWidget` to reference `BP_DungeonManager` directly, or rebuild the minimap logic in Blueprint).

### 7.3 Room-registration race — fixed 2026-07-25

Historically only some rooms would end up registered in `BP_DungeonManager`'s tracking arrays. Root cause: `FindRoomIndex`'s execute pin wasn't wired into `RegisterRoom`'s graph, so the lookup function was silently never invoked and duplicate/skipped registrations could occur. Fixed by wiring `FindRoomIndex` properly and gating all of `RegisterRoom`'s array-append logic behind `FindRoomIndex(RoomID) < 0` (i.e. "only register if not already present"). `ReconcileMissingRooms` (§6) was added as an additional safety net on top of the fix.

### 7.4 Misc

- One case in `SpawnRoomOfType`'s room-type switch (`NewEnumerator2`) has a hardcoded fallback level path (`Room_Building_SquareMedium_Convert`) instead of reading from the PDA — treat as a TODO if you're touching that switch.
- `Content/Blueprints/Procedural/Placeholders/Room_CorridorM_T/BP_Room_Building_CorridorM_TèPlaceholder.uasset` has a mojibake character in its filename (duplicate of the correctly-named file next to it) — cosmetic cleanup item.

---

## 8. Quick reference — "where do I look for X?"

| Question | Look at |
|---|---|
| Why is this door open/closed? | `BP_DungeonManager.RefreshDoorsForRoom`, `IsOpenState`/`IsBlockingState` |
| Why did this room's state change (or not)? | `BP_DungeonManager.SetRoomState` and its call sites (`OnPlayerEnterRoom`, `OnEnemyDied`, `UnlockRoom`) |
| How do rooms get spawned? | `BP_DungeonGeneratorNEW.SpawnNextRoom` |
| How does a room report itself in? | `BP_RoomWrapper.OnLevelContentReady` → `RegisterRoom` |
| How do rooms find their neighbors / doors get linked? | `BP_DungeonGeneratorNEW.LinkConnectorTransforms`, called from `BP_DungeonManager.EventBP_OnAllRoomsReady` |
| How do I add a new room? | §5 "Adding a new room to the pool" |
| Minimap not updating? | Known issue, §7.2 — not a quick fix, needs a real port |

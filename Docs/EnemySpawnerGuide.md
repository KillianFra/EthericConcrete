# Enemy Spawner — Implementation Guide (Combat Rooms)

Audience: whoever builds the enemy-spawning feature for combat rooms. This is a **design/implementation guide, not ground truth** — none of this exists yet. It proposes how to slot enemy spawning into the dungeon system documented in [`DungeonGenerationGuide.md`](DungeonGenerationGuide.md) without fighting its existing patterns. Read that doc first; this one assumes you already know the room state machine, the parallel-array manager, and `BP_RoomWrapper`'s scan-on-load flow.

Verified before writing this (2026-07-25):
- `Source/EthericConcrete/ETHDungeonManager.h` (the orphaned C++ class the room state machine was ported from) already declares the *intended* contract for enemy counts: `SetRoomEnemyCount(RoomID, Count)` — comment says "call after enemy spawn" — and `OnEnemyDied(RoomID)`, plus an `LivingEnemyCount` field on the per-room struct and an `EETHRoomType::Enemy` enum value. `BP_DungeonManager` was ported line-for-line from this class, so the BP almost certainly already has equivalents — **confirm the exact BP node names in the editor before wiring anything**, this doc can't verify live Blueprint state (MCP/editor connection was unavailable when this was written).
- There is **no enemy character content yet** (no BP under `Content/Blueprints/Characters/` currently hooks into the dungeon system) and **no death/kill signal exists anywhere in the codebase**. `UETHAttributeSet::PostGameplayEffectExecute` (`Source/EthericConcrete/ETHAttributeSet.cpp:141`) only clamps `Health` to `[0, MaxHealth]` — it does not broadcast anything when `Health` hits 0. Any enemy character you use today (e.g. `CBP_EGuardParent_01`, `BP_Sartan_01`) has no death event to hook — you have to add one.
- `E_RoomType.uasset` (the live Blueprint enum) currently has 4 values (`Normal`, `Special`, `Boss`, `Default`) — it does **not** match the 7-value C++ `EETHRoomType` enum (`Generic/Start/Enemy/Chest/Corridor/Boss/Locked`). Don't assume "Enemy" is a distinct room type in the live enum; check it in the editor. This guide uses **"combat room"** to mean "a room whose `SetRoomState` transitions include `Active`/`Combat`", regardless of which `RoomType` value that maps to today.

---

## 1. Where spawning fits in the state machine

From the room state table (§2 of the main guide):

| Value | State | Meaning here |
|---|---|---|
| 1 | Discovered | Room visible, no enemies active yet |
| 2 | Active | Player just entered — **this is the spawn trigger** |
| 3 | Combat | Enemies alive, doors blocked |
| 4 | Cleared | All enemies dead — doors open |

`OnPlayerEnterRoom(RoomID)` already sets rooms-with-enemies to `Combat` (or `Active` if none). The cleanest hook point is: **spawn happens once, the moment a room transitions to `Active`, before anything sets it to `Combat`.** Concretely:

1. Player enters the room's `BP_RoomVolume` → `BP_RoomWrapper.VolumeEntered` fires → calls `DungeonManager.OnPlayerEnterRoom(RoomID)` (existing flow, §4 of main guide).
2. `OnPlayerEnterRoom` needs to know *before* deciding `Active` vs `Combat` whether this room has enemies to spawn. Today it presumably decides `Combat` based on `LivingEnemyCount > 0`, which is 0 until something spawns — chicken-and-egg. Two ways to break it, pick one:
   - **(Recommended) Spawn-then-count.** Add a call from `BP_RoomWrapper` (not the manager) directly: `VolumeEntered` first calls a new `BeginEncounter()` function on itself (spawns the first wave from its own registered spawn points, see §2), which calls `DungeonManager.SetRoomEnemyCount(RoomID, SpawnedCount)` *before* calling `OnPlayerEnterRoom`. That way `OnPlayerEnterRoom`'s existing "enemies remaining → Combat" branch just works unmodified.
   - Alternative: have the manager call an event on the wrapper asking it to spawn, then read the count back. More round-trips, no real benefit — don't do this unless the wrapper can't reach the manager first (it can, it already holds `DungeonManagerRef`).
3. Only start an encounter once per room. Guard with a `bEncounterStarted` bool on the wrapper (or check `EnemySpawnPoints.Length > 0 AND LivingEnemyCount == 0 AND state was Discovered/Undiscovered`) so re-entering an already-cleared room doesn't respawn enemies.
4. **Enemies do not call the manager directly.** Each enemy's death calls a function on its own `BP_RoomWrapper` (`NotifyEnemyDied`, see §3.1) instead of `DungeonManager.OnEnemyDied(RoomID)`. This is the one deviation from the "wrapper is a thin pass-through to the manager" pattern used elsewhere (`VolumeEntered`/`VolumeExited` do call the manager directly) — it's necessary because multi-wave rooms (§3.2) need something in the middle deciding whether a given kill should actually surface to the manager as "cleared," and the manager itself has no concept of waves.

Don't spawn at `RegisterRoom` time (room-load time) — that's before the player has entered, defeats the "close doors during combat" tension, and racy with the manager's array indices still initializing (§6 of main guide notes `ReconcileMissingRooms` exists precisely because early-registration ordering is fragile).

---

## 2. Spawn points — follow the door/volume scan pattern exactly

`BP_RoomWrapper.OnLevelContentReady` already scans the sub-level's actor list once (via `UETHLevelLibrary::GetActorsInLevel`) and buckets actors by class: doors → `RegisteredDoors`, room volumes → `RoomVolumeActors` (§4 of main guide). Add enemy spawn points the same way — don't invent a second discovery mechanism.

1. **New actor class `BP_EnemySpawnPoint`** (plain `Actor`, under `Content/Blueprints/Procedural/` next to the other room-content actors). Minimal fields:
   - `EnemyClass` (soft class ref, `TSubclassOf<APawn>` or your enemy base) — which enemy to spawn here, per-instance override.
   - `WaveIndex` (int, default `0`) — which wave this point belongs to. A room where every spawn point is left at `0` is a single-wave room and behaves exactly as described everywhere else in this doc; multi-wave rooms just use higher values on some points (see §3.2). This is the only field needed to support waves — don't build a separate "wave definition" data asset, the spawn points *are* the wave definition.
   - No collision/mesh needed at runtime (it's a marker), but give it an editor-only billboard/arrow component so level designers can see and orient it in the `.umap`, same convention as door/connector placement.
2. In `OnLevelContentReady`'s existing per-actor cast chain, add a third branch: cast to `BP_EnemySpawnPoint` → append to a new `EnemySpawnPoints` array on the wrapper (parallel to `RegisteredDoors`/`RoomVolumeActors` — same pattern, not a new pattern).
3. Placement: designers drop `BP_EnemySpawnPoint` actors directly in the room's `.umap` (same file that holds the doors), same workflow as "Adding a new room to the pool" (§5 of main guide) — no PDA change needed, no generator change needed. This keeps the feature entirely content-side for anyone adding spawns to an existing room.

---

## 3. Enemy death signal — this is the part that doesn't exist yet

Per CLAUDE.md, all stat/ability changes go through GAS, and `UETHAttributeSet` is the shared attribute set. Reuse it rather than inventing a parallel health system for enemies:

1. Give your enemy base class (whatever `APawn`/`ACharacter` child enemies use — currently none of the `Eminence/` characters have an `AbilitySystemComponent`, check before assuming) an `IAbilitySystemInterface` + owned `UAbilitySystemComponent` + `UETHAttributeSet`, mirroring `AProtagonist` (`Protagonist.h`).
2. In `UETHAttributeSet::PostGameplayEffectExecute` (`ETHAttributeSet.cpp:141`), where `Health` is clamped after a hit, add: if `GetHealth() <= 0.f`, call a new `BlueprintImplementableEvent`/multicast delegate — e.g. `OnActorDied` — via `GetOwningActor()`. This is a **shared codepath for player and enemies**, so gate it (e.g. only broadcast if not already dead, and let `AProtagonist` vs. an enemy class implement `OnActorDied` differently — player triggers a game-over flow, enemy triggers §3.1 below).
3. Enemy's `OnActorDied` implementation (Blueprint, on your enemy class):
   - Play death animation/ragdoll/VFX/loot as needed (out of scope here).
   - Call `RoomWrapperRef.NotifyEnemyDied()` — **not** the manager (see §1 step 4 and §3.1). The enemy needs a reference to its own `BP_RoomWrapper` — set it at spawn time, same moment `RoomID` gets set (the spawner, §1 step 2, passes both into the enemy on `SpawnActor`, e.g. via an interface call or exposed variables right after spawn, same as `BP_RoomWrapper` gets its `RoomID` set post-spawn today per §5 step 5-6 of the main guide). The enemy itself never needs to know its `RoomID` or talk to `DungeonManagerRef` at all — keep it dumb, all room bookkeeping lives in the wrapper.
   - Destroy/disable self.

If reworking the shared attribute set for this feels too invasive to land alongside the rest of the feature, the fallback is a simpler enemy-only `Health` float + `TakeDamage` override with its own death broadcast — but that splits stat handling into two systems (GAS for player, ad-hoc for enemies) and contradicts the "all ability/stat changes go through GAS" rule in CLAUDE.md. Prefer the shared-attribute-set route unless you hit a concrete blocker.

### 3.1 `BP_RoomWrapper.NotifyEnemyDied()` — the gate between "an enemy died" and "the manager should care"

This function is the only thing that's allowed to call `DungeonManager.OnEnemyDied(RoomID)`. Its job: decrement local wave bookkeeping, and only forward to the manager when the kill should actually be allowed to close out the encounter.

```
NotifyEnemyDied():
    CurrentWaveRemaining -= 1
    if CurrentWaveRemaining > 0:
        DungeonManager.OnEnemyDied(RoomID)   // just an accurate decrement, room stays in Combat
        return
    // this wave is now empty
    if CurrentWaveIndex < TotalWaveCount - 1:
        CurrentWaveIndex += 1
        SpawnWave(CurrentWaveIndex)          // refreshes DungeonManager's displayed count, does NOT call OnEnemyDied
    else:
        DungeonManager.OnEnemyDied(RoomID)   // the true last kill — this is what's allowed to flip the room to Cleared
```

For a single-wave room (`TotalWaveCount == 1`, the default), this collapses to exactly the original single-call behavior — every death forwards straight to the manager. Multi-wave rooms only differ in that the wave-ending kill that *isn't* the final wave is deliberately swallowed instead of forwarded, so the manager's `LivingEnemyCount` never bottoms out at 0 mid-encounter and the room never prematurely opens its doors.

### 3.2 Multi-wave rooms

The naive "spawn everything once, flip a bool" approach breaks in two ways for multi-wave rooms: it can't sequence waves at all, and — even if you bolted sequencing on top — routing every individual death straight to `DungeonManager.OnEnemyDied` means the *first* wave hitting zero enemies would immediately zero-cross the manager's counter and clear the room (doors open) before wave 2 ever spawns. The fix is entirely in `BP_RoomWrapper`; neither the manager nor the enemy class needs to know waves exist.

State the wrapper needs, alongside `EnemySpawnPoints` (§2):
- `TotalWaveCount` (int) — computed once in `BeginEncounter()` as `Max(WaveIndex across EnemySpawnPoints) + 1`.
- `CurrentWaveIndex` (int) — starts at `0`.
- `CurrentWaveRemaining` (int) — set every time a wave spawns.

`BeginEncounter()` (replaces the plain "spawn everything" version from §1):
1. Compute `TotalWaveCount` from `EnemySpawnPoints`.
2. Call `SpawnWave(0)`.

`SpawnWave(WaveIndex)`:
1. Filter `EnemySpawnPoints` to those with matching `WaveIndex`.
2. `SpawnActorFromClass` each, passing `RoomWrapperRef = self` into the spawned enemy (not the manager — see §3).
3. `CurrentWaveIndex = WaveIndex`, `CurrentWaveRemaining = <number spawned>`.
4. `DungeonManager.SetRoomEnemyCount(RoomID, CurrentWaveRemaining)` — this is a plain setter with no side effects (per the C++ contract this was ported from, §0), so calling it again on every wave transition is safe and keeps any debug/minimap enemy-count display accurate per-wave instead of showing a stale total.

Design rooms with waves by placing spawn points with `WaveIndex = 0, 1, 2, …` in the `.umap` — no PDA, generator, or manager change required. A designer adding a third wave to an existing combat room just drops more `BP_EnemySpawnPoint` actors with `WaveIndex = 2`.

---

## 4. Wiring checklist

- [ ] `BP_EnemySpawnPoint` actor created (with `EnemyClass` + `WaveIndex`, default `0`), placed in target combat room `.umap`(s) at desired spawn transforms. Use higher `WaveIndex` values only for rooms that need multiple waves.
- [ ] `BP_RoomWrapper.OnLevelContentReady` cast chain extended with the `BP_EnemySpawnPoint` branch → `EnemySpawnPoints` array.
- [ ] `BP_RoomWrapper` gets `TotalWaveCount` / `CurrentWaveIndex` / `CurrentWaveRemaining` variables, a `BeginEncounter()` function (guarded by `bEncounterStarted`), a `SpawnWave(WaveIndex)` function, and `NotifyEnemyDied()` (§3.1) implementing the wave-gating logic.
- [ ] `VolumeEntered` updated to call `BeginEncounter()` before `DungeonManager.OnPlayerEnterRoom(RoomID)`.
- [ ] Enemy base class gets an `AbilitySystemComponent` + `UETHAttributeSet` (or confirms it already has one — check `CBP_EGuardParent_01` in the editor first, don't assume).
- [ ] `UETHAttributeSet::PostGameplayEffectExecute` broadcasts an `OnActorDied` event when `Health` reaches 0.
- [ ] Enemy's `OnActorDied` implementation calls `RoomWrapperRef.NotifyEnemyDied()` — **not** the manager directly.
- [ ] Confirm in-editor that `BP_DungeonManager`'s ported equivalents of `SetRoomEnemyCount`/`OnEnemyDied` exist with those names, and that `SetRoomEnemyCount` is a pure setter with no side effects (the design in §3.1/§3.2 depends on that) — the C++ header this was ported from is orphaned and may have drifted from the live Blueprint (same caution as the `RoomType` enum mismatch noted in §2/verification above).
- [ ] PIE test, single-wave room: enter a combat room → doors close (state `Active`→`Combat`, per existing `RefreshDoorsForRoom` AND-logic) → kill all spawned enemies → room transitions to `Cleared` → doors open → `DiscoverAdjacentRooms` fires as usual.
- [ ] PIE test, multi-wave room: kill wave 1 fully → confirm doors stay closed and room state stays `Combat` (does **not** flicker to `Cleared`) → wave 2 spawns → kill wave 2 → room transitions to `Cleared`, doors open.

---

## 5. What NOT to build

- Don't route spawning through `BP_DungeonManager`'s `RegisterRoom` or `ReconcileMissingRooms` — those are room-*registration* recovery paths, unrelated to enemy lifecycle, and adding enemy logic there couples two things that should stay decoupled.
- Don't touch `BPI_DungeonRoom` or `BPI_Door` to wire this — per §4/§7.1 of the main guide, both interfaces are dead scaffolding nothing implements. Follow the real pattern (direct `DungeonManagerRef` calls), not the abandoned one.
- Don't add a second "is room cleared" check — `OnEnemyDied`'s existing decrement-to-zero logic already flips the room to `Cleared`/`Discovered` (§2 of the main guide). `BP_RoomWrapper.NotifyEnemyDied()` (§3.1) is the only thing deciding *when* to let a kill reach that check; it never re-implements the state transition itself.
- Don't have enemies call `DungeonManager.OnEnemyDied` directly, and don't give the enemy class any awareness of waves or room state. Multi-wave gating only works if every kill funnels through one place (`BP_RoomWrapper.NotifyEnemyDied`) that can decide whether to forward it — scattering that decision into the enemy class means every enemy variant has to reimplement wave logic.

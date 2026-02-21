# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EthericConcrete is an Unreal Engine 5.7 dungeon-crawling action RPG with procedural generation. It's a mixed C++/Blueprint project — core systems (character, attributes) are in C++, while game logic, abilities, and content are primarily in Blueprints.

## Build Commands

The project uses UE5.7's build system. Build tasks are configured in `EthericConcrete.code-workspace` for VS Code:

```bash
# Build (Development, Win64) — via UE batch file
Engine\Build\BatchFiles\Build.bat EthericConcreteEditor Win64 Development "EthericConcrete.uproject" -WaitMutex -FromMsBuild

# Rebuild (clean + build)
Engine\Build\BatchFiles\Rebuild.bat EthericConcreteEditor Win64 Development "EthericConcrete.uproject" -WaitMutex -FromMsBuild

# Clean
Engine\Build\BatchFiles\Clean.bat EthericConcreteEditor Win64 Development "EthericConcrete.uproject" -WaitMutex -FromMsBuild
```

Available build configurations: Debug, DebugGame, Development, Shipping.

## Architecture

### C++ Module: `EthericConcrete` (Source/EthericConcrete/)

Single runtime module with dependencies on: Core, CoreUObject, Engine, InputCore, **GameplayAbilities**, **GameplayTags**, **GameplayTasks**.

**Key C++ classes:**

- **`AProtagonist`** (`Protagonist.h/cpp`) — Player character. Inherits `ACharacter` + `IAbilitySystemInterface`. Owns the `UAbilitySystemComponent` and `UETHAttributeSet`. Exposes `BlueprintImplementableEvent` delegates for health/anomaly changes.

- **`UETHAttributeSet`** (`ETHAttributeSet.h/cpp`) — GAS attribute set defining all character stats. Attributes are organized by category:
  - **Anomaly (ANM)**: `Anomaly`
  - **Corpus (COR)**: `Health`, `MaxHealth`, `Armor`, `WeaponDamage`
  - **Kinetic (KIN)**: `MoveSpeed`, `MaxMoveSpeed`, `WeaponAttackSpeed`, `MaxWeaponAttackSpeed`
  - **Focus (FCS)**: `DistanceWeaponAccuracy`, `CriticalChance`
  - **Synapse (SYN)**: `SpellDamage`, `SpellCooldownReduction`
  - **Echo (ECH)**: `CriticalDamage`, `SpellCooldownReduction_Echo`

  All attributes are replicated. Uses `PreAttributeChange` for pre-clamping and `PostGameplayEffectExecute` for post-effect logic.

### Blueprint Systems (Content/Blueprints/)

- **Procedural/** — Dungeon generation system with room-based architecture. Includes `BP_DungeonGeneratorNEW`, room interfaces (`BPI_DungeonRoom`), room state enum (`E_RoomState`), and `BP_RoomWrapper`.
- **Abilities/** — Ability implementations (melee combos, spells) built on GAS.
- **Characters/** — Character variants, distortions, movement modes.
- **Items/** — Weapons and equipment.
- **Spells/** — Spell implementations (e.g., Fireball).
- **StatsSystem/** — Stat enums and structures.
- **Environment/** — Level objects, prop slot system, structures.

### Gameplay Tags (Config/DefaultGameplayTags.ini)

```
Melee.ComboWindowOpen / ComboWindowClose  — Combo timing system
Melee.UnArmed.Punch.A01-A03              — Unarmed combo chain
Spell.Fireball                            — Fireball ability
```

### Key Plugins

- **GameplayAbilities** — Core ability framework (GAS)
- **Mover / ChaosMover** — Advanced character movement
- **SkeletalMerging** — Character mesh merging for equipment
- **UEGitPlugin-dev** — Git integration (in Plugins/)

## Development Patterns

- **All ability/stat changes must go through GAS** — use `UAbilitySystemComponent` and `UETHAttributeSet`. New attributes follow the `ATTRIBUTE_ACCESSORS` macro pattern with `ReplicatedUsing`.
- **C++ for systems, Blueprints for content** — Core mechanics in C++ with `BlueprintImplementableEvent`/`BlueprintReadOnly` for BP extensibility.
- **Network replication ready** — Attributes use `OnRep_` functions and `GetLifetimeReplicatedProps`.

## Git Workflow

- **Main branch**: `master`
- **Feature branches**: `EC-XXX-Description` naming convention
- **Git LFS** for binary assets (`.uasset`, `.umap`)
- Engine target: Win64, DirectX 12, fixed 60 FPS

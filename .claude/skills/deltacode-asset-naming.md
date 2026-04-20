# SKILL: deltacode-asset-naming
# DeltaCode Plugin — Asset Naming, Folder Structure, Lyra Conventions

## Purpose
This skill governs every asset name and folder path DeltaCode generates or references.
It implements Lyra naming conventions as the primary standard, with DeltaCode-specific
DC_ identifiers for all plugin-generated content.

This skill is referenced by all other skills whenever an asset name or path appears.
Treat these conventions as law — a violation breaks asset references, search, and migration.

---

## Naming Convention Sources (Priority Order)

1. **Epic C++ Coding Standard** — mandatory for all C++ identifiers
2. **Lyra Starter Game conventions** — primary for UE5.5+ asset prefixes
3. **Allar/Gamemakin UE5 Style Guide** — fills gaps Lyra doesn't address
4. **DeltaCode DC_ namespace** — applied on top for all plugin-generated assets

When sources conflict, the order above determines which wins.

---

## Asset Prefix Reference Table

| Asset Type | DeltaCode Prefix | Lyra Basis | Example |
|---|---|---|---|
| Blueprint Actor | `B_DC_` | `B_` | `B_DC_HealthPickup` |
| Blueprint Component | `B_DC_` | `B_` | `B_DC_InteractComp` |
| Widget Blueprint | `W_DC_` | `W_` | `W_DC_ObjectiveHUD` |
| Level / Map | `L_DC_` | `L_` | `L_DC_ExtractionTest` |
| Gameplay Ability | `GA_DC_` | `GA_` | `GA_DC_FireWeapon` |
| Gameplay Effect | `GE_DC_` | `GE_` | `GE_DC_ApplyDamage` |
| Gameplay Attribute Set | `AS_DC_` | `AS_` | `AS_DC_CombatStats` |
| Static Mesh | `SM_DC_` | `SM_` | `SM_DC_Crate_01` |
| Skeletal Mesh | `SK_DC_` | `SK_` | `SK_DC_Enemy_Grunt` |
| Material | `M_DC_` | `M_` | `M_DC_EnemySkin` |
| Material Instance | `MI_DC_` | `MI_` | `MI_DC_EnemySkin_Red` |
| Material Function | `MF_DC_` | `MF_` | `MF_DC_Dissolve` |
| Texture (base color) | `T_DC_` + `_D` suffix | `T_` | `T_DC_Enemy_Grunt_D` |
| Texture (normal) | `T_DC_` + `_N` suffix | `T_` | `T_DC_Enemy_Grunt_N` |
| Texture (roughness) | `T_DC_` + `_R` suffix | `T_` | `T_DC_Enemy_Grunt_R` |
| Niagara System | `NS_DC_` | `NS_` | `NS_DC_BloodSplatter` |
| Sound Wave | `S_DC_` | `S_` | `S_DC_Pickup_Collect` |
| MetaSound Source | `MSS_DC_` | `MSS_` | `MSS_DC_EnemyFootstep` |
| Data Table | `DT_DC_` | `DT_` | `DT_DC_EnemyStats` |
| Data Asset | `DA_DC_` | `DA_` | `DA_DC_MissionConfig` |
| Input Action | `IA_DC_` | `IA_` | `IA_DC_Interact` |
| Input Mapping Context | `IMC_DC_` | `IMC_` | `IMC_DC_Default` |
| Animation Blueprint | `ABP_DC_` | `ABP_` | `ABP_DC_Enemy_Grunt` |
| Animation Montage | `AM_DC_` | `AM_` | `AM_DC_Enemy_Attack` |
| Behavior Tree | `BT_DC_` | `BT_` | `BT_DC_EnemyPatrol` |
| Blackboard | `BB_DC_` | `BB_` | `BB_DC_EnemyAI` |
| Physics Asset | `PHYS_DC_` | `PHYS_` | `PHYS_DC_Enemy_Grunt` |
| Curve | `Curve_DC_` | — | `Curve_DC_HealthRegen` |
| Enum (Blueprint) | `E_DC_` | — | `E_DC_MissionState` |
| Structure (Blueprint) | `F_DC_` | — | `F_DC_LootEntry` |

---

## Full Asset Name Pattern

```
[TypePrefix]_[DC_][BaseName]_[Descriptor]_[OptionalVariant]
```

Examples:
- `B_DC_EnemyBase` — Blueprint, DeltaCode, Enemy base class
- `B_DC_Enemy_Grunt_01` — Blueprint, DeltaCode, Enemy, Grunt variant 1
- `T_DC_Rock_Wall_D` — Texture, DeltaCode, Rock Wall, Diffuse channel
- `MI_DC_EnemySkin_Blue` — Material Instance, DeltaCode, Enemy Skin, Blue variant
- `L_DC_Mission_Extraction_01` — Level, DeltaCode, Mission type, variant

### Variant Numbering
- Use two-digit numeric variants: `_01`, `_02` not `_1`, `_2`
- Only add variant suffix when multiple versions exist — don't pad single assets

---

## Texture Channel Suffixes

| Channel | Suffix | Notes |
|---|---|---|
| Base Color / Diffuse | `_D` | Universal |
| Normal Map | `_N` | Universal |
| Roughness | `_R` | Universal |
| Metallic | `_MT` | Avoids collision with Material prefix `M_` |
| Emissive | `_E` | Universal |
| Ambient Occlusion | `_AO` | Universal |
| Packed (e.g. R+AO+E) | `_RAE` | Combine channel suffixes |

Packing 4 channels (RGBA) is NOT recommended except for alpha in diffuse textures —
alpha textures incur additional overhead at runtime.

---

## Content Folder Structure

All DeltaCode content lives under a single top-level namespace folder.
This prevents migration conflicts and content browser pollution.

```
Content/
└── DeltaCode/                           ← All plugin content here. Never outside.
    ├── Core/                            ← Base Blueprint children of C++ classes
    │   ├── B_DC_PickupBase
    │   ├── B_DC_ObjectiveBase
    │   ├── B_DC_EnemyBase
    │   ├── B_DC_EnemyRanged
    │   ├── B_DC_BossBase
    │   ├── B_DC_SpawnZone
    │   ├── B_DC_ExtractionZone
    │   └── B_DC_WeaponBase
    ├── Missions/                        ← Mission template content
    │   ├── Extraction/
    │   │   ├── L_DC_Mission_Extraction_01
    │   │   └── B_DC_ExtractionDirector
    │   ├── Destiny/
    │   │   ├── L_DC_Mission_Destiny_01
    │   │   └── B_DC_ArenaDirector
    │   ├── Fallout/
    │   │   ├── L_DC_Mission_Fallout_01
    │   │   └── B_DC_QuestGiverNPC
    │   └── OpenWorld/
    │       ├── L_DC_Mission_OpenWorld_01
    │       ├── B_DC_POI_Base
    │       └── B_DC_TownHub
    ├── Weapons/                         ← Weapon Blueprint children
    │   ├── B_DC_Weapon_Rifle_01
    │   └── B_DC_Weapon_Pistol_01
    ├── Pickups/                         ← Pickup Blueprint children
    │   ├── B_DC_Pickup_Ammo
    │   ├── B_DC_Pickup_Health
    │   └── B_DC_Pickup_Armor
    ├── Enemies/                         ← Enemy Blueprint children
    │   ├── B_DC_Enemy_Grunt_01
    │   └── B_DC_Enemy_Heavy_01
    ├── UI/                              ← Widget Blueprints
    │   ├── W_DC_ObjectiveHUD
    │   ├── W_DC_MissionComplete
    │   └── W_DC_BossHealthBar
    ├── Input/                           ← Input Actions and Mapping Contexts
    │   ├── IA_DC_Interact
    │   └── IMC_DC_Default
    ├── Data/                            ← Data Tables, Data Assets
    │   ├── DT_DC_EnemyStats
    │   └── DA_DC_MissionConfig_Extraction
    └── Python/                          ← Danger Zone scripting
        └── dc_danger_zone.py
```

### Folder Rules (Allar + Lyra)
- Organize by **feature/game domain**, not by asset type
  ✓ `Content/DeltaCode/Enemies/` — correct
  ✗ `Content/DeltaCode/Blueprints/` — wrong (type-based folder)
- Folder names use PascalCase, no spaces, no underscores
- Never create folders named `Assets`, `Meshes`, `Textures`, `Materials`
  — these are redundant given prefix conventions and Content Browser filters
- The `Core/` folder is PROTECTED — Danger Zone never deletes or overwrites Core assets
- Developer sandbox assets go in `Content/DeltaCode/Developers/[YourName]/`

---

## C++ Identifier Naming (Epic Standard)

All code identifiers — not just assets — follow Epic's mandatory standard:

```
Classes:        PascalCase + type prefix
                ADCEnemyBase, UDCHealthComponent, FDCMissionData

Variables:      PascalCase, bool with b prefix
                MaxHealth, PatrolRadius, bIsDead, bHasActivated

Functions:      PascalCase verb phrases
                InitializeEnemy(), OnPickedUp(), ApplyDamage()

Enums:          E prefix, PascalCase values
                EDCMissionState::Active, EDCBossPhase::Enraged

Constants:      PascalCase (no ALL_CAPS in UE5 standard)
                MaxEnemiesPerWave, DefaultPatrolRadius

Parameters:     PascalCase, In/Out prefixes where helpful
                InDamageAmount, OutResultMessage

Private members: PascalCase (no m_ prefix — UE convention differs from standard C++)
```

---

## What DeltaCode Generates vs What It References

| Asset | DeltaCode generates | DeltaCode references |
|---|---|---|
| C++ class files | Yes — writes .h/.cpp to Source/ | Via module API macro |
| Blueprint assets | Describes setup — user creates in Editor | By content path string |
| Levels | Via Python Danger Zone script | By L_DC_ name |
| Data Tables | Describes row structure — user populates | By DT_DC_ path |
| Materials/Textures | Never generates — references placeholder | By T_DC_ / M_DC_ path |

DeltaCode NEVER generates binary .uasset files directly.
Blueprint descriptions are always text instructions for the user to implement.

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Asset names without the DC_ namespace identifier (e.g., `B_Enemy` instead of `B_DC_Enemy`)
❌ Type-based top-level folders (`Content/Blueprints/`, `Content/Textures/`)
❌ Assets placed outside `Content/DeltaCode/` namespace
❌ Old BP_ prefix instead of Lyra B_ prefix
❌ Old WBP_ prefix instead of Lyra W_ prefix  
❌ Mixed conventions — once Lyra prefixes are chosen, apply them everywhere
❌ Spaces in asset names or folder names
❌ Lowercase starting characters in any asset or folder name
❌ Deleting or modifying anything in `Content/DeltaCode/Core/` from Danger Zone

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Epic C++ Coding Standard, Lyra Starter Game analysis,
         Allar/ue5-style-guide, Tom Looman naming guide, Epic Asset Naming Docs
Plugin: DeltaCode
Last Updated: 2026-03

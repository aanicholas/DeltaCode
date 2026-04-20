# SKILL: ue5-level-scripting
# DeltaCode Plugin — Danger Zone Level Scripting, Python Bridge, Actor Spawning

## Purpose
This skill governs everything DeltaCode does when operating in Danger Zone mode:
clearing a level, spawning mission actors, placing navigation meshes, setting up
lighting, and orchestrating the Python scripting pipeline that drives procedural
level authoring from the Editor.

This skill is ONLY active in Danger Zone mode. It must never be invoked by Safe Mode
operations. Safe Mode cannot delete actors — this is a hard constraint.

---

## Danger Zone Pipeline Overview

```
User selects mission type + clicks Generate
        ↓
DeltaCodeEditor: confirms via modal dialog
        ↓
DeltaCodeEditorModule calls UDCLevelScriptingBridge::ExecuteDangerZoneScript()
        ↓
C++ bridge invokes Python via IPythonScriptPlugin::Get()->ExecPythonCommand()
        ↓
Python script (dc_danger_zone.py):
  1. Clear all non-essential actors
  2. Set level lighting/sky baseline
  3. Spawn layout volumes and navigation mesh
  4. Spawn DC actor classes based on mission template
  5. Set actor properties (location, rotation, scale)
  6. Compile nav mesh
        ↓
Return success/failure JSON to C++ bridge
        ↓
Display result in DeltaCode panel
```

---

## C++ Python Bridge

### UDCLevelScriptingBridge.h
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DCLevelScriptingBridge.generated.h"

UENUM(BlueprintType)
enum class EDCMissionTemplate : uint8
{
    Extraction      UMETA(DisplayName = "Extraction Shooter"),
    Destiny         UMETA(DisplayName = "Destiny-Style Mission"),
    Fallout         UMETA(DisplayName = "Fallout-Style Mission"),
    OpenWorldRPG    UMETA(DisplayName = "Open World RPG")
};

UCLASS()
class DELTACODEEDITOR_API UDCLevelScriptingBridge : public UObject
{
    GENERATED_BODY()

public:
    // Main entry point for Danger Zone operations
    static bool ExecuteDangerZoneScript(EDCMissionTemplate MissionTemplate,
                                        FString& OutResultMessage);

    // Validates Python plugin is available before attempting anything
    static bool IsPythonAvailable();

private:
    static FString BuildPythonCommandString(EDCMissionTemplate MissionTemplate);
    static FString GetScriptPath();
};
```

### UDCLevelScriptingBridge.cpp
```cpp
// Copyright DeltaCode. All Rights Reserved.
#include "DCLevelScriptingBridge.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Paths.h"

bool UDCLevelScriptingBridge::IsPythonAvailable()
{
    return IPythonScriptPlugin::IsAvailable() &&
           IPythonScriptPlugin::Get()->IsPythonAvailable();
}

bool UDCLevelScriptingBridge::ExecuteDangerZoneScript(
    EDCMissionTemplate MissionTemplate, FString& OutResultMessage)
{
    if (!IsPythonAvailable())
    {
        OutResultMessage = TEXT("Python Script Plugin is not enabled. "
                                "Enable it in Edit > Plugins > Scripting > Python.");
        return false;
    }

    FString Command = BuildPythonCommandString(MissionTemplate);
    IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);
    OutResultMessage = TEXT("Danger Zone script executed successfully.");
    return true;
}

FString UDCLevelScriptingBridge::BuildPythonCommandString(EDCMissionTemplate Template)
{
    FString ScriptPath = GetScriptPath();
    FString TemplateName;

    switch (Template)
    {
        case EDCMissionTemplate::Extraction:    TemplateName = TEXT("extraction");    break;
        case EDCMissionTemplate::Destiny:       TemplateName = TEXT("destiny");       break;
        case EDCMissionTemplate::Fallout:       TemplateName = TEXT("fallout");       break;
        case EDCMissionTemplate::OpenWorldRPG:  TemplateName = TEXT("openworld");     break;
    }

    return FString::Printf(
        TEXT("import importlib.util; "
             "spec = importlib.util.spec_from_file_location('dc_dz', r'%s'); "
             "mod = importlib.util.module_from_spec(spec); "
             "spec.loader.exec_module(mod); "
             "mod.run_danger_zone('%s')"),
        *ScriptPath, *TemplateName);
}

FString UDCLevelScriptingBridge::GetScriptPath()
{
    // Scripts live in the plugin's Content/Python folder
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectPluginsDir(),
                        TEXT("DeltaCode/Content/Python/dc_danger_zone.py")));
}
```

---

## Python Script — dc_danger_zone.py

This script runs inside UE5's embedded Python interpreter.
All `unreal.*` imports are available in this context.

```python
# DeltaCode Danger Zone Script
# Copyright DeltaCode. All Rights Reserved.
# Runs inside UE5 Editor via IPythonScriptPlugin

import unreal

# ─── CONSTANTS ────────────────────────────────────────────────────────────────

# Actor classes to PRESERVE when clearing a level (never delete these)
PRESERVE_CLASSES = [
    unreal.DirectionalLight,
    unreal.SkyAtmosphere,
    unreal.VolumetricCloud,
    unreal.SkyLight,
    unreal.ExponentialHeightFog,
    unreal.PostProcessVolume,
]

# DeltaCode C++ actor class paths in the plugin content
DC_CLASSES = {
    "pickup_base":      "/DeltaCode/Core/B_DC_PickupBase",
    "objective":        "/DeltaCode/Core/B_DC_ObjectiveBase",
    "enemy_base":       "/DeltaCode/Core/B_DC_EnemyBase",
    "boss_base":        "/DeltaCode/Core/B_DC_BossBase",
    "spawn_zone":       "/DeltaCode/Core/B_DC_SpawnZone",
    "extraction_zone":  "/DeltaCode/Core/B_DC_ExtractionZone",
}

# ─── UTILITIES ────────────────────────────────────────────────────────────────

def get_editor_world():
    return unreal.EditorLevelLibrary.get_editor_world()

def clear_level():
    """Delete all non-essential actors from the current level."""
    world = get_editor_world()
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors()

    actors_to_delete = []
    for actor in all_actors:
        should_preserve = False
        for preserve_class in PRESERVE_CLASSES:
            if isinstance(actor, preserve_class):
                should_preserve = True
                break
        if not should_preserve:
            actors_to_delete.append(actor)

    with unreal.ScopedEditorTransaction("DeltaCode: Clear Level") as trans:
        for actor in actors_to_delete:
            actor.destroy_actor()

    unreal.log(f"DeltaCode: Cleared {len(actors_to_delete)} actors.")

def spawn_actor(class_path, location, rotation=None, label=None):
    """Spawn a Blueprint actor by content path."""
    if rotation is None:
        rotation = unreal.Rotator(0, 0, 0)

    asset = unreal.load_asset(class_path)
    if asset is None:
        unreal.log_warning(f"DeltaCode: Could not load asset at {class_path}")
        return None

    loc = unreal.Vector(location[0], location[1], location[2])
    rot = unreal.Rotator(rotation[0], rotation[1], rotation[2])

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(asset, loc, rot)
    if actor and label:
        actor.set_actor_label(label)
    return actor

def rebuild_navmesh():
    """Rebuild navigation mesh after level changes."""
    unreal.NavigationSystemV1.build_paths(get_editor_world())
    unreal.log("DeltaCode: Navigation mesh rebuilt.")

# ─── MISSION TEMPLATES ────────────────────────────────────────────────────────

def build_extraction(world):
    """
    Extraction Shooter layout:
    - 3 loot zones with pickup clusters
    - 8 enemy patrol positions around the map
    - 1 extraction zone at the far end
    - 1 optional boss guarding the extraction
    """
    with unreal.ScopedEditorTransaction("DeltaCode: Build Extraction Mission"):

        # Loot zones (3 clusters)
        loot_positions = [
            (-2000, 0, 0), (0, 2000, 0), (2000, -1000, 0)
        ]
        for i, pos in enumerate(loot_positions):
            for j in range(3):
                offset = (j * 100, j * 100, 0)
                loc = (pos[0] + offset[0], pos[1] + offset[1], pos[2])
                spawn_actor(DC_CLASSES["pickup_base"], loc,
                           label=f"DC_LootPickup_{i}_{j}")

        # Enemy patrol positions
        enemy_positions = [
            (-1500, 500, 0), (-500, 1500, 0), (500, 2500, 0),
            (1500, 1500, 0), (2500, 500, 0), (2500, -500, 0),
            (1500, -1500, 0), (500, -2000, 0)
        ]
        for i, pos in enumerate(enemy_positions):
            spawn_actor(DC_CLASSES["enemy_base"], pos,
                       label=f"DC_Enemy_{i:02d}")

        # Extraction zone
        spawn_actor(DC_CLASSES["extraction_zone"], (4000, 0, 0),
                   label="DC_ExtractionZone")

        # Boss guarding extraction
        spawn_actor(DC_CLASSES["boss_base"], (3500, 0, 0),
                   rotation=(0, 180, 0), label="DC_ExtractionBoss")

        # Mission objective
        spawn_actor(DC_CLASSES["objective"], (0, 0, 0),
                   label="DC_Objective_ReachExtraction")

    rebuild_navmesh()
    unreal.log("DeltaCode: Extraction mission built.")

def build_destiny(world):
    """
    Destiny-style Arena Mission:
    - Linear path of 3 encounter arenas
    - Each arena: wave spawner + objective trigger
    - Final arena: boss room with ADCBossBase
    - Chest reward actor after boss
    """
    with unreal.ScopedEditorTransaction("DeltaCode: Build Destiny Mission"):

        # Arena positions along X axis (each 3000 units apart)
        arenas = [0, 3000, 6000]
        for i, x in enumerate(arenas):
            # Wave spawn zone
            spawn_actor(DC_CLASSES["spawn_zone"], (x, 0, 0),
                       label=f"DC_Arena{i+1}_SpawnZone")
            # Objective trigger for this arena
            spawn_actor(DC_CLASSES["objective"], (x, 0, 200),
                       label=f"DC_Arena{i+1}_Objective")
            # Enemies: 4 per arena
            for j in range(4):
                offset_x = (j % 2) * 400 - 200
                offset_y = (j // 2) * 400 - 200
                spawn_actor(DC_CLASSES["enemy_base"],
                           (x + offset_x, offset_y, 0),
                           label=f"DC_Arena{i+1}_Enemy_{j}")

        # Boss arena
        boss_x = 9000
        spawn_actor(DC_CLASSES["boss_base"], (boss_x, 0, 0),
                   label="DC_FinalBoss")
        spawn_actor(DC_CLASSES["objective"], (boss_x, 0, 200),
                   label="DC_Objective_DefeatBoss")
        spawn_actor(DC_CLASSES["pickup_base"], (boss_x + 500, 0, 0),
                   label="DC_BossChestReward")

    rebuild_navmesh()
    unreal.log("DeltaCode: Destiny mission built.")

def build_fallout(world):
    """
    Fallout-Style Mission:
    - Central hub with quest giver NPC
    - 3 quest objective locations in the world
    - Ambient enemy patrols between objectives
    - Optional boss at final objective
    - Loot scattered throughout
    """
    with unreal.ScopedEditorTransaction("DeltaCode: Build Fallout Mission"):

        # Quest hub / starting area
        spawn_actor(DC_CLASSES["objective"], (0, 0, 0),
                   label="DC_QuestGiver_NPC")

        # 3 quest objectives spread around the map
        objective_positions = [
            (3000, 2000, 0), (-2000, 3000, 0), (4000, -2000, 0)
        ]
        for i, pos in enumerate(objective_positions):
            spawn_actor(DC_CLASSES["objective"], pos,
                       label=f"DC_QuestObjective_{i+1}")
            # 2 enemies guarding each objective
            spawn_actor(DC_CLASSES["enemy_base"],
                       (pos[0] - 300, pos[1], 0), label=f"DC_Guard_{i}_A")
            spawn_actor(DC_CLASSES["enemy_base"],
                       (pos[0] + 300, pos[1], 0), label=f"DC_Guard_{i}_B")
            # Loot near each objective
            spawn_actor(DC_CLASSES["pickup_base"],
                       (pos[0], pos[1] + 200, 0), label=f"DC_Loot_{i}")

        # Ambient roaming enemies
        ambient_positions = [
            (1500, 0, 0), (-1000, 1500, 0), (2500, -500, 0), (0, -2000, 0)
        ]
        for i, pos in enumerate(ambient_positions):
            spawn_actor(DC_CLASSES["enemy_base"], pos,
                       label=f"DC_AmbientEnemy_{i}")

        # Final objective boss
        spawn_actor(DC_CLASSES["boss_base"], (4000, -2000, 200),
                   label="DC_FinalObjectiveBoss")

    rebuild_navmesh()
    unreal.log("DeltaCode: Fallout mission built.")

def build_openworld_rpg(world):
    """
    Open World RPG (Skyrim/Elden Ring/Witcher/RDR2 hybrid):
    - Central town / safe zone with quest giver
    - 4 Points of Interest (POI) spread across the map
    - Dynamic enemy camps between POIs
    - One dungeon entrance objective
    - Final boss at dungeon end
    - Loot and ambient pickups throughout
    - Trigger volumes for narrative events
    """
    with unreal.ScopedEditorTransaction("DeltaCode: Build Open World RPG"):

        # Central town / safe hub
        spawn_actor(DC_CLASSES["objective"], (0, 0, 0),
                   label="DC_TownHub_QuestGiver")
        for i in range(3):
            angle_offset = i * 200
            spawn_actor(DC_CLASSES["pickup_base"],
                       (angle_offset - 200, 300, 0),
                       label=f"DC_Town_Merchant_{i}")

        # 4 Points of Interest
        poi_positions = [
            (6000, 0, 0),    # East POI — ruined fort
            (-6000, 0, 0),   # West POI — abandoned mine
            (0, 6000, 0),    # North POI — enemy stronghold
            (0, -6000, 0),   # South POI — ancient shrine
        ]
        poi_labels = ["RuinedFort", "AbandonedMine", "EnemyStronghold", "AncientShrine"]

        for i, (pos, label) in enumerate(zip(poi_positions, poi_labels)):
            spawn_actor(DC_CLASSES["objective"], pos,
                       label=f"DC_POI_{label}_Objective")
            # Enemy camp at each POI (3-5 enemies)
            for j in range(3):
                e_offset = (j * 300 - 300, j * 200, 0)
                spawn_actor(DC_CLASSES["enemy_base"],
                           (pos[0] + e_offset[0], pos[1] + e_offset[1], 0),
                           label=f"DC_{label}_Enemy_{j}")
            # Loot at POI
            spawn_actor(DC_CLASSES["pickup_base"],
                       (pos[0] + 100, pos[1] + 100, 0),
                       label=f"DC_{label}_Loot")

        # Enemy camps between POIs (mid-map, 4 camps)
        camp_positions = [
            (3000, 3000, 0), (-3000, 3000, 0),
            (-3000, -3000, 0), (3000, -3000, 0)
        ]
        for i, pos in enumerate(camp_positions):
            for j in range(4):
                offset = (j * 250 - 375, 0, 0)
                spawn_actor(DC_CLASSES["enemy_base"],
                           (pos[0] + offset[0], pos[1], 0),
                           label=f"DC_Camp{i}_Enemy_{j}")

        # Dungeon entrance (narrative trigger objective)
        spawn_actor(DC_CLASSES["objective"], (8000, 8000, 0),
                   label="DC_DungeonEntrance_Trigger")

        # Mini-bosses at two POIs
        spawn_actor(DC_CLASSES["boss_base"],
                   (poi_positions[2][0], poi_positions[2][1], 0),
                   label="DC_Stronghold_MiniBoss")

        # Final boss at dungeon end
        spawn_actor(DC_CLASSES["boss_base"], (9000, 9000, 0),
                   label="DC_FinalDungeon_Boss")

        # Ambient loot scattered across open world
        ambient_loot = [
            (1500, 1500, 0), (-1500, 1500, 0), (2000, -1000, 0),
            (-2000, -2000, 0), (4000, 2000, 0), (-4000, -1000, 0)
        ]
        for i, pos in enumerate(ambient_loot):
            spawn_actor(DC_CLASSES["pickup_base"], pos,
                       label=f"DC_AmbientLoot_{i}")

    rebuild_navmesh()
    unreal.log("DeltaCode: Open World RPG framework built.")

# ─── ENTRY POINT ──────────────────────────────────────────────────────────────

def run_danger_zone(mission_template: str):
    """
    Main entry point called from C++ bridge.
    mission_template: 'extraction' | 'destiny' | 'fallout' | 'openworld'
    """
    world = get_editor_world()
    if world is None:
        unreal.log_error("DeltaCode: No editor world found. Open a level first.")
        return

    unreal.log(f"DeltaCode: Starting Danger Zone — {mission_template}")

    # Step 1: Clear non-essential actors
    clear_level()

    # Step 2: Build mission layout
    builders = {
        "extraction": build_extraction,
        "destiny":    build_destiny,
        "fallout":    build_fallout,
        "openworld":  build_openworld_rpg,
    }

    builder = builders.get(mission_template)
    if builder is None:
        unreal.log_error(f"DeltaCode: Unknown mission template '{mission_template}'")
        return

    builder(world)
    unreal.log(f"DeltaCode: Danger Zone complete — {mission_template}")
```

---

## Safe Mode Restriction — Hard Constraint

The following operations are PERMANENTLY LOCKED to Danger Zone mode.
DeltaCode MUST REFUSE these operations when Safe Mode is active:

- `clear_level()` — deletes actors
- `actor.destroy_actor()` — deletes a specific actor
- Any call to `unreal.EditorLevelLibrary` that removes or replaces existing actors
- Any Python command containing `destroy_actor` or `delete`

Safe Mode MAY:
- Spawn new actors (additive only)
- Modify properties on NEW actors it just created
- Generate C++ and Blueprint code files (no level mutation)

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Calling `destroy_actor` in Safe Mode under any circumstance
❌ Modifying existing level actors' properties without user confirmation
❌ Using `unreal.EditorLevelLibrary.get_all_level_actors()` in a tight loop
   without progress reporting — freezes the editor on large levels
❌ Running Danger Zone without the ScopedEditorTransaction wrapper — breaks Undo
❌ Spawning actors at Z=0 blindly — always account for ground height
❌ Forgetting `rebuild_navmesh()` after any spawn operation — AI will not pathfind
❌ Hardcoding class paths as strings without checking asset existence first

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Epic Python Editor Scripting docs, unreal.EditorLevelLibrary API
Plugin: DeltaCode
Last Updated: 2026-03

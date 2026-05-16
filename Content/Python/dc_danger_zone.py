# DeltaCode - Unreal Engine Code Helper
# Copyright (c) 2026 Andrew Nicholas
#
# This program is free software: you can redistribute
# it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software
# Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# DeltaCode Danger Zone — level-clear + rebuild pipeline.
#
# This script runs inside UE's embedded Python interpreter, invoked by
# FDCLevelScriptingBridge::ExecuteDangerZoneScript via importlib. It is ONLY
# reached after the editor-side confirmation modal (Hard Constraint #2).
#
# Uses UE5.5+ editor subsystems (EditorActorSubsystem, UnrealEditorSubsystem)
# instead of the deprecated EditorLevelLibrary shim — per the project's
# "prefer newer engine features" directive in CLAUDE.md.

import unreal

# ─── CONSTANTS ───────────────────────────────────────────────────────────────

# Actors that must survive a Danger Zone clear. Anything else in the level is
# treated as disposable mission content and removed.
_PRESERVE_CLASSES = (
    unreal.DirectionalLight,
    unreal.SkyAtmosphere,
    unreal.VolumetricCloud,
    unreal.SkyLight,
    unreal.ExponentialHeightFog,
    unreal.PostProcessVolume,
    unreal.NavMeshBoundsVolume,
)

# DeltaCode Blueprint classes in the plugin's protected Core folder. These
# are referenced by soft path so the script doesn't hard-fail if one asset is
# missing — spawn_actor logs a warning and continues.
# Floor dimensions per template: (width_cm, depth_cm, center_x, center_y).
# The engine Plane mesh is 100×100 cm at scale 1.
_FLOOR_CONFIGS = {
    "extraction":    (8000,  3000,  2000,  0),
    "arena":         (14000, 3000,  7000,  0),
    "questhub":      (12000, 12000, 0,     0),
    "reactivestory": (25000, 25000, 0,     0),
}

DC_CLASSES = {
    "pickup_base":     "/Game/DeltaCode/Core/B_DC_PickupBase.B_DC_PickupBase_C",
    "objective":       "/Game/DeltaCode/Core/B_DC_ObjectiveBase.B_DC_ObjectiveBase_C",
    # DeltaCode enemy/boss BPs parented off ADCEnemyBase. Possessed by
    # ADCEnemyAIController, running BT_DC_Enemy with BB_DC_Enemy_Default —
    # the BT/BB pair is duplicated from the plugin template by
    # dc_create_ai_assets.py on every Build Mission. Lyra's BP_CombatEnemy
    # was the prior default but it depends on a Lyra Experience init pass
    # we don't currently provide outside Experience context, so enemies
    # spawned manually ended up visible but inert. P3 (B_DC_LyraEnemyBase
    # parented off ALyraCharacter) will restore Lyra-native polish.
    "enemy_base":      "/Game/DeltaCode/Core/B_DC_EnemyBase.B_DC_EnemyBase_C",
    "boss_base":       "/Game/DeltaCode/Core/B_DC_BossBase.B_DC_BossBase_C",
    "spawn_zone":      "/Game/DeltaCode/Core/B_DC_SpawnZone.B_DC_SpawnZone_C",
    "extraction_zone": "/Game/DeltaCode/Core/B_DC_ExtractionZone.B_DC_ExtractionZone_C",
}

# Blueprint class paths whose instances already carry mesh / anim / AI / BT
# from the template. _apply_mesh_to_actor skips them entirely so we don't
# overwrite the template's intended setup. Match is done via substring so
# either the package path or `.ClassName_C` form hits.
#
# Substring-match list. When a spawned actor's class path contains any of
# these, _apply_mesh_to_actor leaves the actor alone so the BP's intentional
# mesh / anim / AI setup isn't stomped by the generic Lyra-mannequin pass.
#
# B_DC_LyraEnemyBase is parented off ALyraCharacter and gets its mesh + anim
# class wired at BP-creation time — re-applying mesh per-spawn would just
# write the same values, but it would also reset CharacterMesh0's relative
# location using the DCCharacterBase capsule offset, which is wrong for the
# LyraCharacter capsule.
_PRE_EQUIPPED_CLASS_PATHS = ("B_DC_LyraEnemyBase",)

# Dedicated non-World-Partition level that Danger Zone operates on. WP levels
# build actor descriptors at save time, which asserts in OnActorDescInstanceAdded
# for actors spawned via UEditorActorSubsystem (Python's only spawn path). Epic
# ships no runtime WP→standard converter, so we side-step by owning our own
# standard level. First run creates it; subsequent runs just open it.
_DC_DANGER_ZONE_LEVEL = "/Game/DeltaCode/Maps/L_DC_DangerZone"

# ─── SUBSYSTEM HELPERS ───────────────────────────────────────────────────────

def _actor_subsystem():
    """Resolve the EditorActorSubsystem — the UE5.5+ replacement for
    EditorLevelLibrary's actor mutation functions."""
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def _editor_subsystem():
    """Resolve the UnrealEditorSubsystem — replaces EditorLevelLibrary.get_editor_world."""
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

def _spawn_registered(cls, loc, rot):
    """Spawn via the C++ World-Partition-safe path, falling back to the raw
    EditorActorSubsystem call if the native library is unavailable.

    DCWorldPartitionLibrary.spawn_actor_registered calls the same editor path
    but also runs UDataLayerEditorSubsystem::InitializeNewActorDataLayers,
    which the Python spawn bypasses. Without that call the DataLayer manager
    asserts on save (CanResolveDataLayers()) the moment the user tries to
    persist the generated level.
    """
    lib = getattr(unreal, 'DCWorldPartitionLibrary', None)
    if lib is not None:
        actor = lib.spawn_actor_registered(cls, loc, rot)
        if actor is not None:
            return actor
    return _actor_subsystem().spawn_actor_from_class(cls, loc, rot)

def get_editor_world():
    subsys = _editor_subsystem()
    if subsys is None:
        return None
    return subsys.get_editor_world()

# ─── CORE UTILITIES ──────────────────────────────────────────────────────────

def clear_level():
    """Destroy every actor in the active level that isn't on the preserve list.

    Wrapped in a ScopedEditorTransaction so the entire clear is a single
    Ctrl+Z undo step — important because partial rollback of a mission
    teardown would leave the level in an inconsistent state.
    """
    actors = _actor_subsystem().get_all_level_actors()
    to_delete = [a for a in actors if not isinstance(a, _PRESERVE_CLASSES)]

    with unreal.ScopedEditorTransaction("DeltaCode: Clear Level"):
        subsys = _actor_subsystem()
        for actor in to_delete:
            subsys.destroy_actor(actor)

    unreal.log(f"DeltaCode: cleared {len(to_delete)} actors, preserved {len(actors) - len(to_delete)}.")

def spawn_actor(class_path, location, rotation=None, label=None):
    """Spawn a Blueprint class into the current editor world.

    class_path — soft object path, e.g. '/Game/DeltaCode/Core/B_DC_PickupBase.B_DC_PickupBase_C'
    location   — (x, y, z) tuple in Unreal units.
    rotation   — (pitch, yaw, roll) tuple in degrees.
    label      — optional actor label shown in the World Outliner.
    """
    asset = unreal.load_object(None, class_path)
    loc = unreal.Vector(location[0], location[1], location[2])
    rot = unreal.Rotator(rotation[0], rotation[1], rotation[2]) if rotation else unreal.Rotator(0, 0, 0)

    if asset is None:
        unreal.log_warning(f"DeltaCode: could not load class at {class_path} — spawning placeholder cube.")
        placeholder_label = label or class_path.rsplit("/", 1)[-1]
        return _spawn_placeholder(loc, rot, f"PLACEHOLDER_{placeholder_label}")

    # spawn_actor_from_class already places the actor at (loc, rot). A follow-up
    # set_actor_location() with teleport=False gets treated as a swept move from
    # the BP's SCS root (mannequin pivot ≈ -710, 710, -110) and silently fails
    # against the new floor, pinning every actor to that pivot.
    actor = _spawn_registered(asset, loc, rot)
    if actor is not None:
        if label:
            actor.set_actor_label(label)
        # Skeletal mesh cannot be added to the BP asset in UE5.7 (no
        # simple_construction_script property), so we apply it to each spawned
        # instance instead.
        _apply_mesh_to_actor(actor, class_path)
    return actor


def _spawn_placeholder(location, rotation, label):
    """Spawn a StaticMeshActor with a cube mesh as a visual placeholder."""
    cube_path = "/Engine/BasicShapes/Cube.Cube"
    actor = _spawn_registered(unreal.StaticMeshActor, location, rotation)
    if actor is None:
        return None
    mesh = unreal.load_asset(cube_path)
    if mesh is not None:
        actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(0.5, 0.5, 0.5))
    return actor

def create_floor(template_slug):
    """Spawn a scaled Plane mesh as the arena floor for the given template."""
    width, depth, cx, cy = _FLOOR_CONFIGS.get(template_slug, (8000, 8000, 0, 0))
    scale_x = width / 100.0
    scale_y = depth / 100.0

    actor = _spawn_registered(
        unreal.StaticMeshActor, unreal.Vector(cx, cy, 0), unreal.Rotator(0, 0, 0))
    if actor is None:
        unreal.log_error("DeltaCode: failed to spawn arena floor actor.")
        return None

    mesh = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    if mesh is not None:
        actor.static_mesh_component.set_static_mesh(mesh)

    actor.set_actor_scale3d(unreal.Vector(scale_x, scale_y, 1.0))
    actor.set_actor_label("DC_ArenaFloor")
    unreal.log(f"DeltaCode: spawned DC_ArenaFloor ({width}x{depth} cm at ({cx},{cy})) for '{template_slug}'.")
    return actor


# ─── ARENA GAUNTLET GRAYBOX HELPERS ──────────────────────────────────────────
# Used by build_arena() to stamp cover blocks, boss pillars, reward platform,
# and entry triggers. Stick to engine basic shapes (Cube, Plane) and TriggerBox
# so a blank project compiles this with zero extra assets.

def _spawn_cube(location, scale, label):
    """StaticMeshActor with the engine Cube mesh at a given scale and label."""
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    actor = _spawn_registered(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, 0))
    if actor is None:
        return None
    if cube is not None:
        actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(scale)
    actor.set_actor_label(label)
    return actor


def _spawn_cover_cluster(label_prefix, center):
    """Four 300×150×150 cm cover blocks at (±400, ±400) from the arena center."""
    cube_scale = unreal.Vector(3.0, 1.5, 1.5)
    offsets = ((-400, -400), (400, -400), (-400, 400), (400, 400))
    for i, (dx, dy) in enumerate(offsets):
        loc = unreal.Vector(center[0] + dx, center[1] + dy, 75)
        _spawn_cube(loc, cube_scale, f"{label_prefix}_{i}")


def _spawn_boss_cover(label_prefix, center):
    """Four 200×200×400 cm corner pillars around the boss arena center."""
    pillar_scale = unreal.Vector(2.0, 2.0, 4.0)
    offsets = ((-1000, -1000), (1000, -1000), (-1000, 1000), (1000, 1000))
    for i, (dx, dy) in enumerate(offsets):
        loc = unreal.Vector(center[0] + dx, center[1] + dy, 200)
        _spawn_cube(loc, pillar_scale, f"{label_prefix}_{i}")


def _spawn_reward_platform(label, center):
    """Raised 1000×1000 cm plane at z=50 — the post-boss reward area."""
    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    loc = unreal.Vector(center[0], center[1], 50)
    actor = _spawn_registered(unreal.StaticMeshActor, loc, unreal.Rotator(0, 0, 0))
    if actor is None:
        return
    if plane is not None:
        actor.static_mesh_component.set_static_mesh(plane)
    actor.set_actor_scale3d(unreal.Vector(10.0, 10.0, 1.0))
    actor.set_actor_label(label)


def _spawn_arena_entry_trigger(label, position):
    """TriggerBox marker at an arena entrance. No gameplay wiring yet — a
    future slice will hook arena gating off these actors via their labels.
    Default TriggerBox brush is 200×200×200 cm; scale forms a 100×800×400
    gate across the entry axis."""
    loc = unreal.Vector(position[0], position[1], position[2] + 200)
    actor = _spawn_registered(unreal.TriggerBox, loc, unreal.Rotator(0, 0, 0))
    if actor is None:
        return
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(0.5, 4.0, 2.0))


def rebuild_navmesh():
    """Synchronously rebuild navigation for the current editor world so AI
    can path the moment PIE starts.

    Routed through unreal.DCAIEditorBridge.rebuild_navigation_mesh, which
    calls FEditorBuildUtils::EditorBuild(BuildAIPaths) — same path the
    editor's Build menu uses, blocking with progress UI. The previous
    `RebuildNavigation` console command was async, so PIE could start
    before tiles existed and enemies would stand still even though
    BB/BT/possession were all wired correctly.

    Falls back to the async console command on older builds where the
    bridge hasn't been compiled in yet, with a warning so the user knows
    the rebuild may not complete before PIE.
    """
    try:
        result = unreal.DCAIEditorBridge.rebuild_navigation_mesh()
        if isinstance(result, tuple):
            ok = bool(result[0])
            msg = str(result[-1]) if len(result) > 1 else ""
        else:
            ok = bool(result)
            msg = ""
        if ok:
            unreal.log(f"DeltaCode: NavMesh built — {msg}")
        else:
            unreal.log_warning(f"DeltaCode: NavMesh build failed — {msg}")
    except AttributeError:
        unreal.log_warning(
            "DeltaCode: DCAIEditorBridge.rebuild_navigation_mesh not "
            "registered — rebuild the plugin and restart the editor. "
            "Falling back to async console command for now.")
        try:
            world = get_editor_world()
            if world is not None:
                unreal.SystemLibrary.execute_console_command(
                    world, "RebuildNavigation")
        except Exception as e:
            unreal.log_warning(f"DeltaCode: NavMesh fallback failed — {e}")
    except Exception as e:
        unreal.log_warning(f"DeltaCode: NavMesh rebuild skipped — {e}")

# ─── MISSION TEMPLATES ───────────────────────────────────────────────────────

def build_extraction(_world):
    """Extraction Zone — 3 loot clusters, 8 enemy patrols, extraction boss."""
    with unreal.ScopedEditorTransaction("DeltaCode: Build Extraction Mission"):
        loot_positions = [(-2000, 0, 0), (0, 2000, 0), (2000, -1000, 0)]
        for i, base in enumerate(loot_positions):
            for j in range(3):
                loc = (base[0] + j * 100, base[1] + j * 100, base[2])
                spawn_actor(DC_CLASSES["pickup_base"], loc, label=f"DC_LootPickup_{i}_{j}")

        z = _CHARACTER_Z_OFFSET
        enemy_positions = [
            (-1500,  500, z), ( -500, 1500, z), (  500, 2500, z),
            ( 1500, 1500, z), ( 2500,  500, z), ( 2500, -500, z),
            ( 1500,-1500, z), (  500,-2000, z),
        ]
        for i, pos in enumerate(enemy_positions):
            spawn_actor(DC_CLASSES["enemy_base"], pos, label=f"DC_Enemy_{i:02d}")

        spawn_actor(DC_CLASSES["extraction_zone"], (4000, 0, 0), label="DC_ExtractionZone")
        spawn_actor(DC_CLASSES["boss_base"],       (3500, 0, z),
                    rotation=(0, 180, 0), label="DC_ExtractionBoss")
        spawn_actor(DC_CLASSES["objective"],       (   0, 0, 0),
                    label="DC_Objective_ReachExtraction")

    rebuild_navmesh()
    unreal.log("DeltaCode: extraction mission built.")

def build_arena(_world):
    """Arena Gauntlet — Slice A: level skeleton only.

    Linear graybox along +X: three combat arenas (centers 1000/4000/7000),
    a boss arena (center 10500), and a reward platform (center 13000).
    Each arena gets cover geometry; entry positions are marked with
    TriggerBox actors for the future gating slice to hook into.

    Gameplay systems left for later slices:
      • Slice B — AI perception/state colors
      • Slice C — arena gate mechanics (the entry triggers exist but are
        not yet wired; a future slice looks them up by label)
      • Slice D — boss phases / shield
      • Slice E — hub level + transition
      • Slice F — vendor / storage actors
    """
    with unreal.ScopedEditorTransaction("DeltaCode: Build Arena Mission"):
        z = _CHARACTER_Z_OFFSET

        # (label_prefix, entry_x, center_x) for the three combat arenas.
        arenas = (
            ("DC_Arena1", 200,  1000),
            ("DC_Arena2", 3000, 4000),
            ("DC_Arena3", 6000, 7000),
        )
        for prefix, entry_x, center_x in arenas:
            _spawn_arena_entry_trigger(f"{prefix}_EntryTrigger", (entry_x, 0, 0))
            _spawn_cover_cluster(f"{prefix}_Cover", center=(center_x, 0))
            spawn_actor(DC_CLASSES["spawn_zone"], (center_x, 0,   0),
                        label=f"{prefix}_SpawnZone")
            spawn_actor(DC_CLASSES["objective"],  (center_x, 0, 200),
                        label=f"{prefix}_Objective")
            for j in range(4):
                off_x = (j % 2) * 400 - 200
                off_y = (j // 2) * 400 - 200
                spawn_actor(DC_CLASSES["enemy_base"],
                            (center_x + off_x, off_y, z),
                            label=f"{prefix}_Enemy_{j}")

        # Boss arena — larger footprint, corner pillars instead of a cluster.
        _spawn_arena_entry_trigger("DC_BossArena_EntryTrigger", (9000, 0, 0))
        _spawn_boss_cover("DC_Boss_Cover", center=(10500, 0))
        spawn_actor(DC_CLASSES["boss_base"],  (10500, 0,   z),
                    label="DC_FinalBoss")
        spawn_actor(DC_CLASSES["objective"],  (10500, 0, 200),
                    label="DC_Objective_DefeatBoss")

        # Reward area — raised plane with the chest pickup on top.
        _spawn_reward_platform("DC_RewardPlatform", center=(13000, 0))
        spawn_actor(DC_CLASSES["pickup_base"], (13000, 0, 100),
                    label="DC_BossChestReward")

    rebuild_navmesh()
    unreal.log("DeltaCode: arena mission built.")

def build_questhub(_world):
    """Quest Hub World — quest hub, 3 objectives with guards, ambient roamers, final boss."""
    with unreal.ScopedEditorTransaction("DeltaCode: Build Quest Hub Mission"):
        z = _CHARACTER_Z_OFFSET
        spawn_actor(DC_CLASSES["objective"], (0, 0, 0), label="DC_QuestGiver_NPC")

        objectives = [(3000, 2000, 0), (-2000, 3000, 0), (4000, -2000, 0)]
        for i, pos in enumerate(objectives):
            spawn_actor(DC_CLASSES["objective"],   pos,                         label=f"DC_QuestObjective_{i+1}")
            spawn_actor(DC_CLASSES["enemy_base"],  (pos[0] - 300, pos[1], z),   label=f"DC_Guard_{i}_A")
            spawn_actor(DC_CLASSES["enemy_base"],  (pos[0] + 300, pos[1], z),   label=f"DC_Guard_{i}_B")
            spawn_actor(DC_CLASSES["pickup_base"], (pos[0], pos[1] + 200, 0),   label=f"DC_Loot_{i}")

        for i, pos in enumerate([(1500, 0, z), (-1000, 1500, z), (2500, -500, z), (0, -2000, z)]):
            spawn_actor(DC_CLASSES["enemy_base"], pos, label=f"DC_AmbientEnemy_{i}")

        spawn_actor(DC_CLASSES["boss_base"], (4000, -2000, 200 + z), label="DC_FinalObjectiveBoss")

    rebuild_navmesh()
    unreal.log("DeltaCode: questhub mission built.")

def build_reactivestory(_world):
    """Reactive Story World — town hub, 4 POIs with camps + loot, mid-map camps, dungeon boss."""
    with unreal.ScopedEditorTransaction("DeltaCode: Build Reactive Story Mission"):
        z = _CHARACTER_Z_OFFSET
        spawn_actor(DC_CLASSES["objective"], (0, 0, 0), label="DC_TownHub_QuestGiver")
        for i in range(3):
            spawn_actor(DC_CLASSES["pickup_base"], (i * 200 - 200, 300, 0), label=f"DC_Town_Merchant_{i}")

        poi_positions = [(6000, 0, 0), (-6000, 0, 0), (0, 6000, 0), (0, -6000, 0)]
        poi_labels    = ["RuinedFort", "AbandonedMine", "EnemyStronghold", "AncientShrine"]
        for pos, label in zip(poi_positions, poi_labels):
            spawn_actor(DC_CLASSES["objective"], pos, label=f"DC_POI_{label}_Objective")
            for j in range(3):
                spawn_actor(DC_CLASSES["enemy_base"],
                            (pos[0] + j * 300 - 300, pos[1] + j * 200, z),
                            label=f"DC_{label}_Enemy_{j}")
            spawn_actor(DC_CLASSES["pickup_base"], (pos[0] + 100, pos[1] + 100, 0),
                        label=f"DC_{label}_Loot")

        for i, pos in enumerate([(3000, 3000, 0), (-3000, 3000, 0), (-3000, -3000, 0), (3000, -3000, 0)]):
            for j in range(4):
                spawn_actor(DC_CLASSES["enemy_base"], (pos[0] + j * 250 - 375, pos[1], z),
                            label=f"DC_Camp{i}_Enemy_{j}")

        spawn_actor(DC_CLASSES["objective"], (8000, 8000, 0),   label="DC_DungeonEntrance_Trigger")
        spawn_actor(DC_CLASSES["boss_base"], (0, 6000, z),       label="DC_Stronghold_MiniBoss")
        spawn_actor(DC_CLASSES["boss_base"], (9000, 9000, z),    label="DC_FinalDungeon_Boss")

        for i, pos in enumerate([(1500, 1500, 0), (-1500, 1500, 0), (2000, -1000, 0),
                                 (-2000, -2000, 0), (4000, 2000, 0), (-4000, -1000, 0)]):
            spawn_actor(DC_CLASSES["pickup_base"], pos, label=f"DC_AmbientLoot_{i}")

    rebuild_navmesh()
    unreal.log("DeltaCode: reactivestory mission built.")

# ─── CORE BLUEPRINT CREATION ─────────────────────────────────────────────────

# Role-based color conventions for DC actor Blueprints.
# Each core Blueprint gets the Third Person mannequin with a tinted material
# so roles are visually distinct in Play mode.
#
# Combat roles:
#   Enemy (base):       Red          (1.0, 0.0, 0.0)
#   Boss:               Dark Red     (0.6, 0.0, 0.0)  + 1.5× scale
#
# Gameplay objects:
#   Pickup:             Cyan         (0.0, 0.8, 1.0)
#   Objective:          Yellow       (1.0, 0.85, 0.0)
#   Spawn Zone:         Green        (0.0, 1.0, 0.0)
#   Extraction Zone:    Purple       (0.8, 0.0, 1.0)
#
# Future NPC roles (apply when Blueprints are created):
#   Quest Giver / NPC:  Green        (0.0, 0.8, 0.2)
#   Vendor / Shopkeeper: Yellow      (1.0, 0.85, 0.0)
#   Armourer:           Yellow-Orange (1.0, 0.6, 0.0)

_CORE_BLUEPRINTS = [
    ("B_DC_EnemyBase",       "/Script/DeltaCode.DCEnemyBase"),
    ("B_DC_PickupBase",      "/Script/DeltaCode.DCPickupBase"),
    ("B_DC_ObjectiveBase",   "/Script/Engine.Actor"),
    ("B_DC_BossBase",        "/Script/DeltaCode.DCEnemyBase"),
    ("B_DC_SpawnZone",       "/Script/Engine.Actor"),
    ("B_DC_ExtractionZone",  "/Script/Engine.Actor"),
]

_CORE_COLORS = {
    "B_DC_EnemyBase":      (1.0, 0.0, 0.0),
    "B_DC_BossBase":       (0.6, 0.0, 0.0),
    "B_DC_PickupBase":     (0.0, 0.8, 1.0),
    "B_DC_ObjectiveBase":  (1.0, 0.85, 0.0),
    "B_DC_SpawnZone":      (0.0, 1.0, 0.0),
    "B_DC_ExtractionZone": (0.8, 0.0, 1.0),
}

_CORE_SCALES = {
    "B_DC_BossBase": 1.5,
}

# Lyra 5.7 mannequin paths. SKM_Manny (not _Invis) so enemies are visible;
# ABP_Mannequin_Base is the full locomotion AnimBP.
_MANNEQUIN_MESH = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"
_MANNEQUIN_ANIM_CLASS = "/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C"

# Legacy fallbacks for non-Lyra UE template projects and pre-5.4 Lyra layouts.
# _resolve_mannequin_mesh()/_resolve_mannequin_anim_class() try the primary
# path first then walk these in order; if none load, warns once and returns
# None so callers can skip mesh assignment instead of crashing.
_MANNEQUIN_MESH_FALLBACKS = [
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple",
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
]
_MANNEQUIN_ANIM_CLASS_FALLBACKS = [
    "/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C",
    "/Game/Characters/Mannequins/Animations/ABP_Quinn.ABP_Quinn_C",
]

# Resolution caches. None = not yet tried. False = tried and failed (warned).
_RESOLVED_MANNEQUIN_MESH = None
_RESOLVED_MANNEQUIN_ANIM_CLASS = None

# Lyra detection. None = not yet tried. False = absent. True = present.
# Cached because every Build Mission asks the same question.
_RESOLVED_LYRA_AVAILABLE = None
_LYRA_CHARACTER_CLASS_PATH = "/Script/LyraGame.LyraCharacter"

# Lyra-parented enemy BP we generate when Lyra is detected. Lives alongside
# B_DC_EnemyBase so the same Build Mission flow can pick whichever the project
# supports — see _apply_scan_to_dc_classes for the routing.
_LYRA_ENEMY_BP_NAME = "B_DC_LyraEnemyBase"
_LYRA_ENEMY_BP_PATH = f"/Game/DeltaCode/Core/{_LYRA_ENEMY_BP_NAME}"
_LYRA_ENEMY_BP_CLASS_PATH = f"{_LYRA_ENEMY_BP_PATH}.{_LYRA_ENEMY_BP_NAME}_C"

def _lyra_available():
    """Return True iff ALyraCharacter is loadable in this project. Cached.

    Used to gate B_DC_LyraEnemyBase creation (we don't author a Lyra-parented
    BP in non-Lyra projects) and to route DC_CLASSES['enemy_base'] when the
    BP is present.
    """
    global _RESOLVED_LYRA_AVAILABLE
    if _RESOLVED_LYRA_AVAILABLE is not None:
        return _RESOLVED_LYRA_AVAILABLE
    cls = unreal.load_class(None, _LYRA_CHARACTER_CLASS_PATH)
    _RESOLVED_LYRA_AVAILABLE = cls is not None
    if _RESOLVED_LYRA_AVAILABLE:
        unreal.log(
            f"DeltaCode: Lyra detected — {_LYRA_CHARACTER_CLASS_PATH} loadable.")
    else:
        unreal.log(
            f"DeltaCode: Lyra not detected — {_LYRA_CHARACTER_CLASS_PATH} "
            f"unavailable. B_DC_LyraEnemyBase creation will be skipped.")
    return _RESOLVED_LYRA_AVAILABLE


def _resolve_mannequin_mesh():
    """Return the mannequin SkeletalMesh asset, trying the Lyra 5.7 path first
    then falling back to legacy template paths. Caches the result so repeated
    calls don't re-load. Logs a single warning the first time nothing resolves
    and returns None thereafter — callers should skip mesh assignment in that
    case rather than crashing."""
    global _RESOLVED_MANNEQUIN_MESH
    if _RESOLVED_MANNEQUIN_MESH is False:
        return None
    if _RESOLVED_MANNEQUIN_MESH is not None:
        return _RESOLVED_MANNEQUIN_MESH
    candidates = (_MANNEQUIN_MESH, *_MANNEQUIN_MESH_FALLBACKS)
    for path in candidates:
        asset = unreal.load_asset(path)
        if asset is not None:
            _RESOLVED_MANNEQUIN_MESH = asset
            return asset
    unreal.log_warning(
        f"DeltaCode: No mannequin SkeletalMesh found. Tried: "
        f"{', '.join(candidates)}. Spawned actors will be invisible — "
        f"update _MANNEQUIN_MESH in dc_danger_zone.py to match this "
        f"project's mannequin location.")
    _RESOLVED_MANNEQUIN_MESH = False
    return None


def _resolve_mannequin_anim_class():
    """Return the mannequin AnimBlueprint generated UClass, with same primary
    + fallback + warn-once semantics as _resolve_mannequin_mesh()."""
    global _RESOLVED_MANNEQUIN_ANIM_CLASS
    if _RESOLVED_MANNEQUIN_ANIM_CLASS is False:
        return None
    if _RESOLVED_MANNEQUIN_ANIM_CLASS is not None:
        return _RESOLVED_MANNEQUIN_ANIM_CLASS
    candidates = (_MANNEQUIN_ANIM_CLASS, *_MANNEQUIN_ANIM_CLASS_FALLBACKS)
    for path in candidates:
        cls = unreal.load_class(None, path)
        if cls is not None:
            _RESOLVED_MANNEQUIN_ANIM_CLASS = cls
            return cls
    unreal.log_warning(
        f"DeltaCode: No mannequin AnimBlueprint found. Tried: "
        f"{', '.join(candidates)}. Spawned actors will not animate.")
    _RESOLVED_MANNEQUIN_ANIM_CLASS = False
    return None


_CORE_PACKAGE_PATH = "/Game/DeltaCode/Core"

# Vertical offset applied to every mannequin-driven actor (enemies, bosses).
# Matches the UE5 Character capsule half-height — without it the capsule is
# centred on the actor pivot and the mesh sinks halfway into the arena floor.
# _resolve_character_z_offset() rewrites this at run_danger_zone() start to
# match whatever class actually ends up in DC_CLASSES["enemy_base"].
_CHARACTER_Z_OFFSET = 88


def _inspect_preequipped_once(actor, class_path):
    """Log AI controller / BT / capsule half-height for the first instance of a
    pre-equipped BP class seen during a single build. Prevents dozens of
    identical log lines when 30+ enemies spawn. State lives on the function
    itself so it resets naturally between importlib.reload cycles."""
    seen = getattr(_inspect_preequipped_once, "_seen", None)
    if seen is None:
        seen = set()
        _inspect_preequipped_once._seen = seen
    if class_path in seen:
        return
    seen.add(class_path)
    try:
        ai_cls  = actor.get_editor_property('ai_controller_class')
        bt      = None
        try:
            bt = actor.get_editor_property('behavior_tree')
        except Exception:
            pass
        cap     = actor.get_editor_property('capsule_component')
        half    = cap.get_editor_property('capsule_half_height') if cap else None
        unreal.log(f"DeltaCode[pre-equipped] {class_path}")
        unreal.log(f"  ai_controller : {ai_cls.get_name() if ai_cls else 'None'}")
        unreal.log(f"  behavior_tree : {bt.get_name() if bt else 'None'}")
        unreal.log(f"  capsule_half  : {half}")

        # Component dump — used to diagnose why pre-equipped AI idles in PIE.
        # Some combat AIs gate locomotion behind an equipped-weapon check on
        # BeginPlay, so if no Weapon / ChildActor / Inventory component shows
        # up here that's the likely cause.
        try:
            comps = actor.get_components_by_class(unreal.ActorComponent)
            unreal.log(f"  components    : {len(comps)}")
            for c in comps:
                try:
                    unreal.log(
                        f"    - {c.get_name()} "
                        f"[{c.get_class().get_name()}]")
                except Exception:
                    pass
        except Exception as e:
            unreal.log_warning(
                f"DeltaCode[pre-equipped] component dump failed — {e}")
    except Exception as e:
        unreal.log_warning(f"DeltaCode[pre-equipped] introspect failed — {e}")


def _apply_scan_to_dc_classes(scan):
    """Use dc_inspect_project results to route DC_CLASSES['enemy_base'] and
    ['boss_base']. Replaces the older path-specific Variant_Combat probe —
    the inspector returns all Character subclasses across /Game/, so we
    can find BP_CombatEnemy and any Boss variant by name regardless of
    where they live in the project.

    Routing priority (highest to lowest):
      1. B_DC_LyraEnemyBase — present only in Lyra projects, gives us a
         LyraCharacter-parented pawn that ABP_Mannequin_Base initialises
         correctly (no sliding mannequin).
      2. B_DC_EnemyBase — DeltaCode's portable fallback. Used in non-Lyra
         projects and when Lyra is present but the Lyra BP failed to create.

    scan — dict from dc_inspect_project(silent=True). Empty dict on failure
    triggers the fallback path.
    """
    fallback_path = "/Game/DeltaCode/Core/B_DC_EnemyBase.B_DC_EnemyBase_C"
    enemy_rows = scan.get("enemy", [])

    # Lyra-native enemy first. Both checks must pass: Lyra detected AND the
    # generated BP actually exists on disk. If we lost the BP between runs
    # (manual delete, branch switch), fall through to the legacy lookup.
    routed = False
    if (_lyra_available()
            and unreal.EditorAssetLibrary.does_asset_exist(_LYRA_ENEMY_BP_PATH)):
        DC_CLASSES["enemy_base"] = _LYRA_ENEMY_BP_CLASS_PATH
        DC_CLASSES["boss_base"]  = _LYRA_ENEMY_BP_CLASS_PATH
        unreal.log(
            f"DeltaCode: enemy_base/boss_base → {_LYRA_ENEMY_BP_CLASS_PATH} "
            f"(LyraCharacter-parented).")
        routed = True

    if not routed:
        # Find BP_CombatEnemy by exact name match in the enemy/character rows.
        combat = next(
            (r for r in enemy_rows
             if r.get("kind") in ("BP", "BP-AI") and r.get("name") == "BP_CombatEnemy"),
            None)
        if combat:
            cls_path = f"{combat['path']}.{combat['name']}_C"
            DC_CLASSES["enemy_base"] = cls_path
            DC_CLASSES["boss_base"]  = cls_path
            unreal.log(f"DeltaCode: enemy_base/boss_base → {cls_path}")
        else:
            DC_CLASSES["enemy_base"] = fallback_path
            DC_CLASSES["boss_base"]  = fallback_path
            unreal.log("DeltaCode: BP_CombatEnemy not found — falling back to "
                       "B_DC_EnemyBase for enemy/boss.")

    # Override boss with a dedicated Boss variant if one exists. Spawner BPs
    # are skipped (they'd place a spawner actor instead of a pawn). Skipped
    # entirely when we routed to B_DC_LyraEnemyBase — a project-shipped Boss
    # BP would lack IDCEnemyAIPawn / DC components and silently fail to run
    # the BT, which is a regression from the Lyra-parented routing.
    if not routed:
        for r in enemy_rows:
            if r.get("kind") not in ("BP", "BP-AI"):
                continue
            name = r.get("name", "")
            if "Boss" in name and "Spawner" not in name:
                cls_path = f"{r['path']}.{name}_C"
                DC_CLASSES["boss_base"] = cls_path
                unreal.log(f"DeltaCode: boss_base overridden → {cls_path}")
                break

    # Combat montage detection — log only for v1; no DC_CLASSES update yet.
    montages = [r for r in scan.get("animation", []) if r.get("kind") == "Montage"]
    combat_montages = [m for m in montages
                       if "Combat" in m.get("name", "") or "Attack" in m.get("name", "")]
    if combat_montages:
        names = ", ".join(m["name"] for m in combat_montages[:5])
        more = f" (+{len(combat_montages) - 5} more)" if len(combat_montages) > 5 else ""
        unreal.log(f"DeltaCode: combat montages found ({len(combat_montages)}): "
                   f"{names}{more}")


def _resolve_character_z_offset():
    """Read the enemy BP's capsule half-height from its CDO and rewrite the
    module-level _CHARACTER_Z_OFFSET. Template BPs commonly use 96 (stock
    Character default) while DCCharacterBase uses 88 — spawning at the wrong
    Z drops pawns through the floor or floats them above it."""
    global _CHARACTER_Z_OFFSET
    class_path = DC_CLASSES.get("enemy_base", "")
    try:
        enemy_cls = unreal.load_object(None, class_path)
        if enemy_cls is None:
            unreal.log_warning(
                f"DeltaCode: could not load {class_path} to read capsule half.")
            return
        cdo = unreal.get_default_object(enemy_cls)
        cap = cdo.get_editor_property('capsule_component')
        if cap is None:
            unreal.log_warning(
                f"DeltaCode: {class_path} CDO has no capsule_component.")
            return
        half = cap.get_editor_property('capsule_half_height')
        if half and half > 0:
            _CHARACTER_Z_OFFSET = float(half)
            unreal.log(
                f"DeltaCode: _CHARACTER_Z_OFFSET resolved to {_CHARACTER_Z_OFFSET} "
                f"from {class_path}")
    except Exception as e:
        unreal.log_warning(f"DeltaCode: Z-offset resolve failed — {e}")


def _apply_mesh_to_actor(actor, class_path):
    """Attach the mannequin skeletal mesh to a live spawned actor instance.

    UE5.7 removed the Blueprint.simple_construction_script property, so we
    cannot add a SkeletalMeshComponent to the BP asset from Python. Instead we
    set the mesh on the already-constructed actor's existing component.
    Actors without a SkeletalMeshComponent (pickups, objectives, zones) are
    silently tolerated — the warning makes the skip visible in the Output Log.

    Pre-equipped Blueprints (e.g. /Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy)
    already ship with mesh + anim + AIController + BT wired. Skip them entirely
    so we don't stomp the template setup with the mannequin.
    """
    if any(p in class_path for p in _PRE_EQUIPPED_CLASS_PATHS):
        _inspect_preequipped_once(actor, class_path)
        return

    skel_mesh = _resolve_mannequin_mesh()
    if skel_mesh is None:
        return
    # Prefer the inherited ACharacter mesh (CharacterMesh0). On a vanilla
    # ACharacter subclass that's the only SkeletalMeshComponent present —
    # a bare class-based lookup without the name match would still hit it,
    # but being explicit guards against future BPs that add additional
    # skeletal meshes (weapons etc.) and we want CharacterMesh0 specifically.
    all_mesh_comps = actor.get_components_by_class(unreal.SkeletalMeshComponent)
    mesh_comp = next(
        (c for c in all_mesh_comps if c.get_name() == "CharacterMesh0"),
        all_mesh_comps[0] if all_mesh_comps else None)
    if mesh_comp is not None:
        mesh_comp.set_editor_property("skeletal_mesh_asset", skel_mesh)
        # Drop the mesh by the capsule half-height so the mannequin's feet sit
        # on the capsule bottom rather than floating at its centre.
        mesh_comp.set_editor_property(
            "relative_location", unreal.Vector(0, 0, -_CHARACTER_Z_OFFSET))
        # Mannequin locomotion AnimBP. Route through the resolver which
        # tries the Lyra 5.7 ABP_Mannequin_Base first and falls back to
        # legacy paths for non-Lyra projects.
        anim_class = _resolve_mannequin_anim_class()
        if anim_class is not None:
            # Set the two UPROPERTYs that drive runtime anim instance
            # creation. animation_mode must be AnimationBlueprint (default
            # is Custom, which yields a null AnimInstance). anim_class
            # names the AnimBP generated UClass. Both persist into the
            # saved level as instance overrides on CharacterMesh0.
            #
            # We deliberately do NOT call set_anim_instance_class here.
            # That method invokes InitAnim against the editor-world pawn
            # and creates a transient UAnimInstance subobject that gets
            # serialised into the saved level. At PIE start the duplicated
            # subobject shadows PIE's own InitAnim pass — PIE sees
            # AnimInstance != nullptr and skips construction — and the
            # shadowed instance's references still point at editor-world
            # components that never get remapped to PIE. Result: the
            # AnimGraph never ticks against the live CharacterMovement,
            # the character slides without locomotion. Letting PIE create
            # a fresh anim instance from anim_class at BeginPlay is the
            # correct path.
            mesh_comp.set_editor_property(
                "animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
            mesh_comp.set_editor_property("anim_class", anim_class)
        else:
            unreal.log_warning(
                f"DeltaCode: failed to resolve mannequin AnimBP for "
                f"{actor.get_name()}")

        # AI wiring — ensure the pawn has an AIController and that the mesh
        # rotates toward its velocity rather than its controller yaw. Keeps
        # a custom DCEnemyAIController if one is already assigned on the class.
        current_ai = actor.get_editor_property('ai_controller_class')
        current_ai_name = current_ai.get_name() if current_ai is not None else ""
        if current_ai_name != "DCEnemyAIController":
            actor.set_editor_property('ai_controller_class', unreal.AIController)
        actor.set_editor_property('use_controller_rotation_yaw', False)

        char_move = actor.get_editor_property('character_movement')
        if char_move is not None:
            char_move.set_editor_property('orient_rotation_to_movement', True)

        # Behavior tree — loaded and best-effort attached if present. Vanilla
        # AIController has no BehaviorTree UPROPERTY, so the set_editor_property
        # below is a no-op unless a custom AIController exposes one. Actually
        # driving wander in PIE requires that custom class to call
        # RunBehaviorTree(bt) in OnPossess.
        bt_path = "/Game/DeltaCode/AI/BT_DC_Enemy"
        if unreal.EditorAssetLibrary.does_asset_exist(bt_path):
            bt_asset = unreal.load_asset(bt_path)
            try:
                actor.set_editor_property('behavior_tree', bt_asset)
            except Exception:
                unreal.log_warning(
                    f"DeltaCode: {actor.get_name()} has no 'behavior_tree' "
                    f"property — BT {bt_path} loaded but not wired.")
    else:
        unreal.log_warning(
            f"DeltaCode: no SkeletalMeshComponent on {actor.get_name()}")


def _create_blueprint_if_missing(package_path, bp_name, parent_class_path,
                                 force_recreate=False):
    """Create a Blueprint asset if it doesn't already exist.

    When force_recreate is True, deletes the existing asset first so it gets
    rebuilt with the latest configuration.

    Mesh / anim wiring is intentionally NOT done here — _apply_mesh_to_actor
    runs on every spawned instance and writes mesh + anim_class on the live
    actor's CharacterMesh0 component. The previous SCS-based path attempted
    to add a SkeletalMeshComponent at BP-creation time but silently failed
    (UBlueprint::SimpleConstructionScript has no BlueprintReadWrite specifier
    so get_editor_property can't reach it from Python). The per-spawn path
    has been doing the real work all along; the CDO-time wiring would have
    been redundant even if it worked.
    """
    full_path = f"{package_path}/{bp_name}"

    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        if not force_recreate:
            unreal.log(f"DeltaCode: {bp_name} already exists, skipping.")
            return unreal.load_asset(full_path)
        unreal.EditorAssetLibrary.delete_asset(full_path)
        unreal.log(f"DeltaCode: {bp_name} deleted for recreate.")

    parent_class = unreal.load_class(None, parent_class_path)
    if parent_class is None:
        unreal.log_warning(
            f"DeltaCode: parent class '{parent_class_path}' not found for {bp_name}, "
            f"falling back to Actor.")
        parent_class = unreal.Actor

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    blueprint = asset_tools.create_asset(
        bp_name,
        package_path,
        unreal.Blueprint,
        factory)

    if blueprint:
        unreal.EditorAssetLibrary.save_asset(full_path)
        unreal.log(f"DeltaCode: Created {bp_name}")
    else:
        unreal.log_error(f"DeltaCode: Failed to create {bp_name}")
    return blueprint


def _gather_subobject_handles(blueprint):
    """Return all subobject handles for a Blueprint via SubobjectDataSubsystem.

    UE5+ canonical path for script-based BP component inspection / mutation.
    The legacy SimpleConstructionScript UPROPERTY isn't script-reflected
    (no BlueprintReadWrite specifier on UBlueprint::SimpleConstructionScript)
    so get_editor_property('simple_construction_script') always fails — we
    have to go through the subsystem.
    """
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if sub is None:
        unreal.log_warning(
            "DeltaCode: SubobjectDataSubsystem unavailable — cannot edit "
            "BP components.")
        return None, []
    handles = sub.k2_gather_subobject_data_for_blueprint(blueprint)
    return sub, handles


def _subobject_object(data):
    """Extract the underlying UObject (typically a component template) from a
    FSubobjectData struct in a UE-version-robust way.

    UE5.7 API tour:
    - FSubobjectData is a USTRUCT whose UPROPERTYs are all plain UPROPERTY()
      (no BlueprintReadWrite specifier), so get_editor_property('weak_object_ptr')
      and friends won't work.
    - The canonical accessor is USubobjectDataBlueprintFunctionLibrary's
      GetAssociatedObject (GetObject is deprecated in 5.7). But Python
      reflection of those static UFUNCTIONs isn't guaranteed across every
      build configuration.
    - to_tuple() is a built-in on every UE Python struct wrapper and surfaces
      all UPROPERTYs as a positional tuple regardless of access specifier.
      FSubobjectData's first UPROPERTY is WeakObjectPtr<UObject> (the
      component template), so tuple[0] is the UObject directly when the
      library path is unavailable.

    Try library → deprecated library → to_tuple. Returns None if every path
    fails (which would mean a fundamental API shape change we'd need to
    rediscover for that engine version).
    """
    try:
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if obj is not None:
            return obj
    except Exception:
        pass
    try:
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            return obj
    except Exception:
        pass
    try:
        tup = data.to_tuple()
        if tup and len(tup) > 0:
            cand = tup[0]
            if isinstance(cand, unreal.Object):
                return cand
    except Exception:
        pass
    return None


def _find_subobject_template(handles, sub, component_class):
    """Walk handles, return the first component template that is an instance
    of component_class (or a subclass). Used for idempotency and for the
    wiring step that needs to set a UPROPERTY on the template.
    """
    for h in handles:
        data = sub.k2_find_subobject_data_from_handle(h)
        if data is None:
            continue
        obj = _subobject_object(data)
        if obj is not None and isinstance(obj, component_class):
            return obj
    return None


def _add_dc_component(blueprint, bp_name, component_class, component_name):
    """Attach a DeltaCode runtime component to a Blueprint via the modern
    SubobjectDataSubsystem (UE5+).

    Used to give Lyra-parented enemies the same DCFaction / DCEquipment /
    DCHealth / DCEnemyAIData components that ADCEnemyBase exposes by C++
    inheritance — DCEnemyAIController and DCBTTask_AttackTarget look these up
    via FindComponentByClass, so as long as they're present the AI runs.

    Idempotent: walks existing subobjects first and skips if a template of
    the same class is already attached.
    """
    sub, handles = _gather_subobject_handles(blueprint)
    if sub is None or not handles:
        unreal.log_warning(
            f"DeltaCode: [{bp_name}] no subobject handles — cannot add "
            f"{component_name}.")
        return False

    if _find_subobject_template(handles, sub, component_class) is not None:
        unreal.log(
            f"DeltaCode: [{bp_name}] {component_name} already present "
            f"— skipping.")
        return True

    # handles[0] is conventionally the actor root — the only valid parent for
    # SceneComponent-less ActorComponents like ours.
    params = unreal.AddNewSubobjectParams()
    params.parent_handle = handles[0]
    params.new_class = component_class
    params.blueprint_context = blueprint

    try:
        result = sub.add_new_subobject(params)
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: [{bp_name}] add_new_subobject raised for "
            f"{component_name} — {e}")
        return False

    # add_new_subobject returns (FSubobjectDataHandle, FText fail_reason).
    # Defensive unpacking — Python wrapping of UE tuple returns occasionally
    # surfaces extra in/out items (we hit this with DCAIEditorBridge).
    if isinstance(result, tuple):
        new_handle = result[0] if len(result) > 0 else None
        fail_text = result[-1] if len(result) > 1 else None
    else:
        new_handle = result
        fail_text = None

    fail_str = str(fail_text) if fail_text is not None else ""
    if fail_str and fail_str != "":
        unreal.log_warning(
            f"DeltaCode: [{bp_name}] add_new_subobject reported failure for "
            f"{component_name}: {fail_str}")
        # A non-empty failure text doesn't necessarily mean nothing was
        # added; verify by re-walking handles.

    # Re-walk handles to confirm the component is now present. This is the
    # authoritative success check — handle validity alone isn't, because the
    # subsystem can return a placeholder handle on partial success.
    _, fresh_handles = _gather_subobject_handles(blueprint)
    if _find_subobject_template(fresh_handles, sub, component_class) is not None:
        unreal.log(
            f"DeltaCode: [{bp_name}] added {component_name} via "
            f"SubobjectDataSubsystem.")
        return True

    unreal.log_warning(
        f"DeltaCode: [{bp_name}] add_new_subobject for {component_name} "
        f"completed but template not found on rescan — component may be "
        f"missing at runtime.")
    return False


def _create_lyra_enemy_blueprint(force_recreate=False):
    """Create B_DC_LyraEnemyBase parented off ALyraCharacter.

    Skipped if Lyra isn't installed in this project. Adds the DC component
    set the AI subsystem expects (faction / equipment / health), wires the
    Lyra mannequin mesh + AnimBP, sets the AI controller to
    ADCEnemyAIController, and marks the BP as implementing IDCEnemyAIPawn so
    DCEnemyAIController can read its BehaviorTree at possession time.

    The 'BehaviorTree' BP variable that backs IDCEnemyAIPawn::
    GetEnemyBehaviorTree is added by dc_create_ai_assets._wire_lyra_enemy_blueprint
    on the wiring pass — keeping creation here and wiring there mirrors how
    B_DC_EnemyBase is split across the two scripts.

    Returns the Blueprint asset on success, None otherwise. Failure logs and
    falls back — caller continues with B_DC_EnemyBase routing.
    """
    if not _lyra_available():
        return None

    if unreal.EditorAssetLibrary.does_asset_exist(_LYRA_ENEMY_BP_PATH):
        if not force_recreate:
            unreal.log(
                f"DeltaCode: {_LYRA_ENEMY_BP_NAME} already exists, skipping.")
            return unreal.load_asset(_LYRA_ENEMY_BP_PATH)
        unreal.EditorAssetLibrary.delete_asset(_LYRA_ENEMY_BP_PATH)
        unreal.log(f"DeltaCode: {_LYRA_ENEMY_BP_NAME} deleted for recreate.")

    parent_class = unreal.load_class(None, _LYRA_CHARACTER_CLASS_PATH)
    if parent_class is None:
        unreal.log_warning(
            f"DeltaCode: ALyraCharacter unavailable — cannot create "
            f"{_LYRA_ENEMY_BP_NAME}. Falling back to B_DC_EnemyBase.")
        return None

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    blueprint = asset_tools.create_asset(
        _LYRA_ENEMY_BP_NAME, _CORE_PACKAGE_PATH, unreal.Blueprint, factory)
    if blueprint is None:
        unreal.log_error(
            f"DeltaCode: create_asset returned None for {_LYRA_ENEMY_BP_NAME}.")
        return None

    # DC components — DCEnemyAIController and DCBTTask_AttackTarget look these
    # up via FindComponentByClass instead of casting to a concrete pawn class.
    # DCEnemyAIData carries the BehaviorTree pointer; the controller falls
    # back to FindComponentByClass<UDCEnemyAIData>() when IDCEnemyAIPawn
    # returns null, which is how this BP-only implementer gets a tree at
    # possession time. The BT pointer itself is set later by
    # dc_create_ai_assets._wire_lyra_enemy_blueprint once the BT asset exists.
    _add_dc_component(blueprint, _LYRA_ENEMY_BP_NAME,
                      unreal.DCFactionComponent, "DCFactionComponent")
    _add_dc_component(blueprint, _LYRA_ENEMY_BP_NAME,
                      unreal.DCEquipmentComponent, "DCEquipmentComponent")
    _add_dc_component(blueprint, _LYRA_ENEMY_BP_NAME,
                      unreal.DCHealthComponent, "DCHealthComponent")
    _add_dc_component(blueprint, _LYRA_ENEMY_BP_NAME,
                      unreal.DCEnemyAIData, "DCEnemyAIData")

    # Default AI controller — same controller B_DC_EnemyBase uses, now driven
    # via IDCEnemyAIPawn.
    try:
        gen_class = blueprint.generated_class()
        cdo = unreal.get_default_object(gen_class)
        ai_controller_class = unreal.load_class(
            None, "/Script/DeltaCode.DCEnemyAIController")
        if ai_controller_class is not None:
            cdo.set_editor_property('ai_controller_class', ai_controller_class)
            cdo.set_editor_property(
                'auto_possess_ai',
                unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
            unreal.log(
                f"DeltaCode: [{_LYRA_ENEMY_BP_NAME}] AIControllerClass set "
                f"→ DCEnemyAIController.")
        else:
            unreal.log_warning(
                f"DeltaCode: [{_LYRA_ENEMY_BP_NAME}] DCEnemyAIController "
                f"unavailable — pawn will spawn unpossessed.")
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: [{_LYRA_ENEMY_BP_NAME}] AIController wiring "
            f"failed — {e}")

    # Lyra mannequin mesh + AnimBP on the inherited CharacterMesh0. ALyraCharacter
    # ships without a mesh assigned by default (it normally comes from PawnData
    # via the Experience system). Outside an Experience, set them ourselves so
    # the pawn isn't invisible.
    try:
        gen_class = blueprint.generated_class()
        cdo = unreal.get_default_object(gen_class)
        all_mesh = cdo.get_components_by_class(unreal.SkeletalMeshComponent)
        mesh_comp = next(
            (c for c in all_mesh if c.get_name() == "CharacterMesh0"),
            all_mesh[0] if all_mesh else None)
        if mesh_comp is not None:
            skel = _resolve_mannequin_mesh()
            if skel is not None:
                mesh_comp.set_editor_property("skeletal_mesh_asset", skel)
            anim_class = _resolve_mannequin_anim_class()
            if anim_class is not None:
                mesh_comp.set_editor_property(
                    "animation_mode",
                    unreal.AnimationMode.ANIMATION_BLUEPRINT)
                mesh_comp.set_editor_property("anim_class", anim_class)
            unreal.log(
                f"DeltaCode: [{_LYRA_ENEMY_BP_NAME}] mesh + anim class set on "
                f"CharacterMesh0.")
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: [{_LYRA_ENEMY_BP_NAME}] mesh/anim wiring failed "
            f"— {e}")

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: compile_blueprint failed on "
            f"{_LYRA_ENEMY_BP_NAME} — {e}")

    unreal.EditorAssetLibrary.save_asset(_LYRA_ENEMY_BP_PATH)
    unreal.log(f"DeltaCode: Created {_LYRA_ENEMY_BP_NAME}.")
    return blueprint


def create_core_blueprints(force_recreate=False):
    """Create all required DeltaCode Blueprint assets under /Game/DeltaCode/Core/.

    Safe to call repeatedly — skips assets that already exist unless
    force_recreate=True, which deletes and rebuilds every asset with the
    latest mesh/color/scale configuration.

    Called automatically at the start of run_danger_zone() (with
    force_recreate=True), and can be invoked independently from the
    DeltaCode panel's 'Create Core Assets' button.
    """
    created = 0
    for bp_name, parent_path in _CORE_BLUEPRINTS:
        result = _create_blueprint_if_missing(
            _CORE_PACKAGE_PATH, bp_name, parent_path,
            force_recreate=force_recreate)
        if result is not None:
            created += 1

    # Lyra-parented enemy. Best-effort: failure is logged inside and we fall
    # back to B_DC_EnemyBase via _apply_scan_to_dc_classes.
    try:
        _create_lyra_enemy_blueprint(force_recreate=force_recreate)
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: B_DC_LyraEnemyBase creation raised — {e}. "
            f"Falling back to B_DC_EnemyBase.")

    unreal.log(f"DeltaCode: core blueprint check complete — {created}/{len(_CORE_BLUEPRINTS)} assets ready.")


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

_BUILDERS = {
    "extraction":    build_extraction,
    "arena":         build_arena,
    "questhub":      build_questhub,
    "reactivestory": build_reactivestory,
}

def _spawn_level_essentials():
    """Add lighting + NavMesh bounds to a freshly created sandbox level so
    PIE isn't pitch black and spawned AI has something to path on. Called
    on first-time creation of L_DC_DangerZone; clear_level() preserves
    these actors across subsequent Build Mission runs via _PRESERVE_CLASSES."""
    # Sun light: (0, 225, 30) — yaw 225° puts the sun in the back-left quadrant
    # with a 30° pitch, giving long raking shadows for screenshot-friendly framing.
    dir_light = _spawn_registered(
        unreal.DirectionalLight,
        unreal.Vector(0, 0, 500),
        unreal.Rotator(0, 225, 30))
    if dir_light is not None:
        dir_light.set_actor_label("DC_DirectionalLight")

    # Ambient fill — samples the SkyAtmosphere to light unlit faces.
    sky_light = _spawn_registered(
        unreal.SkyLight,
        unreal.Vector(0, 0, 500),
        unreal.Rotator(0, 0, 0))
    if sky_light is not None:
        sky_light.set_actor_label("DC_SkyLight")

    # Physically-based sky model — gives the SkyLight something to sample.
    atmosphere = _spawn_registered(
        unreal.SkyAtmosphere,
        unreal.Vector(0, 0, 0),
        unreal.Rotator(0, 0, 0))
    if atmosphere is not None:
        atmosphere.set_actor_label("DC_SkyAtmosphere")

    _ensure_navmesh_bounds()


def _ensure_directional_light_rotation():
    """Force DC_DirectionalLight to (0, 225, 30) on every Build Mission.

    DirectionalLight is in _PRESERVE_CLASSES, so once L_DC_DangerZone exists
    the spawn block in _spawn_level_essentials never runs again — meaning
    rotation tweaks landed in source don't reach already-built levels until
    the user manually re-aims the light. This helper closes the gap by
    rewriting the rotation every build. Idempotent. startswith match catches
    suffixed labels like 'DC_DirectionalLight_2'.

    If you've manually re-aimed DC_DirectionalLight in the editor, the next
    Build Mission will overwrite that change — by design.
    """
    target_rot = unreal.Rotator(0, 225, 30)
    for actor in _actor_subsystem().get_all_level_actors():
        if not isinstance(actor, unreal.DirectionalLight):
            continue
        if not actor.get_actor_label().startswith("DC_DirectionalLight"):
            continue
        actor.set_actor_rotation(target_rot, teleport_physics=False)
        unreal.log(
            f"DeltaCode: {actor.get_actor_label()} rotation set to (0, 225, 30).")


def _ensure_navmesh_bounds():
    """Spawn a NavMeshBoundsVolume if none exists in the current level.

    Sized 30000×30000×2000 cm centered at origin — covers every current
    mission template with margin. Worst case is reactivestory (25000×25000
    at origin, ~2500cm margin); arena (14000×3000 at x=7000) gets ~1000cm
    margin on the +X edge; extraction and questhub are fully interior.

    Idempotent: safe to call every Build Mission. Also called from the
    load_level branch as a backfill for levels created before NavMesh
    auto-spawn was added.
    """
    existing = [a for a in _actor_subsystem().get_all_level_actors()
                if isinstance(a, unreal.NavMeshBoundsVolume)]
    if existing:
        unreal.log(
            f"DeltaCode: NavMeshBoundsVolume already present ({len(existing)}) — skipping.")
        return

    volume = _spawn_registered(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(0, 0, 0),
        unreal.Rotator(0, 0, 0))
    if volume is None:
        unreal.log_warning("DeltaCode: failed to spawn NavMeshBoundsVolume.")
        return
    # Default NavMeshBoundsVolume brush is 200×200×200 cm. Scale to 30000×30000×2000.
    volume.set_actor_scale3d(unreal.Vector(150.0, 150.0, 10.0))
    volume.set_actor_label("DC_NavMeshBounds")
    unreal.log("DeltaCode: spawned DC_NavMeshBounds (30000×30000×2000 cm at origin).")


def run_danger_zone(mission_template):
    """Clear the level, then spawn the template's mission layout.

    Called by FDCLevelScriptingBridge::ExecuteDangerZoneScript. Any exception
    raised here will propagate back to the bridge as a failed ExecPythonCommand
    and surface in the Output Log (LogPython).
    """
    # Unconditionally operate on a dedicated non-WP sandbox level. First run
    # creates it; every later run just opens it. Never touches whatever level
    # the user had loaded.
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if not unreal.EditorAssetLibrary.does_asset_exist(_DC_DANGER_ZONE_LEVEL):
        les.new_level(_DC_DANGER_ZONE_LEVEL)
        _spawn_level_essentials()
        _ensure_directional_light_rotation()
        # Persist lights + navmesh so the next run's load_level branch sees
        # them instead of an empty saved level.
        les.save_current_level()
        unreal.log("DeltaCode: created L_DC_DangerZone")
    else:
        les.load_level(_DC_DANGER_ZONE_LEVEL)
        # Backfill navmesh on levels created before auto-spawn existed.
        # Idempotent — no-op if the level already has one.
        _ensure_navmesh_bounds()
        # Rewrite preserved DirectionalLight rotation to match current source —
        # _PRESERVE_CLASSES would otherwise pin existing levels to old values.
        _ensure_directional_light_rotation()
        unreal.log("DeltaCode: opened L_DC_DangerZone")

    world = get_editor_world()
    if world is None:
        unreal.log_error("DeltaCode: no editor world found after opening L_DC_DangerZone.")
        return

    builder = _BUILDERS.get(mission_template)
    if builder is None:
        unreal.log_error(f"DeltaCode: unknown mission template '{mission_template}'.")
        return

    unreal.log(f"DeltaCode: Danger Zone start — template='{mission_template}'")

    # Project scan — informs DC_CLASSES routing and surfaces what content
    # the project ships with. importlib.reload keeps iterative edits hot
    # during development. Failure falls back to B_DC_EnemyBase routing.
    try:
        import importlib
        import dc_inspect_project
        importlib.reload(dc_inspect_project)
        scan = dc_inspect_project.dc_inspect_project(topic="all", silent=True)
        unreal.log(f"DeltaCode: project scan complete — found "
                   f"{len(scan.get('player', []))} player chars, "
                   f"{len(scan.get('enemy', []))} enemy classes, "
                   f"{len(scan.get('animation', []))} animations")
    except Exception as e:
        unreal.log_warning(f"DeltaCode: project scan failed — {e}")
        scan = {}

    _apply_scan_to_dc_classes(scan)
    _resolve_character_z_offset()

    # force_recreate=False: BP recreation with mesh is no longer needed since
    # _apply_mesh_to_actor now sets the mesh on every spawned instance.
    create_core_blueprints(force_recreate=False)

    # AI asset bootstrap — creates BT_DC_Enemy + BB_DC_Enemy_Default under
    # /Game/DeltaCode/AI/ and wires the BT into B_DC_EnemyBase so possession
    # runs the tree. Must come after create_core_blueprints because the wire
    # step depends on B_DC_EnemyBase existing in Content. importlib.reload
    # keeps iterative edits hot during development.
    try:
        import importlib
        import dc_create_ai_assets
        importlib.reload(dc_create_ai_assets)
        dc_create_ai_assets.dc_create_ai_assets()
    except Exception as e:
        unreal.log_warning(f"DeltaCode: AI asset bootstrap failed — {e}")

    clear_level()
    create_floor(mission_template)
    builder(world)
    unreal.log(f"DeltaCode: Danger Zone complete — template='{mission_template}'")

    # Persist the just-built mission so the user can iterate without an
    # extra Save All step. Save All has been observed to no-op on the
    # built level (the active-editor-level pointer can desync from the
    # level that received the spawns), so we save explicitly here.
    # Save failures are non-fatal — the level is still in memory either
    # way, and the warning surfaces in the Output Log for diagnosis.
    try:
        if les.save_current_level():
            unreal.log("DeltaCode: level saved.")
        else:
            unreal.log_warning(
                "DeltaCode: save_current_level returned false — "
                "save the level manually via File > Save Current Level.")
    except Exception as e:
        unreal.log_warning(f"DeltaCode: save_current_level raised — {e}")

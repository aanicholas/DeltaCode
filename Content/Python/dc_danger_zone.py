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
    # Combat-variant template enemy — ships pre-wired with mesh, anim,
    # AIController, and BehaviorTree. _refresh_dynamic_classes() may override
    # boss_base at run_danger_zone start if a dedicated boss variant exists.
    "enemy_base":      "/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy.BP_CombatEnemy_C",
    "boss_base":       "/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy.BP_CombatEnemy_C",
    "spawn_zone":      "/Game/DeltaCode/Core/B_DC_SpawnZone.B_DC_SpawnZone_C",
    "extraction_zone": "/Game/DeltaCode/Core/B_DC_ExtractionZone.B_DC_ExtractionZone_C",
}

# Blueprint class paths whose instances already carry mesh / anim / AI / BT
# from the template. _apply_mesh_to_actor skips them entirely so we don't
# overwrite the template's intended setup. Match is done via substring so
# either the package path or `.ClassName_C` form hits.
_PRE_EQUIPPED_CLASS_PATHS = (
    "/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy",
)

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
    """Rebuild navigation for the current editor world so spawned AI can path.

    UNavigationSystemV1::Build() is not a UFUNCTION, so it cannot be called
    from Python directly. We trigger the rebuild via the engine console command
    instead, which is the standard workaround for editor Python scripts.
    """
    try:
        world = get_editor_world()
        if world is None:
            unreal.log_warning("DeltaCode: No editor world — skipping navmesh rebuild.")
            return
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        unreal.log("DeltaCode: Navigation mesh rebuild triggered.")
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

_MANNEQUIN_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
_MANNEQUIN_ANIM_CLASS = "/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"
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


def _refresh_dynamic_classes():
    """Probe /Game/Variant_Combat/Blueprints/AI/ at build time and overlay
    DC_CLASSES. If a dedicated Boss variant ships with the template, use it
    for boss_base; log any spawners so we know to ignore them (they are not
    pawns — spawning them would place a spawner actor in the world instead
    of an enemy)."""
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        assets = registry.get_assets_by_path(
            "/Game/Variant_Combat/Blueprints/AI", recursive=True)
    except Exception as e:
        unreal.log_warning(f"DeltaCode: AI-path probe failed — {e}")
        return

    if not assets:
        unreal.log("DeltaCode: /Game/Variant_Combat/Blueprints/AI — empty or "
                   "missing. Using DC_CLASSES defaults.")
        return

    for a in assets:
        name = str(a.asset_name)
        pkg  = str(a.package_name)
        if "Spawner" in name:
            unreal.log(f"DeltaCode: ignoring spawner BP {pkg} "
                       f"(would not spawn as a pawn).")
            continue
        if "Boss" in name:
            boss_path = f"{pkg}.{name}_C"
            DC_CLASSES["boss_base"] = boss_path
            unreal.log(f"DeltaCode: boss_base overridden → {boss_path}")


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

    skel_mesh = unreal.load_asset(_MANNEQUIN_MESH)
    if skel_mesh is None:
        return
    # Prefer the inherited ACharacter mesh (CharacterMesh0). In a blank
    # project that hasn't run _add_mannequin_mesh's SCS path, that's the
    # only SkeletalMeshComponent present — a bare class-based lookup
    # without the name match would miss it and leave the enemy mesh-less.
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
        # Third-person combat anim BP — best fit for enemy/boss behaviour.
        anim_class = unreal.load_class(
            None,
            "/Game/Variant_Combat/Anims/ABP_Manny_Combat.ABP_Manny_Combat_C")
        if anim_class is not None:
            mesh_comp.set_editor_property("anim_class", anim_class)
        else:
            unreal.log_warning(
                f"DeltaCode: failed to load ABP_Manny_Combat for "
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


def _resolve_asset_by_name(class_name, name_fragments, fallback_path):
    """Find an asset object path via AssetRegistry name match.

    Iterates all assets of `class_name` and returns the first whose asset name
    contains any substring in `name_fragments`. Falls back to `fallback_path`
    if the registry is unavailable or nothing matches. Every outcome is logged
    so the Output Log shows which path actually got used.
    """
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        results = registry.get_assets_by_class(class_name)
        for r in results:
            name = str(r.asset_name)
            if any(frag in name for frag in name_fragments):
                resolved = str(r.object_path)
                unreal.log(
                    f"DeltaCode: registry resolved {class_name}:{name_fragments} "
                    f"→ {resolved}")
                return resolved
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: registry lookup failed for "
            f"{class_name}:{name_fragments} — {e}")
    unreal.log(
        f"DeltaCode: {class_name}:{name_fragments} not found in registry, "
        f"using fallback {fallback_path}")
    return fallback_path


def _add_mannequin_mesh(blueprint, bp_name):
    """Add a SkeletalMeshComponent with the mannequin mesh + anim blueprint.

    Mesh and anim-blueprint paths are resolved via AssetRegistry so the script
    survives Epic moving assets between engine versions / template refreshes.
    Constants act as fallback if the registry lookup finds nothing.
    """
    try:
        scs = blueprint.get_editor_property('simple_construction_script')
        if scs is None:
            unreal.log_warning(f"DeltaCode: [{bp_name}] SCS is None — cannot add mesh component.")
            return

        node = scs.create_node(unreal.SkeletalMeshComponent.static_class())
        if node is None:
            unreal.log_warning(f"DeltaCode: [{bp_name}] SCS.create_node returned None.")
            return
        unreal.log(f"DeltaCode: [{bp_name}] SCS node created.")

        comp = node.component_template

        # Skeletal mesh.
        mesh_path = _resolve_asset_by_name(
            "SkeletalMesh", ("Manny",), _MANNEQUIN_MESH)
        skel_mesh = unreal.load_asset(mesh_path)
        if skel_mesh is not None:
            comp.set_editor_property("skeletal_mesh_asset", skel_mesh)
            unreal.log(f"DeltaCode: [{bp_name}] Loaded mesh: {mesh_path}")
        else:
            unreal.log_warning(f"DeltaCode: [{bp_name}] FAILED to load mesh: {mesh_path}")

        # Anim blueprint class. AssetRegistry returns the BP asset path; the
        # SkeletalMeshComponent.anim_class property wants the generated UClass,
        # which lives at the same path with a "_C" suffix.
        anim_bp_path = _resolve_asset_by_name(
            "AnimBlueprint", ("Manny", "ThirdPerson"), _MANNEQUIN_ANIM_CLASS)
        if not anim_bp_path.endswith("_C"):
            anim_bp_path = anim_bp_path + "_C"
        anim_class = unreal.load_class(None, anim_bp_path)
        if anim_class is not None:
            comp.set_editor_property("anim_class", anim_class)
            unreal.log(f"DeltaCode: [{bp_name}] anim_class set: {anim_bp_path}")
        else:
            unreal.log_warning(
                f"DeltaCode: [{bp_name}] FAILED to load anim class: {anim_bp_path}")

        scale = _CORE_SCALES.get(bp_name, 1.0)
        if scale != 1.0:
            comp.set_editor_property(
                "relative_scale3d", unreal.Vector(scale, scale, scale))
            unreal.log(f"DeltaCode: [{bp_name}] Scale set to {scale}.")

        scs.add_node(node)
        unreal.log(f"DeltaCode: [{bp_name}] SkeletalMeshComponent added to SCS.")
    except Exception as e:
        unreal.log_warning(
            f"DeltaCode: Could not add mannequin mesh to "
            f"{blueprint.get_name()} — {e}")


def _create_blueprint_if_missing(package_path, bp_name, parent_class_path,
                                 force_recreate=False):
    """Create a Blueprint asset if it doesn't already exist.

    When force_recreate is True, deletes the existing asset first so it gets
    rebuilt with the latest mesh/color/scale setup.
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
        _add_mannequin_mesh(blueprint, bp_name)
        unreal.EditorAssetLibrary.save_asset(full_path)
        unreal.log(f"DeltaCode: Created {bp_name}")
    else:
        unreal.log_error(f"DeltaCode: Failed to create {bp_name}")
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

    unreal.log(f"DeltaCode: core blueprint check complete — {created}/{len(_CORE_BLUEPRINTS)} assets ready.")


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

_BUILDERS = {
    "extraction":    build_extraction,
    "arena":         build_arena,
    "questhub":      build_questhub,
    "reactivestory": build_reactivestory,
}

def _spawn_default_lighting():
    """Add DirectionalLight + SkyLight + SkyAtmosphere to a freshly created
    sandbox level so PIE isn't pitch black. Called only on first-time
    creation of L_DC_DangerZone; subsequent runs load the saved level which
    already contains these actors."""
    # Sun light at -45° pitch — standard "sun from above/behind" angle.
    dir_light = _spawn_registered(
        unreal.DirectionalLight,
        unreal.Vector(0, 0, 500),
        unreal.Rotator(-45, 0, 0))
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
        _spawn_default_lighting()
        # Persist the lights so the next run's load_level branch sees them
        # instead of an empty saved level.
        les.save_current_level()
        unreal.log("DeltaCode: created L_DC_DangerZone")
    else:
        les.load_level(_DC_DANGER_ZONE_LEVEL)
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

    # Overlay DC_CLASSES from the template's AI folder (boss variant detection,
    # spawner disambiguation) and then sync _CHARACTER_Z_OFFSET to whatever
    # enemy class we ended up with. Order matters: refresh first, then Z.
    _refresh_dynamic_classes()
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

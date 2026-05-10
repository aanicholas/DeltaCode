# DeltaCode - Unreal Engine Code Helper
# Copyright (c) 2026 Andrew Nicholas
#
# This program is free software: you can redistribute
# it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software
# Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dc_inspect_project.py — Safe Mode project inspector.
#
# Read-only scan of the current UE project's content. Reports player
# characters, enemy/NPC classes, damage system, animation, and input
# assets. Pure introspection — never calls set_editor_property,
# create_asset, save_asset, or any other mutation API.
#
# Entry points:
#     dc_inspect_project()              # all categories, log to Output Log
#     dc_inspect_project("player")      # one category
#     dc_inspect_project("ai")          # alias for "enemy"
#     dc_inspect_project("combat")      # alias for "damage"
#     dc_inspect_project(silent=True)   # return dict, no log
#
# Default scan scope is /Game/ — engine and plugin content excluded.

import unreal

# ─── CONFIG ──────────────────────────────────────────────────────────────────

_DEFAULT_SCAN_PATH = "/Game"

# Class path tuples (module, class_name) for AssetRegistry filters.
_CHARACTER_CP        = ("/Script/Engine",        "Character")
_AI_CONTROLLER_CP    = ("/Script/AIModule",      "AIController")
_BLUEPRINT_CP        = ("/Script/Engine",        "Blueprint")
_STATIC_MESH_CP      = ("/Script/Engine",        "StaticMesh")
_SKELETAL_MESH_CP    = ("/Script/Engine",        "SkeletalMesh")
_BT_CP               = ("/Script/AIModule",      "BehaviorTree")
_BB_CP               = ("/Script/AIModule",      "BlackboardData")
_ANIM_MONTAGE_CP     = ("/Script/Engine",        "AnimMontage")
_ANIM_BP_CP          = ("/Script/Engine",        "AnimBlueprint")
_ANIM_SEQ_CP         = ("/Script/Engine",        "AnimSequence")
_INPUT_ACTION_CP     = ("/Script/EnhancedInput", "InputAction")
_IMC_CP              = ("/Script/EnhancedInput", "InputMappingContext")

# GAS — present whenever the GameplayAbilities module is loaded.
_GA_CP                          = ("/Script/GameplayAbilities", "GameplayAbility")
_GE_CP                          = ("/Script/GameplayAbilities", "GameplayEffect")
_AS_CP                          = ("/Script/GameplayAbilities", "AttributeSet")
_GC_NOTIFY_STATIC_CP            = ("/Script/GameplayAbilities", "GameplayCueNotify_Static")
_GC_NOTIFY_ACTOR_CP             = ("/Script/GameplayAbilities", "GameplayCueNotify_Actor")

# Lyra-specific types (module /Script/LyraGame). Each scan tolerates absence
# so non-Lyra projects fall through to empty results.
_LYRA_ABILITY_SET_CP            = ("/Script/LyraGame", "LyraAbilitySet")
_LYRA_EXPERIENCE_DEF_CP         = ("/Script/LyraGame", "LyraExperienceDefinition")
_LYRA_EXPERIENCE_ACTIONSET_CP   = ("/Script/LyraGame", "LyraExperienceActionSet")
_LYRA_PAWN_DATA_CP              = ("/Script/LyraGame", "LyraPawnData")
_LYRA_INPUT_CONFIG_CP           = ("/Script/LyraGame", "LyraInputConfig")
_LYRA_CAMERA_MODE_CP            = ("/Script/LyraGame", "LyraCameraMode")
_LYRA_INV_ITEM_DEF_CP           = ("/Script/LyraGame", "LyraInventoryItemDefinition")
_LYRA_INV_FRAGMENT_CP           = ("/Script/LyraGame", "LyraInventoryItemFragment")
_LYRA_EQUIPMENT_DEF_CP          = ("/Script/LyraGame", "LyraEquipmentDefinition")
_LYRA_WEAPON_INSTANCE_CP        = ("/Script/LyraGame", "LyraWeaponInstance")

# Game Features — used by Lyra and any modular UE project.
_GAME_FEATURE_DATA_CP           = ("/Script/GameFeatures", "GameFeatureData")
_GAME_FEATURE_ACTION_CP         = ("/Script/GameFeatures", "GameFeatureAction")

# Modular Gameplay — base classes Lyra characters extend.
_MODULAR_CHARACTER_CP           = ("/Script/ModularGameplayActors", "ModularCharacter")


# ─── EXTENSIBLE SCAN REGISTRY ────────────────────────────────────────────────
#
# Simple class-path-driven categories live here and are dispatched generically
# through _scan_assets_by_class(). Categories that need bespoke heuristics
# (player vs enemy disambiguation, parent-class substring routing, etc.)
# remain in their hand-written _scan_*/_format_* functions in the
# _CATEGORY_FN table further down — DO NOT migrate those into this registry.
#
# To add a new simple category:
#   1. Append one entry to SCAN_CATEGORIES with: label, classes, require_folder.
#   2. Add the topic name to _TOPICS_TO_CATEGORIES (one-line mapping).
#   3. Done. format_scan_for_llm picks up the new label automatically.
#
# Set require_folder=True for categories that are too noisy to scan globally
# (e.g. Texture2D with thousands of assets). Those return an empty list with
# a log warning when called without a folder= argument.
#
# Future categories to plan for (NOT yet implemented):
#   materials    — Material, MaterialInstanceConstant
#   audio        — SoundBase, SoundCue, SoundWave
#   effects      — NiagaraSystem, ParticleSystem
#   ui           — WidgetBlueprint
#   textures     — Texture2D (require_folder=True)
#   environments — StaticMesh filtered to /Game/Environment (require_folder=True)
SCAN_CATEGORIES = {
    "meshes": {
        "label":          "Meshes",
        "classes":        [(_STATIC_MESH_CP,   "SM"),
                           (_SKELETAL_MESH_CP, "SK")],
        "require_folder": False,
    },
}


_TOPICS_TO_CATEGORIES = {
    None:           ("player", "enemy", "damage", "animation", "input"),
    "all":          ("player", "enemy", "damage", "animation", "input",
                     "gas", "experience", "equipment", "inventory",
                     "gamefeatures", "input_config", "camera"),
    "player":       ("player",),
    "enemy":        ("enemy",),
    "ai":           ("enemy",),
    "damage":       ("damage",),
    "combat":       ("damage",),
    "animation":    ("animation",),
    "input":        ("input",),
    # Lyra / GAS additions.
    "gas":          ("gas",),
    "abilities":    ("gas",),
    "experience":   ("experience",),
    "equipment":    ("equipment",),
    "inventory":    ("inventory",),
    "gamefeatures": ("gamefeatures",),
    "input_config": ("input_config",),
    "camera":       ("camera",),
    "lyra":         ("experience", "gas", "equipment", "inventory",
                     "gamefeatures", "input_config", "camera"),
    # Registry-driven categories — see SCAN_CATEGORIES above. Deliberately
    # NOT included in 'all' or 'lyra' (opt-in only).
    "meshes":       ("meshes",),
}


# ─── LOGGING ─────────────────────────────────────────────────────────────────

def _log(msg):
    """Prefixed log — for diagnostics and warnings."""
    unreal.log(f"DeltaCode[Inspect]: {msg}")


def _out(msg=""):
    """Bare log — for the tabular report output."""
    unreal.log(msg)


# ─── REGISTRY HELPERS ────────────────────────────────────────────────────────

def _registry():
    return unreal.AssetRegistryHelpers.get_asset_registry()


def _tlap(cp):
    """(module, name) → TopLevelAssetPath."""
    return unreal.TopLevelAssetPath(cp[0], cp[1])


def _filter_assets(class_path, scan_path=_DEFAULT_SCAN_PATH, recursive=True):
    """All AssetData of given class under scan_path."""
    f = unreal.ARFilter(
        class_paths=[_tlap(class_path)],
        package_paths=[scan_path],
        recursive_paths=recursive,
    )
    try:
        return list(_registry().get_assets(f))
    except Exception as e:
        _log(f"WARN: get_assets failed for {class_path} — {e}")
        return []


def _filter_assets_multi(class_path, scan_paths, recursive=True):
    """All AssetData of class across multiple package roots.

    Used by Lyra-aware categories so plugin content (/ShooterCore/, etc.) is
    included alongside /Game/. Deduplicates by package_name.
    """
    seen = set()
    out = []
    for p in scan_paths:
        for ad in _filter_assets(class_path, scan_path=p, recursive=recursive):
            key = str(ad.package_name)
            if key in seen:
                continue
            seen.add(key)
            out.append(ad)
    return out


def _normalize_folder(folder):
    """Folder filter normalizer — 'ShooterCore/Foo' → '/ShooterCore/Foo'.
    Leaves already-absolute paths like '/Game/Foo' alone."""
    if not folder:
        return folder
    return folder if folder.startswith("/") else "/" + folder


def _scan_assets_by_class(class_specs, folder=None):
    """Generic scanner used by every entry in SCAN_CATEGORIES.

    class_specs: list of ((module, classname), kind_label) pairs.
    folder:      mount-relative path like '/ShooterCore/Weapons/Shotgun'.
                 If set, the scan is restricted to that single package_path
                 (no recursion into other mounts). If None, the scan covers
                 /Game plus every Game Feature plugin mount.
    Returns:     list of {"kind", "name", "path"} rows — same shape as
                 _scan_animation/_scan_input output, formatted via _emit_compact.
    """
    paths = [_normalize_folder(folder)] if folder else _lyra_scan_paths()
    rows = []
    for class_path, kind in class_specs:
        for ad in _filter_assets_multi(class_path, paths):
            rows.append({"kind": kind,
                         "name": str(ad.asset_name),
                         "path": _asset_full_path(ad)})
    return rows


def _format_registry_category(label, rows):
    """Generic format used by every entry in SCAN_CATEGORIES."""
    _hr_section(label, len(rows))
    for r in rows:
        _emit_compact(r)


def _lyra_plugin_mount_points():
    """Game Feature plugin mount points discovered on disk.

    Reads ``Plugins/GameFeatures/<Name>/<Name>.uplugin`` under the project
    directory and returns mount-point strings like ``/ShooterCore`` for use
    as ``package_paths`` in AssetRegistry filters. Returns [] for non-Lyra
    projects (no GameFeatures dir).
    """
    import os
    try:
        proj_dir = unreal.SystemLibrary.get_project_directory()
    except Exception:
        return []
    gf_dir = os.path.join(proj_dir, "Plugins", "GameFeatures")
    if not os.path.isdir(gf_dir):
        return []
    out = []
    for name in sorted(os.listdir(gf_dir)):
        uplugin = os.path.join(gf_dir, name, f"{name}.uplugin")
        if os.path.isfile(uplugin):
            out.append(f"/{name}")
    return out


def _lyra_scan_paths():
    """/Game plus all discovered Game Feature plugin mounts. Used by all
    Lyra-aware scans so plugin content is included by default."""
    return [_DEFAULT_SCAN_PATH] + _lyra_plugin_mount_points()


def _native_classes_derived_from(parent_cp):
    """Names of native (C++) classes derived from parent_cp. Returns paths
    like '/Script/DeltaCode.DCCharacterBase'. BP-generated class names
    (ending in '_C') are filtered out."""
    try:
        derived = _registry().get_derived_class_names(
            [_tlap(parent_cp)], set())
        return [str(n) for n in derived
                if str(n).startswith('/Script/') and not str(n).endswith('_C')]
    except Exception as e:
        _log(f"WARN: get_derived_class_names failed for {parent_cp} — {e}")
        return []


def _strip_class_wrapper(s):
    """Convert "Class'/Script/Engine.Character'" → "/Script/Engine.Character"."""
    s = str(s).strip()
    if s.startswith("Class'") and s.endswith("'"):
        return s[6:-1]
    return s


def _bp_parent_class_path(asset_data):
    """Blueprint asset's parent class path (string), or None.

    Tries the cheap AssetData tag first; falls back to loading the asset
    and reading parent_class() if the tag isn't Python-accessible on this
    UE version. Load fallback is slower but reliable.
    """
    try:
        tag = asset_data.get_tag_value('ParentClass')
        if tag:
            return _strip_class_wrapper(tag)
    except Exception:
        pass
    try:
        obj = unreal.load_asset(_asset_full_path(asset_data))
        if obj is not None and hasattr(obj, 'parent_class'):
            pc = obj.parent_class()
            if pc is not None:
                return pc.get_path_name()
    except Exception:
        pass
    return None


def _bp_type(asset_data):
    """BlueprintType tag — e.g. 'BPTYPE_Normal', 'BPTYPE_Interface',
    'BPTYPE_FunctionLibrary'. None if unavailable."""
    try:
        tag = asset_data.get_tag_value('BlueprintType')
        return str(tag) if tag else None
    except Exception:
        return None


def _short_class_name(class_path):
    """/Script/Engine.Character → Character. Strips trailing _C on BP-gen."""
    if not class_path:
        return ""
    s = str(class_path).rsplit('.', 1)[-1]
    return s[:-2] if s.endswith('_C') else s


def _asset_full_path(asset_data):
    """/Game/Foo/Bar (no .asset_name suffix) — usable as load_asset arg."""
    return f"{asset_data.package_path}/{asset_data.asset_name}"


# ─── REPORT FORMATTING (LOG-ONLY HELPERS) ────────────────────────────────────

def _hr_double():
    _out("=" * 50)


def _hr_section(title, count):
    _out("")
    _out(f"--- {title} ({count}) ---")


def _emit_full(row):
    """Multi-line emit: header + path + parent + hints. For BP/native rows."""
    _out(f"  [{row['kind']}]  {row['name']}")
    _out(f"         Path:   {row['path']}")
    if row.get('parent'):
        _out(f"         Parent: {_short_class_name(row['parent'])}")
    hints = row.get('hints') or []
    if hints:
        _out(f"         Hint:   {'; '.join(hints)}")


def _emit_compact(row):
    """Single-line emit: just kind + name. For asset rows where the type
    label and short name say everything."""
    _out(f"  [{row['kind']}]  {row['name']}")


# ─── CATEGORY: PLAYER CHARACTERS ─────────────────────────────────────────────

def _candidate_player_pawn_class_paths():
    """Aggregate DefaultPawnClass references across GameMode-derived BPs in
    /Game/. Returns a set of pawn class path strings — used by
    _scan_player_characters() to flag likely player pawns."""
    pawns = set()
    for ad in _filter_assets(_BLUEPRINT_CP):
        parent = _bp_parent_class_path(ad)
        if not parent or 'GameMode' not in parent:
            continue
        try:
            bp = unreal.load_asset(_asset_full_path(ad))
            if not bp:
                continue
            cdo = unreal.get_default_object(bp.generated_class())
            pawn = cdo.get_editor_property('default_pawn_class')
            if pawn:
                pawns.add(pawn.get_path_name())
        except Exception:
            pass
    return pawns


def _all_character_rows():
    """All Character-derived BPs and native classes in /Game/, with the
    player-hint annotations populated. Shared by _scan_player_characters
    (filters to hinted) and _scan_enemy_npc (filters to non-hinted) so
    the registry walk runs once."""
    candidates = _candidate_player_pawn_class_paths()
    rows = []

    for ad in _filter_assets(_BLUEPRINT_CP):
        parent = _bp_parent_class_path(ad)
        if not (parent and 'Character' in parent):
            continue
        name = str(ad.asset_name)
        bp_path = _asset_full_path(ad)
        gen_path = f"{bp_path}.{name}_C"
        hints = []
        if 'Player' in name:
            hints.append("name contains 'Player'")
        if any(c == bp_path or c == gen_path or c.startswith(gen_path)
               for c in candidates):
            hints.append("matches GameMode.DefaultPawnClass")
        rows.append({
            "kind":   "BP",
            "name":   name,
            "path":   bp_path,
            "parent": parent,
            "hints":  hints,
        })

    for cp in _native_classes_derived_from(_CHARACTER_CP):
        rows.append({
            "kind":   "C++",
            "name":   _short_class_name(cp),
            "path":   cp,
            "parent": None,
            "hints":  [],
        })

    return rows


def _scan_player_characters():
    """Character subclasses WITH at least one player hint (name contains
    'Player' OR matches GameMode.DefaultPawnClass). Returns list of rows."""
    return [r for r in _all_character_rows() if r["hints"]]


def _format_player_characters(rows):
    _hr_section("Player Characters", len(rows))
    for r in rows:
        _emit_full(r)


# ─── CATEGORY: ENEMY / NPC ───────────────────────────────────────────────────

def _scan_enemy_npc():
    """Character subclasses WITHOUT player hints (likely enemies/NPCs),
    plus AIController BPs/natives, plus BehaviorTrees and BlackboardData
    in /Game/. Returns list of row dicts.

    Character subclasses appear here when the player heuristic doesn't
    match — neutral NPCs and ambiguously-named pawns will land here too.
    Caller-side identification (e.g. by exact name match) is the next
    layer of disambiguation."""
    rows = [r for r in _all_character_rows() if not r["hints"]]

    for ad in _filter_assets(_BLUEPRINT_CP):
        parent = _bp_parent_class_path(ad)
        if not (parent and 'AIController' in parent):
            continue
        rows.append({
            "kind":   "BP-AI",
            "name":   str(ad.asset_name),
            "path":   _asset_full_path(ad),
            "parent": parent,
            "hints":  [],
        })

    for cp in _native_classes_derived_from(_AI_CONTROLLER_CP):
        rows.append({
            "kind":   "C++-AI",
            "name":   _short_class_name(cp),
            "path":   cp,
            "parent": None,
            "hints":  [],
        })

    for ad in _filter_assets(_BT_CP):
        rows.append({
            "kind": "BT",
            "name": str(ad.asset_name),
            "path": _asset_full_path(ad),
        })

    for ad in _filter_assets(_BB_CP):
        rows.append({
            "kind": "BB",
            "name": str(ad.asset_name),
            "path": _asset_full_path(ad),
        })

    return rows


def _format_enemy_npc(rows):
    _hr_section("Enemy / NPC", len(rows))
    for r in rows:
        # Character subclasses (BP/C++) and AIController subclasses get the
        # full multi-line layout so the parent class shows. BTs/BBs are
        # plain assets — single-line is enough.
        if r["kind"] in ("BP", "C++", "BP-AI", "C++-AI"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── CATEGORY: DAMAGE SYSTEM ─────────────────────────────────────────────────

def _scan_damage_system():
    """Heuristic: BP interfaces or BPs with 'Damage'/'Damageable' in name;
    BPs with 'Health' in name (often health components or health BPs).
    Returns list of row dicts."""
    rows = []
    seen = set()

    for ad in _filter_assets(_BLUEPRINT_CP):
        name = str(ad.asset_name)
        is_damage = ('Damage' in name) or ('Damageable' in name)
        is_health = 'Health' in name
        if not (is_damage or is_health):
            continue
        if ad.package_name in seen:
            continue
        seen.add(ad.package_name)

        parent = _bp_parent_class_path(ad)
        bptype = _bp_type(ad)
        if bptype == 'BPTYPE_Interface' or (parent and parent.endswith('.Interface')):
            kind = "BPI"
        elif parent and 'Component' in parent:
            kind = "Cmp"
        else:
            kind = "BP"
        rows.append({
            "kind":   kind,
            "name":   name,
            "path":   _asset_full_path(ad),
            "parent": parent,
            "hints":  [],
        })

    return rows


def _format_damage_system(rows):
    _hr_section("Damage System", len(rows))
    for r in rows:
        _emit_full(r)


# ─── CATEGORY: ANIMATION ─────────────────────────────────────────────────────

def _scan_animation():
    """AnimMontages, AnimBlueprints, AnimSequences in /Game/. Returns list
    of row dicts."""
    rows = []
    for ad in _filter_assets(_ANIM_MONTAGE_CP):
        rows.append({"kind": "Montage",  "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for ad in _filter_assets(_ANIM_BP_CP):
        rows.append({"kind": "AnimBP",   "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for ad in _filter_assets(_ANIM_SEQ_CP):
        rows.append({"kind": "Sequence", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    return rows


def _format_animation(rows):
    _hr_section("Animation", len(rows))
    for r in rows:
        _emit_compact(r)


# ─── CATEGORY: INPUT ─────────────────────────────────────────────────────────

def _scan_input():
    """Enhanced Input — InputActions and InputMappingContexts. Returns list
    of row dicts."""
    rows = []
    for ad in _filter_assets(_INPUT_ACTION_CP):
        rows.append({"kind": "IA",  "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for ad in _filter_assets(_IMC_CP):
        rows.append({"kind": "IMC", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    return rows


def _format_input(rows):
    _hr_section("Input", len(rows))
    for r in rows:
        _emit_compact(r)


# ─── CATEGORY: GAS (ABILITIES / EFFECTS / ATTRIBUTES / CUES) ────────────────

def _scan_gas():
    """GAS surface across /Game and Lyra plugin mounts.

    Returns rows for: UGameplayAbility BPs + natives, UGameplayEffect BPs +
    natives, UAttributeSet natives (data instances are rare), GameplayCue
    notify BPs + natives, and ULyraAbilitySet data assets. The BP filter
    iterates the Blueprint asset list once and dispatches by parent class
    substring, so adding more GAS-adjacent kinds doesn't grow the scan cost.
    """
    paths = _lyra_scan_paths()
    bps = _filter_assets_multi(_BLUEPRINT_CP, paths)
    rows = []

    for ad in bps:
        parent = _bp_parent_class_path(ad)
        if not parent:
            continue
        # Order matters — GameplayCueNotify is checked first because its
        # name does not contain GameplayAbility/GameplayEffect, but a future
        # rename could shadow if checked later.
        if 'GameplayCueNotify' in parent:
            kind = "GC-BP"
        elif 'GameplayEffect' in parent:
            kind = "GE-BP"
        elif 'GameplayAbility' in parent:
            kind = "GA-BP"
        else:
            continue
        rows.append({"kind": kind, "name": str(ad.asset_name),
                     "path": _asset_full_path(ad),
                     "parent": parent, "hints": []})

    for cp in _native_classes_derived_from(_GA_CP):
        rows.append({"kind": "GA-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_GE_CP):
        rows.append({"kind": "GE-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_AS_CP):
        rows.append({"kind": "AS", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_GC_NOTIFY_STATIC_CP):
        rows.append({"kind": "GC-Static-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_GC_NOTIFY_ACTOR_CP):
        rows.append({"kind": "GC-Actor-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})

    # ULyraAbilitySet — PrimaryDataAsset; instances ARE assets.
    for ad in _filter_assets_multi(_LYRA_ABILITY_SET_CP, paths):
        rows.append({"kind": "AbilitySet", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})

    return rows


def _format_gas(rows):
    _hr_section("GAS (Abilities / Effects / Attributes / Cues)", len(rows))
    for r in rows:
        # Native rows have parent=None and no hints; data-asset rows omit
        # parent entirely. Multi-line layout for anything with a parent.
        if r.get("parent"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── CATEGORY: EXPERIENCE (LYRA EXPERIENCES / PAWN DATA) ────────────────────

def _scan_experience():
    """ULyraExperienceDefinition / ActionSet / PawnData data assets.

    These are the wiring layer that selects which game features, abilities,
    pawn class, input config, and camera mode are active for a session. The
    Inspector's player heuristic is GameMode.DefaultPawnClass-based and
    misses Lyra's experience-driven flow entirely — this category surfaces
    the assets that actually drive pawn selection.
    """
    paths = _lyra_scan_paths()
    rows = []
    for ad in _filter_assets_multi(_LYRA_EXPERIENCE_DEF_CP, paths):
        rows.append({"kind": "Experience", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for ad in _filter_assets_multi(_LYRA_EXPERIENCE_ACTIONSET_CP, paths):
        rows.append({"kind": "ActionSet", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for ad in _filter_assets_multi(_LYRA_PAWN_DATA_CP, paths):
        rows.append({"kind": "PawnData", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    return rows


def _format_experience(rows):
    _hr_section("Experience / PawnData", len(rows))
    for r in rows:
        _emit_compact(r)


# ─── CATEGORY: EQUIPMENT (LYRA EQUIPMENT / WEAPONS) ─────────────────────────

def _scan_equipment():
    """ULyraEquipmentDefinition + ULyraWeaponInstance — BPs and natives.

    Equipment definitions describe what to grant on equip (ability sets,
    actors to spawn, instance class). Weapon instances are runtime
    per-equipped objects — Lyra subclasses them per weapon.
    """
    paths = _lyra_scan_paths()
    bps = _filter_assets_multi(_BLUEPRINT_CP, paths)
    rows = []
    for ad in bps:
        parent = _bp_parent_class_path(ad)
        if not parent:
            continue
        if 'LyraEquipmentDefinition' in parent:
            kind = "EquipDef-BP"
        elif 'LyraWeaponInstance' in parent or 'LyraEquipmentInstance' in parent:
            kind = "EquipInst-BP"
        else:
            continue
        rows.append({"kind": kind, "name": str(ad.asset_name),
                     "path": _asset_full_path(ad),
                     "parent": parent, "hints": []})
    for cp in _native_classes_derived_from(_LYRA_EQUIPMENT_DEF_CP):
        rows.append({"kind": "EquipDef-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_LYRA_WEAPON_INSTANCE_CP):
        rows.append({"kind": "WeaponInst-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    return rows


def _format_equipment(rows):
    _hr_section("Equipment / Weapons", len(rows))
    for r in rows:
        if r.get("parent"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── CATEGORY: INVENTORY (LYRA ITEM DEFS / FRAGMENT KINDS) ──────────────────

def _scan_inventory():
    """ULyraInventoryItemDefinition BPs + native fragment subclasses.

    Items are typically Blueprint subclasses of ULyraInventoryItemDefinition
    rather than data assets, so we filter BPs by parent. Fragment classes
    are inline-instanced subobjects of items (not assets), so we enumerate
    the available fragment SUBCLASSES — that tells callers what kinds of
    behavior items can be composed from.
    """
    paths = _lyra_scan_paths()
    bps = _filter_assets_multi(_BLUEPRINT_CP, paths)
    rows = []
    for ad in bps:
        parent = _bp_parent_class_path(ad)
        if parent and 'LyraInventoryItemDefinition' in parent:
            rows.append({"kind": "ItemDef-BP", "name": str(ad.asset_name),
                         "path": _asset_full_path(ad),
                         "parent": parent, "hints": []})
    for cp in _native_classes_derived_from(_LYRA_INV_ITEM_DEF_CP):
        rows.append({"kind": "ItemDef-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for cp in _native_classes_derived_from(_LYRA_INV_FRAGMENT_CP):
        rows.append({"kind": "Fragment-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    return rows


def _format_inventory(rows):
    _hr_section("Inventory (Items / Fragment Kinds)", len(rows))
    for r in rows:
        if r.get("parent"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── CATEGORY: GAME FEATURES ────────────────────────────────────────────────

def _scan_gamefeatures():
    """UGameFeatureData assets, native UGameFeatureAction subclasses, and
    .uplugin file enumeration of Plugins/GameFeatures/.

    The .uplugin walk is the only way to know which feature plugins are
    physically present; the Editor's enabled/disabled state changes at
    runtime and isn't inspected here. Combine with _scan_experience() to
    see which features each Experience pulls in.
    """
    paths = _lyra_scan_paths()
    rows = []
    for ad in _filter_assets_multi(_GAME_FEATURE_DATA_CP, paths):
        rows.append({"kind": "FeatureData", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    for cp in _native_classes_derived_from(_GAME_FEATURE_ACTION_CP):
        rows.append({"kind": "Action-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    for mount in _lyra_plugin_mount_points():
        rows.append({"kind": "Plugin", "name": mount.lstrip("/"),
                     "path": f"Plugins/GameFeatures/{mount.lstrip('/')}"})
    return rows


def _format_gamefeatures(rows):
    _hr_section("Game Features", len(rows))
    for r in rows:
        if r.get("parent"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── CATEGORY: INPUT CONFIG (LYRA TAG → IA WIRING) ──────────────────────────

def _scan_input_config():
    """ULyraInputConfig data assets — tag → InputAction maps that bind
    GameplayTag-named abilities to physical inputs. Distinct from raw
    InputAction/IMC scanning under the 'input' category."""
    paths = _lyra_scan_paths()
    rows = []
    for ad in _filter_assets_multi(_LYRA_INPUT_CONFIG_CP, paths):
        rows.append({"kind": "InputConfig", "name": str(ad.asset_name),
                     "path": _asset_full_path(ad)})
    return rows


def _format_input_config(rows):
    _hr_section("Lyra Input Config", len(rows))
    for r in rows:
        _emit_compact(r)


# ─── CATEGORY: CAMERA MODES ─────────────────────────────────────────────────

def _scan_camera():
    """ULyraCameraMode BP children + native subclasses. Camera modes are
    referenced from LyraPawnData and switched at runtime by abilities."""
    paths = _lyra_scan_paths()
    bps = _filter_assets_multi(_BLUEPRINT_CP, paths)
    rows = []
    for ad in bps:
        parent = _bp_parent_class_path(ad)
        if parent and 'LyraCameraMode' in parent:
            rows.append({"kind": "CameraMode-BP", "name": str(ad.asset_name),
                         "path": _asset_full_path(ad),
                         "parent": parent, "hints": []})
    for cp in _native_classes_derived_from(_LYRA_CAMERA_MODE_CP):
        rows.append({"kind": "CameraMode-C++", "name": _short_class_name(cp),
                     "path": cp, "parent": None, "hints": []})
    return rows


def _format_camera(rows):
    _hr_section("Camera Modes", len(rows))
    for r in rows:
        if r.get("parent"):
            _emit_full(r)
        else:
            _emit_compact(r)


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

# (scan_fn, format_fn) per category. Scans return list[dict]; formats log them.
_CATEGORY_FN = {
    "player":       (_scan_player_characters, _format_player_characters),
    "enemy":        (_scan_enemy_npc,         _format_enemy_npc),
    "damage":       (_scan_damage_system,     _format_damage_system),
    "animation":    (_scan_animation,         _format_animation),
    "input":        (_scan_input,             _format_input),
    "gas":          (_scan_gas,               _format_gas),
    "experience":   (_scan_experience,        _format_experience),
    "equipment":    (_scan_equipment,         _format_equipment),
    "inventory":    (_scan_inventory,         _format_inventory),
    "gamefeatures": (_scan_gamefeatures,      _format_gamefeatures),
    "input_config": (_scan_input_config,      _format_input_config),
    "camera":       (_scan_camera,            _format_camera),
}


def dc_inspect_project(topic=None, silent=False, folder=None):
    """Read-only scan of the current UE project.

    topic — None runs the original 5 categories (player/enemy/damage/
        animation/input). 'all' adds the 7 Lyra/GAS categories. 'lyra' runs
        only the Lyra/GAS categories. Single-category aliases:
        'player', 'enemy'/'ai', 'damage'/'combat', 'animation', 'input',
        'gas'/'abilities', 'experience', 'equipment', 'inventory',
        'gamefeatures', 'input_config', 'camera', 'meshes'.

    silent — if False (default), logs a structured report to the Output Log
        and returns None. If True, suppresses logging and returns a dict
        keyed by category name with list[dict] values — for programmatic
        consumption (e.g. Danger Zone deciding what assets to reuse).

    folder — mount-relative path (e.g. '/ShooterCore/Weapons/Shotgun') used
        by registry-driven categories (SCAN_CATEGORIES) to narrow their scan
        to a single package path. Categories with require_folder=True (e.g.
        textures) return an empty list with a log warning when this is None.
        Hand-written categories (player, enemy, gas, etc.) ignore this kwarg.

    Dict shape (silent=True): {category: [{"kind": str, "name": str,
        "path": str, "parent": Optional[str], "hints": list[str]}, ...]}.
        'parent' and 'hints' are present on BP/native rows; absent on
        plain asset rows (BT/BB/Montage/AnimBP/Sequence/IA/IMC/SM/SK).
    """
    if topic is not None and topic not in _TOPICS_TO_CATEGORIES:
        valid = sorted(t for t in _TOPICS_TO_CATEGORIES if t is not None)
        if not silent:
            _log(f"unknown topic '{topic}'. Valid: {', '.join(valid)}")
        return {} if silent else None

    cats = _TOPICS_TO_CATEGORIES[topic]
    result = {}
    for cat in cats:
        try:
            if cat in SCAN_CATEGORIES:
                spec = SCAN_CATEGORIES[cat]
                if spec["require_folder"] and not folder:
                    _log(f"category '{cat}' requires folder= argument; "
                         f"returning empty (would scan globally otherwise)")
                    result[cat] = []
                else:
                    result[cat] = _scan_assets_by_class(spec["classes"],
                                                       folder=folder)
            else:
                scan_fn, _ = _CATEGORY_FN[cat]
                result[cat] = scan_fn()
        except Exception as e:
            _log(f"WARN: category '{cat}' failed — {e}")
            result[cat] = []

    if silent:
        return result

    _hr_double()
    _out("DeltaCode Project Inspector")
    _hr_double()
    for cat in cats:
        if cat in SCAN_CATEGORIES:
            _format_registry_category(SCAN_CATEGORIES[cat]["label"],
                                      result[cat])
        else:
            _, format_fn = _CATEGORY_FN[cat]
            format_fn(result[cat])
    _out("")
    _hr_double()
    total = sum(len(v) for v in result.values())
    label = "category" if len(cats) == 1 else "categories"
    _out(f"Total: {total} items across {len(cats)} {label}.")
    _hr_double()
    return None


def format_scan_for_llm(scan):
    """Convert a silent-mode scan dict into a compact markdown block suitable
    for inclusion in an LLM prompt. One section per category, one bullet per
    row, with kind/path/parent/hints inlined.

    Example:
        ## Player Characters (2)
        - BP_DC_Player [BP, /Game/...] parent: Character (matches GameMode.DefaultPawnClass)
        - ADCCharacterBase [C++, /Script/DeltaCode.DCCharacterBase]
    """
    titles = (
        ("player",       "Player Characters"),
        ("enemy",        "Enemy / NPC"),
        ("damage",       "Damage System"),
        ("animation",    "Animation"),
        ("input",        "Input"),
        ("gas",          "GAS (Abilities / Effects / Attributes / Cues)"),
        ("experience",   "Experience / PawnData"),
        ("equipment",    "Equipment / Weapons"),
        ("inventory",    "Inventory (Items / Fragment Kinds)"),
        ("gamefeatures", "Game Features"),
        ("input_config", "Lyra Input Config"),
        ("camera",       "Camera Modes"),
    )
    # Append registry-driven categories so future SCAN_CATEGORIES entries
    # render in the LLM-formatted scan with no second edit needed.
    titles = titles + tuple((k, v["label"]) for k, v in SCAN_CATEGORIES.items())
    lines = ["# DeltaCode Project Scan", ""]
    for cat, title in titles:
        if cat not in scan:
            continue
        rows = scan[cat]
        lines.append(f"## {title} ({len(rows)})")
        if not rows:
            lines.append("- (none found)")
        else:
            for r in rows:
                kind = r.get("kind", "?")
                name = r.get("name", "?")
                path = r.get("path", "")
                parent = r.get("parent")
                hints = r.get("hints", []) or []
                line = f"- {name} [{kind}, {path}]"
                if parent:
                    line += f" parent: {_short_class_name(parent)}"
                if hints:
                    line += f" ({'; '.join(hints)})"
                lines.append(line)
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def write_scan_for_llm(output_path, topic="all", folder=None):
    """Run the inspector silently, format the result for LLM consumption,
    and write it to output_path. Used by the C++ bridge to capture inspector
    output for inclusion in an Anthropic request body — see
    FDCLevelScriptingBridge::RunInspectorForLLM — and by the Run Inspector
    button to render the same formatted scan in the panel's Response box.

    folder — forwarded to dc_inspect_project for registry-driven categories
    (SCAN_CATEGORIES). Currently only callable from Python directly; C++/UI
    plumbing for folder-targeted scans is a follow-up task."""
    import os
    scan = dc_inspect_project(topic=topic, silent=True, folder=folder)
    text = format_scan_for_llm(scan)
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(text)
    total = sum(len(v) for v in scan.values())
    _log(f"wrote LLM-formatted scan ({len(text)} chars, {total} items, "
         f"topic={topic}, folder={folder}) to {output_path}")


if __name__ == "__main__":
    dc_inspect_project()

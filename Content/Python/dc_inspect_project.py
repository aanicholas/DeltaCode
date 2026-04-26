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
#     dc_inspect_project()              # all categories
#     dc_inspect_project("player")      # one category
#     dc_inspect_project("ai")          # alias for "enemy"
#     dc_inspect_project("combat")      # alias for "damage"
#
# Default scan scope is /Game/ — engine and plugin content excluded.

import unreal

# ─── CONFIG ──────────────────────────────────────────────────────────────────

_DEFAULT_SCAN_PATH = "/Game"

# Class path tuples (module, class_name) for AssetRegistry filters.
_CHARACTER_CP        = ("/Script/Engine",        "Character")
_AI_CONTROLLER_CP    = ("/Script/AIModule",      "AIController")
_BLUEPRINT_CP        = ("/Script/Engine",        "Blueprint")
_BT_CP               = ("/Script/AIModule",      "BehaviorTree")
_BB_CP               = ("/Script/AIModule",      "BlackboardData")
_ANIM_MONTAGE_CP     = ("/Script/Engine",        "AnimMontage")
_ANIM_BP_CP          = ("/Script/Engine",        "AnimBlueprint")
_ANIM_SEQ_CP         = ("/Script/Engine",        "AnimSequence")
_INPUT_ACTION_CP     = ("/Script/EnhancedInput", "InputAction")
_IMC_CP              = ("/Script/EnhancedInput", "InputMappingContext")

_TOPICS_TO_CATEGORIES = {
    None:        ("player", "enemy", "damage", "animation", "input"),
    "all":       ("player", "enemy", "damage", "animation", "input"),
    "player":    ("player",),
    "enemy":     ("enemy",),
    "ai":        ("enemy",),
    "damage":    ("damage",),
    "combat":    ("damage",),
    "animation": ("animation",),
    "input":     ("input",),
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


# ─── REPORT FORMATTING ───────────────────────────────────────────────────────

def _hr_double():
    _out("=" * 50)


def _hr_section(title, count):
    _out("")
    _out(f"--- {title} ({count}) ---")


def _item_bp(prefix, ad, parent=None, hint=None):
    _out(f"  [{prefix}]  {ad.asset_name}")
    _out(f"         Path:   {_asset_full_path(ad)}")
    if parent:
        _out(f"         Parent: {_short_class_name(parent)}")
    if hint:
        _out(f"         Hint:   {hint}")


def _item_native(prefix, class_path):
    _out(f"  [{prefix}]  {_short_class_name(class_path)}")
    _out(f"         Path:   {class_path}")


def _item_asset(prefix, ad, *details):
    line = f"  [{prefix}]  {ad.asset_name}"
    if details:
        line = f"{line} — {' / '.join(details)}"
    _out(line)


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


def _scan_player_characters():
    """List Character-derived BPs and native classes; flag likely player
    candidates (name contains 'Player' OR matches a GameMode's
    DefaultPawnClass)."""
    candidates = _candidate_player_pawn_class_paths()

    bp_chars = []
    for ad in _filter_assets(_BLUEPRINT_CP):
        parent = _bp_parent_class_path(ad)
        if parent and 'Character' in parent:
            bp_chars.append((ad, parent))

    native = _native_classes_derived_from(_CHARACTER_CP)

    rows = []
    for ad, parent in bp_chars:
        hints = []
        name = str(ad.asset_name)
        if 'Player' in name:
            hints.append("name contains 'Player'")
        bp_path = _asset_full_path(ad)
        gen_path = f"{bp_path}.{name}_C"
        if any(c == bp_path or c == gen_path or c.startswith(gen_path)
               for c in candidates):
            hints.append("matches GameMode.DefaultPawnClass")
        hint = "; ".join(hints) if hints else None
        rows.append(("BP", ad, parent, hint))

    for cp in native:
        rows.append(("C++", cp, None, None))

    _hr_section("Player Characters", len(rows))
    for kind, ad_or_path, parent, hint in rows:
        if kind == "BP":
            _item_bp(kind, ad_or_path, parent=parent, hint=hint)
        else:
            _item_native(kind, ad_or_path)
    return len(rows)


# ─── CATEGORY: ENEMY / NPC ───────────────────────────────────────────────────

def _scan_enemy_npc():
    """AIController BPs + native subclasses, plus all BehaviorTrees and
    BlackboardData assets in /Game/."""
    rows = []

    for ad in _filter_assets(_BLUEPRINT_CP):
        parent = _bp_parent_class_path(ad)
        if parent and 'AIController' in parent:
            rows.append(("BP-AI", "bp", ad, parent))

    for cp in _native_classes_derived_from(_AI_CONTROLLER_CP):
        rows.append(("C++-AI", "native", cp, None))

    for ad in _filter_assets(_BT_CP):
        rows.append(("BT", "asset", ad, None))

    for ad in _filter_assets(_BB_CP):
        rows.append(("BB", "asset", ad, None))

    _hr_section("Enemy / NPC", len(rows))
    for prefix, kind, payload, parent in rows:
        if kind == "bp":
            _item_bp(prefix, payload, parent=parent)
        elif kind == "native":
            _item_native(prefix, payload)
        else:
            _item_asset(prefix, payload)
    return len(rows)


# ─── CATEGORY: DAMAGE SYSTEM ─────────────────────────────────────────────────

def _scan_damage_system():
    """Heuristic: BP interfaces or BPs with 'Damage'/'Damageable' in name;
    BPs with 'Health' in name (often health components or health BPs)."""
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
            prefix = "BPI"
        elif parent and 'Component' in parent:
            prefix = "Cmp"
        else:
            prefix = "BP"
        rows.append((prefix, ad, parent))

    _hr_section("Damage System", len(rows))
    for prefix, ad, parent in rows:
        _item_bp(prefix, ad, parent=parent)
    return len(rows)


# ─── CATEGORY: ANIMATION ─────────────────────────────────────────────────────

def _scan_animation():
    """AnimMontages, AnimBlueprints, AnimSequences in /Game/."""
    montages  = _filter_assets(_ANIM_MONTAGE_CP)
    anim_bps  = _filter_assets(_ANIM_BP_CP)
    sequences = _filter_assets(_ANIM_SEQ_CP)
    total = len(montages) + len(anim_bps) + len(sequences)

    _hr_section("Animation", total)
    for ad in montages:
        _item_asset("Montage", ad)
    for ad in anim_bps:
        _item_asset("AnimBP", ad)
    for ad in sequences:
        _item_asset("Sequence", ad)
    return total


# ─── CATEGORY: INPUT ─────────────────────────────────────────────────────────

def _scan_input():
    """Enhanced Input — InputActions and InputMappingContexts."""
    actions  = _filter_assets(_INPUT_ACTION_CP)
    contexts = _filter_assets(_IMC_CP)
    total = len(actions) + len(contexts)

    _hr_section("Input", total)
    for ad in actions:
        _item_asset("IA", ad)
    for ad in contexts:
        _item_asset("IMC", ad)
    return total


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

_CATEGORY_FN = {
    "player":    _scan_player_characters,
    "enemy":     _scan_enemy_npc,
    "damage":    _scan_damage_system,
    "animation": _scan_animation,
    "input":     _scan_input,
}


def dc_inspect_project(topic=None):
    """Read-only scan of the current UE project. Logs a structured report
    to the Output Log.

    topic — None or 'all' runs every category. Single-category aliases:
        'player', 'enemy'/'ai', 'damage'/'combat', 'animation', 'input'.
    """
    if topic is not None and topic not in _TOPICS_TO_CATEGORIES:
        valid = sorted(t for t in _TOPICS_TO_CATEGORIES if t is not None)
        _log(f"unknown topic '{topic}'. Valid: {', '.join(valid)}")
        return

    cats = _TOPICS_TO_CATEGORIES[topic]
    _hr_double()
    _out("DeltaCode Project Inspector")
    _hr_double()

    grand_total = 0
    for cat in cats:
        fn = _CATEGORY_FN.get(cat)
        if fn is None:
            continue
        try:
            grand_total += fn()
        except Exception as e:
            _log(f"WARN: category '{cat}' failed — {e}")

    _out("")
    _hr_double()
    label = "category" if len(cats) == 1 else "categories"
    _out(f"Total: {grand_total} items across {len(cats)} {label}.")
    _hr_double()


if __name__ == "__main__":
    dc_inspect_project()

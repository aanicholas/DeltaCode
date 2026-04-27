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


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

# (scan_fn, format_fn) per category. Scans return list[dict]; formats log them.
_CATEGORY_FN = {
    "player":    (_scan_player_characters, _format_player_characters),
    "enemy":     (_scan_enemy_npc,         _format_enemy_npc),
    "damage":    (_scan_damage_system,     _format_damage_system),
    "animation": (_scan_animation,         _format_animation),
    "input":     (_scan_input,             _format_input),
}


def dc_inspect_project(topic=None, silent=False):
    """Read-only scan of the current UE project.

    topic — None or 'all' runs every category. Single-category aliases:
        'player', 'enemy'/'ai', 'damage'/'combat', 'animation', 'input'.

    silent — if False (default), logs a structured report to the Output Log
        and returns None. If True, suppresses logging and returns a dict
        keyed by category name with list[dict] values — for programmatic
        consumption (e.g. Danger Zone deciding what assets to reuse).

    Dict shape (silent=True): {category: [{"kind": str, "name": str,
        "path": str, "parent": Optional[str], "hints": list[str]}, ...]}.
        'parent' and 'hints' are present on BP/native rows; absent on
        plain asset rows (BT/BB/Montage/AnimBP/Sequence/IA/IMC).
    """
    if topic is not None and topic not in _TOPICS_TO_CATEGORIES:
        valid = sorted(t for t in _TOPICS_TO_CATEGORIES if t is not None)
        if not silent:
            _log(f"unknown topic '{topic}'. Valid: {', '.join(valid)}")
        return {} if silent else None

    cats = _TOPICS_TO_CATEGORIES[topic]
    result = {}
    for cat in cats:
        scan_fn, _ = _CATEGORY_FN[cat]
        try:
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
        ("player",    "Player Characters"),
        ("enemy",     "Enemy / NPC"),
        ("damage",    "Damage System"),
        ("animation", "Animation"),
        ("input",     "Input"),
    )
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


def write_scan_for_llm(output_path, topic="all"):
    """Run the inspector silently, format the result for LLM consumption,
    and write it to output_path. Used by the C++ bridge to capture inspector
    output for inclusion in an Anthropic request body — see
    FDCLevelScriptingBridge::RunInspectorForLLM — and by the Run Inspector
    button to render the same formatted scan in the panel's Response box."""
    import os
    scan = dc_inspect_project(topic=topic, silent=True)
    text = format_scan_for_llm(scan)
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(text)
    total = sum(len(v) for v in scan.values())
    _log(f"wrote LLM-formatted scan ({len(text)} chars, {total} items, topic={topic}) to {output_path}")


if __name__ == "__main__":
    dc_inspect_project()

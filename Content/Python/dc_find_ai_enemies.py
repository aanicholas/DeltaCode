# DeltaCode - Unreal Engine Code Helper
# Copyright (c) 2026 Andrew Nicholas
#
# This program is free software: you can redistribute
# it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software
# Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dc_find_ai_enemies.py — diagnostic. Enumerates every Blueprint in /Game/
# whose generated class is a Pawn/Character with an AIController assigned,
# plus every BehaviorTree and BlackboardData asset. Used to find pre-built
# AI-capable enemies we can spawn directly instead of building our own.
#
# Usage (UE Python console):
#     import dc_find_ai_enemies; dc_find_ai_enemies.report()
#
# UE5.7 AssetData notes:
#   - object_path  → removed. Use package_name + asset_name.
#   - get_assets_by_class takes a TopLevelAssetPath, not a raw string.

import unreal

# ─── HELPERS ─────────────────────────────────────────────────────────────────

def _full_path(asset_data):
    return f"{asset_data.package_name}.{asset_data.asset_name}"


def _tag(asset_data, tag_name):
    try:
        v = asset_data.get_tag_value(tag_name)
        return str(v) if v else ""
    except Exception:
        return ""


def _class_name(obj):
    if obj is None:
        return "None"
    try:
        return obj.get_name()
    except Exception:
        return str(obj)


def _candidate_tags():
    """Native parent substrings we want to inspect — anything pawn-shaped."""
    return ("Character", "Pawn", "DCEnemyBase", "DCCharacterBase")


# ─── AI-CAPABLE PAWN BLUEPRINTS ──────────────────────────────────────────────

def _scan_pawn_blueprints():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    bp_class = unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")
    bps = registry.get_assets_by_class(bp_class)
    unreal.log(f"[AI-SCAN] Total Blueprints in registry: {len(bps)}")

    candidates = []
    inspected = 0
    for bp_data in bps:
        # Cheap pre-filter via AssetData tags — avoids loading every BP.
        parent_native = _tag(bp_data, "NativeParentClass")
        parent_blue   = _tag(bp_data, "ParentClass")
        parent_tag    = parent_native or parent_blue
        if not any(k in parent_tag for k in _candidate_tags()):
            continue

        # Skip the plugin's own Core enemies — user wants pre-built alternatives.
        if "/DeltaCode/Core/" in str(bp_data.package_name):
            continue

        path = _full_path(bp_data)
        try:
            bp = unreal.load_asset(str(bp_data.package_name))
            if bp is None:
                continue
            gc = bp.generated_class()
            if gc is None or not gc.is_child_of(unreal.Pawn):
                continue
            cdo = unreal.get_default_object(gc)
            inspected += 1

            ai_class      = cdo.get_editor_property("ai_controller_class")
            auto_possess  = cdo.get_editor_property("auto_possess_ai")

            # Enemies derived from ADCEnemyBase expose a BehaviorTree default —
            # stock pawns do not. get_editor_property on a missing property
            # raises; swallow so we can still report the rest.
            bt = None
            try:
                bt = cdo.get_editor_property("behavior_tree")
            except Exception:
                pass

            candidates.append({
                "path":     path,
                "parent":   parent_tag,
                "ai_class": _class_name(ai_class),
                "auto":     str(auto_possess),
                "bt":       _class_name(bt) if bt else "None",
            })
        except Exception as e:
            unreal.log_warning(f"[AI-SCAN] could not inspect {path} — {e}")

    unreal.log(f"[AI-SCAN] Inspected {inspected} pawn Blueprints.")
    unreal.log(f"[AI-SCAN] ───── AI-CAPABLE PAWN BLUEPRINTS ─────")
    if not candidates:
        unreal.log("[AI-SCAN]   (none found outside /Game/DeltaCode/Core/)")
    for c in candidates:
        unreal.log(f"[AI-SCAN]   {c['path']}")
        unreal.log(f"[AI-SCAN]     parent       : {c['parent']}")
        unreal.log(f"[AI-SCAN]     ai_controller: {c['ai_class']}")
        unreal.log(f"[AI-SCAN]     auto_possess : {c['auto']}")
        unreal.log(f"[AI-SCAN]     behavior_tree: {c['bt']}")
    return candidates


# ─── BEHAVIOR TREES + BLACKBOARDS ────────────────────────────────────────────

def _scan_ai_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()

    bt_class = unreal.TopLevelAssetPath("/Script/AIModule", "BehaviorTree")
    bts = registry.get_assets_by_class(bt_class)
    unreal.log(f"[AI-SCAN] ───── BehaviorTree assets ({len(bts)}) ─────")
    for a in bts:
        unreal.log(f"[AI-SCAN]   {_full_path(a)}")

    bb_class = unreal.TopLevelAssetPath("/Script/AIModule", "BlackboardData")
    bbs = registry.get_assets_by_class(bb_class)
    unreal.log(f"[AI-SCAN] ───── BlackboardData assets ({len(bbs)}) ─────")
    for a in bbs:
        unreal.log(f"[AI-SCAN]   {_full_path(a)}")


# ─── VARIANT CONTENT HEURISTIC ───────────────────────────────────────────────

def _scan_variant_paths():
    """UE5.5+ Third Person / Combat / Platforming variants sometimes ship with
    AI-driven demo dummies. Probe a few known paths so they surface even if
    their native parent isn't tagged as Character."""
    probes = [
        "/Game/Variant_Combat/NPC",
        "/Game/Variant_Combat/Enemies",
        "/Game/Variant_Combat/AI",
        "/Game/Variant_Combat/Blueprints",
        "/Game/Variant_Platforming/NPC",
        "/Game/Variant_Platforming/Enemies",
        "/Game/Variant_SideScrolling/NPC",
        "/Game/Variant_SideScrolling/Enemies",
        "/Game/ShooterGame",
        "/Game/FirstPerson/Blueprints",
    ]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    unreal.log(f"[AI-SCAN] ───── Variant / template probe paths ─────")
    for p in probes:
        assets = registry.get_assets_by_path(p, recursive=True)
        if not assets:
            unreal.log(f"[AI-SCAN]   {p} — empty / missing")
            continue
        unreal.log(f"[AI-SCAN]   {p} — {len(assets)} asset(s):")
        for a in assets[:20]:
            unreal.log(f"[AI-SCAN]     {a.asset_class_path.asset_name} : "
                       f"{_full_path(a)}")


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

def report():
    unreal.log("═" * 64)
    unreal.log("DeltaCode AI enemy survey")
    unreal.log("═" * 64)
    _scan_pawn_blueprints()
    _scan_ai_assets()
    _scan_variant_paths()
    unreal.log("═" * 64)
    unreal.log("DeltaCode AI enemy survey — done")
    unreal.log("═" * 64)


if __name__ == "__main__":
    report()

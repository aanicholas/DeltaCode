# DeltaCode - Unreal Engine Code Helper
# Copyright (c) 2026 Andrew Nicholas
#
# This program is free software: you can redistribute
# it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software
# Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dc_create_ai_assets.py — bootstrap the BehaviorTree + Blackboard assets
# that ADCEnemyAIController needs at possession time, and wire them into
# B_DC_EnemyBase.
#
# The C++ AI stack (ADCEnemyAIController + the 5 BT tasks under Source/AI/
# + the DCAIBlackboardKeys namespace) ships with the plugin. What's missing
# on a fresh project is the BT/BB uasset files those tasks run against.
# This script fills that gap: idempotent, safe to re-run on every Danger
# Zone build. Existing assets are detected and reused.
#
# Verbose logging at every step so the Output Log shows exactly which part
# succeeded or skipped.

import unreal

# ─── CONSTANTS ───────────────────────────────────────────────────────────────

_AI_PACKAGE_PATH = "/Game/DeltaCode/AI"
_BB_NAME         = "BB_DC_Enemy_Default"
_BT_NAME         = "BT_DC_Enemy"
_BP_PATH         = "/Game/DeltaCode/Core/B_DC_EnemyBase"

_BB_FULL_PATH = f"{_AI_PACKAGE_PATH}/{_BB_NAME}"
_BT_FULL_PATH = f"{_AI_PACKAGE_PATH}/{_BT_NAME}"

# Pre-authored BT template shipped with the plugin. Duplicated on demand
# into /Game/DeltaCode/AI/ so every project gets a working Selector-root
# + WanderFromHome task graph without per-project manual wiring that Python
# can't automate (the BT editor regenerates runtime nodes from the UEdGraph
# on save, which is not Python-authorable).
_BT_TEMPLATE_PATH = "/DeltaCode/DeltaCode/AI/BT_DC_Enemy_Template"

# ─── LOGGING HELPERS ─────────────────────────────────────────────────────────

def _log(msg):  unreal.log(f"DeltaCode[AI]: {msg}")
def _warn(msg): unreal.log_warning(f"DeltaCode[AI]: {msg}")
def _err(msg):  unreal.log_error(f"DeltaCode[AI]: {msg}")


# ─── BLACKBOARD ──────────────────────────────────────────────────────────────

def _make_bb_entry(owner_asset, entry_name, key_type_class, base_class=None):
    """Construct an FBlackboardEntry with a freshly instanced key-type subobject.

    The key-type subobject is outered to the BlackboardData asset so it
    serialises alongside the asset and survives reloads.
    """
    entry = unreal.BlackboardEntry()
    entry.set_editor_property("entry_name", entry_name)
    kt = unreal.new_object(key_type_class, owner_asset)
    if base_class is not None:
        kt.set_editor_property("base_class", base_class)
    entry.set_editor_property("key_type", kt)
    return entry


def _create_blackboard():
    """Create BB_DC_Enemy_Default with the 4 keys DCEnemyAIController expects."""
    if unreal.EditorAssetLibrary.does_asset_exist(_BB_FULL_PATH):
        _log(f"Blackboard already exists: {_BB_FULL_PATH}")
        return unreal.load_asset(_BB_FULL_PATH)

    _log(f"Creating blackboard {_BB_FULL_PATH} ...")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bb = asset_tools.create_asset(
        _BB_NAME, _AI_PACKAGE_PATH, unreal.BlackboardData, None)
    if bb is None:
        _err(f"asset_tools.create_asset returned None for {_BB_NAME}")
        return None

    try:
        entries = [
            _make_bb_entry(bb, "TargetActor",
                           unreal.BlackboardKeyType_Object, unreal.Actor),
            _make_bb_entry(bb, "LastKnownLocation",
                           unreal.BlackboardKeyType_Vector),
            _make_bb_entry(bb, "HomeLocation",
                           unreal.BlackboardKeyType_Vector),
            _make_bb_entry(bb, "bIsInvestigating",
                           unreal.BlackboardKeyType_Bool),
        ]
        bb.set_editor_property("keys", entries)
        _log("Keys set: TargetActor (Object:Actor), LastKnownLocation (Vector), "
             "HomeLocation (Vector), bIsInvestigating (Bool)")
    except Exception as e:
        _warn(f"Failed to populate blackboard keys — {e}. "
              f"Open {_BB_NAME} and add keys manually.")

    unreal.EditorAssetLibrary.save_asset(_BB_FULL_PATH)
    _log(f"Saved {_BB_FULL_PATH}")
    return bb


# ─── BEHAVIOR TREE ───────────────────────────────────────────────────────────

def _create_behavior_tree(bb_asset):
    """Duplicate the plugin's pre-authored BT template into /Game/DeltaCode/AI/
    and wire its BlackboardAsset.

    We ship a pre-authored template because Python can't reliably write to a
    BehaviorTree's UEdGraph — the BT editor's compile step regenerates runtime
    nodes from the editor graph on save, wiping any new_object()-created
    RootNode/children set via set_editor_property. Duplication preserves the
    editor graph (Selector root + WanderFromHome task) baked into the template,
    and set_editor_property('blackboard_asset', ...) survives the compile step
    because it's a simple UPROPERTY on the BT asset.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(_BT_FULL_PATH):
        _log(f"BehaviorTree already exists: {_BT_FULL_PATH}")
        return unreal.load_asset(_BT_FULL_PATH)

    if not unreal.EditorAssetLibrary.does_asset_exist(_BT_TEMPLATE_PATH):
        _err(f"Template BT not found at {_BT_TEMPLATE_PATH} — plugin may be "
             f"misinstalled or the template asset was deleted. Restore it "
             f"from source control, then re-run Build Mission.")
        return None

    _log(f"Duplicating {_BT_TEMPLATE_PATH} → {_BT_FULL_PATH}")
    bt = unreal.EditorAssetLibrary.duplicate_asset(
        _BT_TEMPLATE_PATH, _BT_FULL_PATH)
    if bt is None:
        _err(f"duplicate_asset returned None for {_BT_TEMPLATE_PATH}")
        return None

    if bb_asset is not None:
        try:
            bt.set_editor_property("blackboard_asset", bb_asset)
            _log(f"BlackboardAsset → {bb_asset.get_name()}")
        except Exception as e:
            _warn(f"Failed to assign BlackboardAsset — {e}")
    else:
        _warn("Blackboard asset is None — BT will not run at possession time.")

    unreal.EditorAssetLibrary.save_asset(_BT_FULL_PATH)
    _log(f"Saved {_BT_FULL_PATH}")
    return bt


# ─── BLUEPRINT WIRING ────────────────────────────────────────────────────────

def _wire_enemy_blueprint(bt_asset):
    """Set B_DC_EnemyBase.BehaviorTree default so possession picks up the tree."""
    if bt_asset is None:
        _warn("No BT asset available — skipping B_DC_EnemyBase wiring.")
        return

    if not unreal.EditorAssetLibrary.does_asset_exist(_BP_PATH):
        _warn(f"{_BP_PATH} does not exist yet. "
              f"Run create_core_blueprints() first; wiring will pick up on the "
              f"next Danger Zone run.")
        return

    bp = unreal.load_asset(_BP_PATH)
    if bp is None:
        _err(f"load_asset returned None for {_BP_PATH}")
        return

    try:
        gen_class = bp.generated_class()
        cdo = unreal.get_default_object(gen_class)

        current = None
        try:
            current = cdo.get_editor_property("behavior_tree")
        except Exception:
            pass
        if current == bt_asset:
            _log(f"{_BP_PATH}.BehaviorTree already → {_BT_NAME}, no-op.")
            return

        cdo.set_editor_property("behavior_tree", bt_asset)
        _log(f"Set {_BP_PATH}.BehaviorTree → {_BT_NAME}")

        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            _log(f"Recompiled {_BP_PATH}")
        except Exception as e:
            _warn(f"compile_blueprint failed on {_BP_PATH} — {e}")

        unreal.EditorAssetLibrary.save_asset(_BP_PATH)
        _log(f"Saved {_BP_PATH}")
    except Exception as e:
        _err(f"Failed to set BehaviorTree default on {_BP_PATH} — {e}")


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

def dc_create_ai_assets():
    """Ensure BT_DC_Enemy + BB_DC_Enemy_Default exist, wired into B_DC_EnemyBase.

    Called from run_danger_zone() on every Build Mission. Idempotent — skips
    assets that already exist and re-wires the BP only if its current
    BehaviorTree default differs.
    """
    _log("=" * 60)
    _log("Bootstrapping enemy AI assets")
    bb = _create_blackboard()
    bt = _create_behavior_tree(bb)
    _wire_enemy_blueprint(bt)
    _log("Bootstrap complete")
    _log("=" * 60)


if __name__ == "__main__":
    dc_create_ai_assets()

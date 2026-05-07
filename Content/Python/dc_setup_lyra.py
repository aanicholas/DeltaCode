# DeltaCode - Unreal Engine Code Helper
# Copyright (c) 2026 Andrew Nicholas
#
# This program is free software: you can redistribute
# it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software
# Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dc_setup_lyra.py — one-click DeltaCode → Lyra integration setup.
#
# Idempotent: safe to re-run after a Lyra update, after a DeltaCode plugin
# upgrade, or after deleting a generated asset. Each step detects existing
# assets and reuses them.
#
# What this script creates:
#   1. /Game/DeltaCode/Damage/GE_DC_Kill — duplicate of Lyra's
#      GE_Damage_Basic_SetByCaller with the Damage.Lethal tag added
#      to its granted-tags container.
#   2. /Game/DeltaCode/Damage/DA_DC_DamageTiers — UDCDamageTierTable data
#      asset wired to point DefaultDamageEffectClass at Lyra's
#      GE_Damage_Basic_SetByCaller and LethalDamageEffectClass at GE_DC_Kill.
#   3. AI assets (BT_DC_Enemy + BB_DC_Enemy_Default + B_DC_EnemyBase
#      wiring) by chaining dc_create_ai_assets.dc_create_ai_assets().
#
# What this script verifies but never modifies:
#   - Lyra prerequisite assets (GE_Damage_Basic_SetByCaller etc.)
#   - Optional setup gaps (Anthropic API key, DA_DC_PawnData, B_DC_EnemyBase)
#
# Entry point:
#     dc_setup_lyra()    # full pipeline
#
# Designed to be invoked via FDCLevelScriptingBridge::ExecuteLyraSetup
# from the "Setup Lyra Integration" panel button.

import unreal

# ─── CONSTANTS ───────────────────────────────────────────────────────────────

_DC_DAMAGE_PATH        = "/Game/DeltaCode/Damage"
_GE_KILL_NAME          = "GE_DC_Kill"
_DA_TIERS_NAME         = "DA_DC_DamageTiers"

_GE_KILL_FULL_PATH     = f"{_DC_DAMAGE_PATH}/{_GE_KILL_NAME}"
_DA_TIERS_FULL_PATH    = f"{_DC_DAMAGE_PATH}/{_DA_TIERS_NAME}"

# Lyra's SetByCaller damage GE — duplicated to produce GE_DC_Kill.
_LYRA_SETBYCALLER_GE   = "/Game/GameplayEffects/Damage/GE_Damage_Basic_SetByCaller"

# Read-only sanity checks. If the project has these assets, it's almost
# certainly a Lyra-based project and the duplication step will succeed.
_LYRA_PREREQS = [
    "/Game/GameplayEffects/Damage/GameplayEffectParent_Damage_Basic",
    "/Game/GameplayEffects/Damage/GE_Damage_Basic_Instant",
    _LYRA_SETBYCALLER_GE,
    "/Game/GameplayCueNotifies/GCNL_Character_DamageTaken",
]

# Tag added to GE_DC_Kill's granted-tags container. Must match the native
# tag declared in Public/Types/DCGameplayTags.h. If the C++ module hasn't
# been rebuilt since the tag was added, the request resolves to the empty
# tag and the mutation is skipped — see _add_lethal_tag_to_kill_ge.
_DAMAGE_LETHAL_TAG_NAME = "Damage.Lethal"

# Optional checks reported but not auto-fixed.
_OPTIONAL_PAWN_DATA_PATH    = "/Game/DeltaCode/Core/DA_DC_PawnData"
_OPTIONAL_ENEMY_BP_PATH     = "/Game/DeltaCode/Core/B_DC_EnemyBase"

# ─── LOGGING HELPERS ─────────────────────────────────────────────────────────

def _log(msg):  unreal.log(f"DeltaCode[Setup]: {msg}")
def _warn(msg): unreal.log_warning(f"DeltaCode[Setup]: {msg}")
def _err(msg):  unreal.log_error(f"DeltaCode[Setup]: {msg}")


# ─── PREREQUISITE VERIFICATION (READ-ONLY) ───────────────────────────────────

def _verify_lyra_prerequisites():
    """Read-only check that the four canonical Lyra damage assets exist.

    Returns (ok: bool, missing: list[str]). ok=False does NOT abort the
    script — partial setup is still useful. The caller surfaces the
    missing list to the user.
    """
    missing = [p for p in _LYRA_PREREQS
               if not unreal.EditorAssetLibrary.does_asset_exist(p)]
    if missing:
        _warn("Lyra prerequisites not found at these paths:")
        for p in missing:
            _warn(f"  - {p}")
        _warn("If this is not a Lyra project, dc_setup_lyra is not "
              "appropriate — Lyra-specific assets will be skipped.")
    else:
        _log("Lyra prerequisites OK.")
    return (len(missing) == 0, missing)


# ─── GE_DC_KILL — DUPLICATE LYRA'S SETBYCALLER GE + ADD LETHAL TAG ───────────

def _add_lethal_tag_to_kill_ge(ge_bp):
    """Add Damage.Lethal to the duplicated GE's GrantedTags container.

    Operates on the Blueprint CDO's InheritableOwnedTagsContainer property.
    If the Damage.Lethal native tag isn't registered yet (e.g. the C++
    module hasn't been rebuilt since DCGameplayTags was extended), the
    request returns an empty tag and we log a clear "rebuild and re-run"
    note rather than silently doing nothing.
    """
    if ge_bp is None:
        return

    try:
        manager = unreal.GameplayTagsManager.get()
        lethal_tag = manager.request_gameplay_tag(
            unreal.Name(_DAMAGE_LETHAL_TAG_NAME))
    except Exception as e:
        _warn(f"Could not look up GameplayTagsManager — {e}. "
              f"Open GE_DC_Kill manually and add {_DAMAGE_LETHAL_TAG_NAME} "
              f"to its Granted Tags.")
        return

    if not lethal_tag.is_valid():
        _warn(f"Tag '{_DAMAGE_LETHAL_TAG_NAME}' is not registered. "
              f"Rebuild the DeltaCode C++ module (it declares the native "
              f"tag) and re-run Setup Lyra Integration. GE_DC_Kill was "
              f"created without the granted tag.")
        return

    try:
        gen_class = ge_bp.generated_class()
        cdo = unreal.get_default_object(gen_class)

        # FInheritedTagContainer field — name varies subtly across UE
        # versions; try the canonical UE5.5 property name first.
        inh_prop_name = "inheritable_owned_tags_container"
        inherited = cdo.get_editor_property(inh_prop_name)

        # `Added` is the FGameplayTagContainer of locally-added tags.
        added = inherited.get_editor_property("added")
        # Idempotency — skip if already present.
        if added.has_tag_exact(lethal_tag):
            _log(f"GE_DC_Kill already grants {_DAMAGE_LETHAL_TAG_NAME}, "
                 f"no-op.")
            return

        added.add_tag(lethal_tag)
        inherited.set_editor_property("added", added)
        cdo.set_editor_property(inh_prop_name, inherited)

        unreal.BlueprintEditorLibrary.compile_blueprint(ge_bp)
        unreal.EditorAssetLibrary.save_asset(_GE_KILL_FULL_PATH)
        _log(f"Granted {_DAMAGE_LETHAL_TAG_NAME} added to GE_DC_Kill, "
             f"recompiled, saved.")
    except Exception as e:
        _warn(f"Could not set granted tag on GE_DC_Kill — {e}. "
              f"Open the asset and add {_DAMAGE_LETHAL_TAG_NAME} to its "
              f"InheritableOwnedTagsContainer.Added container manually.")


def _create_kill_ge():
    """Duplicate Lyra's GE_Damage_Basic_SetByCaller into the DeltaCode
    namespace as GE_DC_Kill, then mutate the duplicate's granted tags
    to include Damage.Lethal.

    Idempotent — returns the existing asset on second run.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(_GE_KILL_FULL_PATH):
        _log(f"GE_DC_Kill already exists: {_GE_KILL_FULL_PATH}")
        return unreal.load_asset(_GE_KILL_FULL_PATH)

    if not unreal.EditorAssetLibrary.does_asset_exist(_LYRA_SETBYCALLER_GE):
        _err(f"Source GE not found: {_LYRA_SETBYCALLER_GE}. Cannot create "
             f"GE_DC_Kill without Lyra's SetByCaller damage GE. Skipping.")
        return None

    _log(f"Duplicating {_LYRA_SETBYCALLER_GE} → {_GE_KILL_FULL_PATH}")
    ge = unreal.EditorAssetLibrary.duplicate_asset(
        _LYRA_SETBYCALLER_GE, _GE_KILL_FULL_PATH)
    if ge is None:
        _err(f"duplicate_asset returned None for {_LYRA_SETBYCALLER_GE}")
        return None

    # Save first so the duplicated asset settles on disk before tag mutation.
    unreal.EditorAssetLibrary.save_asset(_GE_KILL_FULL_PATH)
    _add_lethal_tag_to_kill_ge(ge)
    return ge


# ─── DA_DC_DAMAGETIERS — TYPED UDCDamageTierTable INSTANCE ───────────────────

def _soft_class_path(asset_path, class_suffix="_C"):
    """/Game/Foo/Bar → SoftClassPath('/Game/Foo/Bar.Bar_C')."""
    short_name = asset_path.rsplit("/", 1)[-1]
    full_obj_path = f"{asset_path}.{short_name}{class_suffix}"
    return unreal.SoftClassPath(full_obj_path)


def _create_damage_tiers_asset(kill_ge):
    """Create /Game/DeltaCode/Damage/DA_DC_DamageTiers — a UDCDamageTierTable
    instance with DefaultDamageEffectClass = Lyra's SetByCaller GE and
    LethalDamageEffectClass = GE_DC_Kill.

    Requires the UDCDamageTierTable C++ class — defined in
    Source/DeltaCode/Public/Data/DCDamageTierTable.h. If the class isn't
    available (plugin not loaded), the script logs an error and skips.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(_DA_TIERS_FULL_PATH):
        _log(f"DA_DC_DamageTiers already exists: {_DA_TIERS_FULL_PATH}")
        return unreal.load_asset(_DA_TIERS_FULL_PATH)

    table_class = getattr(unreal, "DCDamageTierTable", None)
    if table_class is None:
        _err("UDCDamageTierTable C++ class not found. Rebuild the "
             "DeltaCode plugin (Public/Data/DCDamageTierTable.h must "
             "compile) and re-run. Skipping DA_DC_DamageTiers creation.")
        return None

    _log(f"Creating {_DA_TIERS_FULL_PATH} ...")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    da = asset_tools.create_asset(
        _DA_TIERS_NAME, _DC_DAMAGE_PATH, table_class, None)
    if da is None:
        _err(f"asset_tools.create_asset returned None for {_DA_TIERS_NAME}")
        return None

    # FDCDamageConfig's defaults seed the tier magnitudes and the
    # Data.Damage SetByCaller tag — we only need to wire the GE refs.
    try:
        cfg = da.get_editor_property("damage_config")

        cfg.set_editor_property("default_damage_effect_class",
                                _soft_class_path(_LYRA_SETBYCALLER_GE))
        if kill_ge is not None:
            cfg.set_editor_property("lethal_damage_effect_class",
                                    _soft_class_path(_GE_KILL_FULL_PATH))
        else:
            _warn("GE_DC_Kill missing — DA_DC_DamageTiers.LethalDamageEffectClass "
                  "left unset. Re-run after GE_DC_Kill is created to wire it.")

        da.set_editor_property("damage_config", cfg)

        unreal.EditorAssetLibrary.save_asset(_DA_TIERS_FULL_PATH)
        _log(f"DA_DC_DamageTiers configured + saved.")
    except Exception as e:
        _warn(f"Could not populate DA_DC_DamageTiers — {e}. Open the asset "
              f"and set DamageConfig.DefaultDamageEffectClass and "
              f"LethalDamageEffectClass manually.")

    return da


# ─── CHAIN AI ASSETS ─────────────────────────────────────────────────────────

def _chain_ai_assets():
    """Run dc_create_ai_assets.dc_create_ai_assets() — idempotent BT/BB/BP
    setup. Wrapped in try/except so an AI failure doesn't abort the
    overall setup report."""
    try:
        import importlib
        import dc_create_ai_assets
        importlib.reload(dc_create_ai_assets)
        dc_create_ai_assets.dc_create_ai_assets()
    except Exception as e:
        _warn(f"AI asset bootstrap failed — {e}. Run "
              f"dc_create_ai_assets.dc_create_ai_assets() manually from "
              f"the Python console once the underlying issue is resolved.")


# ─── OPTIONAL SETUP REPORT (READ-ONLY) ───────────────────────────────────────

def _check_anthropic_key_configured():
    """True if UDeltaCodeSettings has an Anthropic key set. Editor-only."""
    settings_class = getattr(unreal, "DeltaCodeSettings", None)
    if settings_class is None:
        return False
    try:
        cdo = unreal.get_default_object(settings_class)
        key = cdo.get_editor_property("anthropic_api_key")
        return bool(key)
    except Exception:
        return False


def _report_optional_setup_gaps():
    """Read-only: log what else the user may want to do but the script
    does not auto-apply. Project-specific or sensitive items only."""
    _log("Optional setup gaps (not auto-applied):")

    if _check_anthropic_key_configured():
        _log("  ✓ Anthropic API key is configured.")
    else:
        _log("  ✗ Anthropic API key NOT set. "
             "Project Settings → Plugins → DeltaCode.")

    if unreal.EditorAssetLibrary.does_asset_exist(_OPTIONAL_PAWN_DATA_PATH):
        _log(f"  ✓ {_OPTIONAL_PAWN_DATA_PATH} exists.")
    else:
        _log(f"  ✗ {_OPTIONAL_PAWN_DATA_PATH} not found. Create a "
             f"ULyraPawnData referencing the DeltaCode pawn class to "
             f"integrate with a Lyra Experience.")

    if unreal.EditorAssetLibrary.does_asset_exist(_OPTIONAL_ENEMY_BP_PATH):
        _log(f"  ✓ {_OPTIONAL_ENEMY_BP_PATH} exists.")
    else:
        _log(f"  ✗ {_OPTIONAL_ENEMY_BP_PATH} not found. "
             f"Run dc_create_ai_assets first or invoke Build Mission "
             f"once to scaffold core Blueprints.")


# ─── ENTRY POINT ─────────────────────────────────────────────────────────────

def dc_setup_lyra():
    """One-click DeltaCode setup for Lyra projects. Idempotent.

    On non-Lyra projects (Lyra prereqs missing), the damage section is
    skipped entirely — without the gate we'd create DA_DC_DamageTiers
    wired to a soft class path pointing at a non-existent GE. AI
    bootstrap and the optional gap report still run because both work
    on any project.
    """
    _log("=" * 60)
    _log("DeltaCode → Lyra integration setup starting")
    _log("=" * 60)

    lyra_ok, _missing = _verify_lyra_prerequisites()

    if lyra_ok:
        kill_ge = _create_kill_ge()
        _create_damage_tiers_asset(kill_ge)
    else:
        _warn("Skipping damage asset creation (GE_DC_Kill, "
              "DA_DC_DamageTiers) — Lyra prerequisites unavailable. "
              "AI bootstrap and optional gap report will still run.")

    _chain_ai_assets()
    _report_optional_setup_gaps()

    _log("=" * 60)
    _log("Setup complete")
    _log("=" * 60)


if __name__ == "__main__":
    dc_setup_lyra()

#!/bin/bash
# Sync DeltaCode plugin sources from this repo (master) to every UE project
# that hosts the plugin. Mirrors Python (file-by-file) AND C++ source
# (rsync --delete) so deletions in canonical propagate — prevents stale
# header drift like the Interfaces/DCDamageable.h conflict that surfaced
# after the tier-based damage migration.
#
# Add paths to PLUGIN_DESTINATIONS when new projects are set up.
set -e

SOURCE_REPO="$HOME/Documents/DeltaCode"

# Each destination is a Plugins/DeltaCode/ root inside a UE project.
# The script syncs Content/Python/ and Source/ subtrees underneath.
PLUGIN_DESTINATIONS=(
    "$HOME/Documents/Unreal Projects/DeltaCodeOpus/Plugins/DeltaCode"
    "$HOME/Documents/Unreal Projects/DC_Prototype/Plugins/DeltaCode"
    "$HOME/Documents/Unreal Projects/DeltaCodeLyra/Plugins/DeltaCode"
)

for DEST in "${PLUGIN_DESTINATIONS[@]}"; do
    # Python — copy the canonical scripts. Per-file copy lets project-
    # local helpers (if any) coexist alongside the canonical set.
    PY_DEST="$DEST/Content/Python"
    cp "$SOURCE_REPO/Content/Python/dc_danger_zone.py"     "$PY_DEST/dc_danger_zone.py"
    cp "$SOURCE_REPO/Content/Python/dc_create_ai_assets.py" "$PY_DEST/dc_create_ai_assets.py"
    cp "$SOURCE_REPO/Content/Python/dc_find_ai_enemies.py"  "$PY_DEST/dc_find_ai_enemies.py"
    cp "$SOURCE_REPO/Content/Python/dc_inspect_project.py"  "$PY_DEST/dc_inspect_project.py"
    cp "$SOURCE_REPO/Content/Python/dc_setup_lyra.py"       "$PY_DEST/dc_setup_lyra.py"

    # C++ source — rsync --delete mirrors exactly so deletions in
    # canonical propagate. Both modules in one go. Trailing slashes are
    # required: with them, contents are mirrored into contents; without,
    # the source dir would nest inside the destination dir.
    rsync -a --delete \
        "$SOURCE_REPO/Source/DeltaCode/" \
        "$DEST/Source/DeltaCode/"
    rsync -a --delete \
        "$SOURCE_REPO/Source/DeltaCodeEditor/" \
        "$DEST/Source/DeltaCodeEditor/"
done

echo "DeltaCode plugin (Python + C++) synced to ${#PLUGIN_DESTINATIONS[@]} project(s)."

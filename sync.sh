#!/bin/bash
# Sync DeltaCode Python files from the repo (master) to every UE project
# that hosts the plugin. Add paths to DESTINATIONS when new projects are set up.
set -e
SOURCE_PYTHON="$HOME/Documents/DeltaCode/Content/Python"
DESTINATIONS=(
    "$HOME/Documents/Unreal Projects/DeltaCodeOpus/Plugins/DeltaCode/Content/Python"
    "$HOME/Documents/Unreal Projects/DC_Prototype/Plugins/DeltaCode/Content/Python"
    "$HOME/Documents/Unreal Projects/DeltaCode/Plugins/DeltaCode/Content/Python"
)

for DEST in "${DESTINATIONS[@]}"; do
    cp "$SOURCE_PYTHON/dc_danger_zone.py" "$DEST/dc_danger_zone.py"
    cp "$SOURCE_PYTHON/dc_create_ai_assets.py" "$DEST/dc_create_ai_assets.py"
    cp "$SOURCE_PYTHON/dc_find_ai_enemies.py" "$DEST/dc_find_ai_enemies.py"
done

echo "DeltaCode Python files synced to ${#DESTINATIONS[@]} project(s)."

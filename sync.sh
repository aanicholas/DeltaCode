#!/bin/bash
PLUGIN_PYTHON="$HOME/Documents/Unreal Projects/DeltaCodeOpus/Plugins/DeltaCode/Content/Python"
SOURCE_PYTHON="$HOME/Documents/DeltaCode/Content/Python"

cp "$SOURCE_PYTHON/dc_danger_zone.py" "$PLUGIN_PYTHON/dc_danger_zone.py"
cp "$SOURCE_PYTHON/dc_create_ai_assets.py" "$PLUGIN_PYTHON/dc_create_ai_assets.py"
cp "$SOURCE_PYTHON/dc_find_ai_enemies.py" "$PLUGIN_PYTHON/dc_find_ai_enemies.py"

echo "DeltaCode Python files synced."

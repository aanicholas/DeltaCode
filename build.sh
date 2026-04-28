#!/bin/bash
# build.sh — Build one or all UE projects that host the DeltaCode plugin.
#
# Usage:
#   ./build.sh           Builds all three projects (fail-fast)
#   ./build.sh opus      Builds DeltaCodeOpus
#   ./build.sh prototype Builds DC_Prototype
#   ./build.sh deltacode Builds the DeltaCode UE project
#
# Each project's engine version is read from the EngineAssociation field
# of its .uproject; the matching engine is expected at /Applications/UE_<ver>.
# Builds the editor target in Development config. Run sync.sh first if you've
# changed Python files — this script only invokes UnrealBuildTool, it does
# not propagate plugin sources.

set -eu

PROJECTS_ROOT="$HOME/Documents/Unreal Projects"

# Read "EngineAssociation": "X.Y" out of a .uproject file. EngineAssociation can
# also hold a GUID for source-built engines; we only support installed Epic
# builds living under /Applications/UE_<version>.
read_engine_version() {
    local uproject="$1"
    sed -nE 's/.*"EngineAssociation"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/p' \
        "$uproject" | head -1
}

build_one() {
    local label="$1"
    local proj_dir="$2"
    local target="$3"
    local uproject="$proj_dir/$(basename "$proj_dir").uproject"

    echo
    echo "=========================================================="
    echo " Building $label  ($target)"
    echo " Project: $uproject"
    echo "=========================================================="

    if [ ! -f "$uproject" ]; then
        echo "ERROR: missing .uproject at $uproject" >&2
        return 1
    fi

    local engine_version
    engine_version="$(read_engine_version "$uproject")"
    if [ -z "$engine_version" ]; then
        echo "ERROR: could not read EngineAssociation from $uproject" >&2
        return 1
    fi

    local engine_root="/Applications/UE_${engine_version}"
    local build_tool="$engine_root/Engine/Build/BatchFiles/Mac/Build.sh"

    if [ ! -x "$build_tool" ]; then
        echo "ERROR: $label needs Unreal Engine ${engine_version}, but it is" >&2
        echo "       not installed at $engine_root." >&2
        echo "       Install UE ${engine_version} via the Epic Games Launcher" >&2
        echo "       (Library tab → Engine Versions → +) or download it to" >&2
        echo "       /Applications/UE_${engine_version}, then re-run this script." >&2
        return 1
    fi

    echo " Engine:  UE ${engine_version}  ($engine_root)"
    echo "----------------------------------------------------------"

    "$build_tool" "$target" Mac Development \
        -Project="$uproject" \
        -WaitMutex
}

build_opus()      { build_one "DeltaCodeOpus" "$PROJECTS_ROOT/DeltaCodeOpus" "DeltaCodeOpusEditor"; }
build_prototype() { build_one "DC_Prototype"  "$PROJECTS_ROOT/DC_Prototype"  "DC_PrototypeEditor"; }
build_deltacode() { build_one "DeltaCode"     "$PROJECTS_ROOT/DeltaCode"     "DeltaCodeEditor"; }

case "${1:-all}" in
    opus)      build_opus ;;
    prototype) build_prototype ;;
    deltacode) build_deltacode ;;
    all)       build_opus && build_prototype && build_deltacode ;;
    -h|--help)
        sed -n '2,14p' "$0"
        exit 0
        ;;
    *)
        echo "Unknown project: $1" >&2
        echo "Usage: $0 [opus|prototype|deltacode]   (no arg = all)" >&2
        exit 2
        ;;
esac

echo
echo "Build complete."

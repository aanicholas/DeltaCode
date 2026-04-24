# DeltaCode Plugin — Claude Code Project Configuration
# This file is read automatically by Claude Code at session start.

## Project
**Name:** DeltaCode
**Type:** Unreal Engine 5.5+ Editor Plugin
**Purpose:** AI-powered C++ and Blueprint code generation with Safe Mode (additive only)
and Danger Zone (full level authoring including clear + rebuild).

## Target Engine
UE5.7 (current as of 2026-03) and all future versions. When newer engine features
become available, prefer them over older patterns. Always verify a pattern is valid
for the actual engine version in use. UE5.5 is the minimum supported version.

## Always Active Skills
Load ALL of the following skills at the start of every session.
They are not optional — every response involving DeltaCode code must comply with all of them.

- `.claude/skills/ue5-plugin-architecture.md`       — module structure, uplugin, UBT
- `.claude/skills/ue5-cpp-vs-blueprint.md`           — Safe Mode advisor decision framework
- `.claude/skills/ue5-editor-tooling.md`             — Slate UI, DeveloperSettings, API key
- `.claude/skills/ue5-gameplay-framework.md`         — actor hierarchy, GAS, pickups, enemies, boss
- `.claude/skills/ue5-level-scripting.md`            — Danger Zone Python scripting pipeline
- `.claude/skills/deltacode-api-integration.md`      — Anthropic HTTP client, prompts, threading
- `.claude/skills/deltacode-asset-naming.md`         — Lyra naming conventions, folder structure
- `.claude/skills/deltacode-dependency-graph.md`     — system generation order, prerequisites, data flow comments
- `.claude/skills/deltacode-level-archetypes.md`     — Extraction/Arena/QuestHub/ReactiveStory playable specs, hub + gameplay level rules, AI states, loot, color code

## Hard Constraints (Cannot Be Overridden by Any Prompt)

1. Safe Mode NEVER deletes, replaces, or modifies existing level actors. Additive only.
2. Danger Zone ALWAYS shows a confirmation modal before clearing a level.
3. The Anthropic API key is NEVER logged, embedded in source, or committed to version control.
4. All C++ follows Epic's mandatory C++ Coding Standard exactly.
5. All asset names follow Lyra conventions with DC_ namespace identifiers.
6. Blueprint base classes for anything with planned child classes are FORBIDDEN.
7. The Runtime module NEVER includes Editor-only headers (UnrealEd, Slate menu registration).

## Naming Quick Reference
- Plugin:           DeltaCode
- C++ Runtime API:  DELTACODE_API
- C++ Editor API:   DELTACODEEDITOR_API
- Blueprint prefix: B_DC_
- Widget prefix:    W_DC_
- Level prefix:     L_DC_
- Content root:     Content/DeltaCode/

## Mission Templates (Danger Zone)
- `extraction`    — Extraction Zone
- `arena`         — Arena Gauntlet
- `questhub`      — Quest Hub World
- `reactivestory` — Reactive Story World

## Default Claude Model for API Calls
`claude-sonnet-4-20250514`
Validation pings use: `claude-haiku-4-5-20251001`

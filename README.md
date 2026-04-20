# DeltaCode

AI-powered code generation and level authoring for Unreal Engine 5.

DeltaCode is a UE5 editor plugin that connects the Anthropic API to the editor,
letting you generate C++ classes, Blueprints, and whole mission layouts from
natural-language prompts. Two modes cover the safety/power trade-off:

- **Safe Mode** — additive only. Never deletes, replaces, or modifies existing
  level actors. Ideal for iterative work on a live level.
- **Danger Zone** — full level authoring, including clear-and-rebuild. Every
  destructive step is gated behind an explicit confirmation modal.

## Features

- Natural-language prompts for C++ class scaffolding and Blueprint creation
- Four prebuilt mission templates: Extraction Shooter, Destiny-style Arena,
  Fallout-style Open World, Narrative Open World RPG
- Editor panel with Safe Mode / Danger Zone toggles, API key configuration,
  and model selection (Claude Sonnet 4 for generation, Haiku 4.5 for validation
  pings)
- Python-driven level scripting pipeline for the Danger Zone clear + rebuild
  flow
- Runtime gameplay framework: pickups, objectives, enemies, bosses, spawn zones,
  extraction zones — all Lyra-naming-compliant (`B_DC_*`, `W_DC_*`, `L_DC_*`)

## Requirements

- Unreal Engine 5.5 or newer (UE5.7 recommended)
- An Anthropic API key (`sk-ant-*`) — get one at
  [console.anthropic.com](https://console.anthropic.com)
- Plugin dependencies: EnhancedInput, GameplayAbilities, PythonScriptPlugin
  (enabled automatically via `DeltaCode.uplugin`)

## Installation

1. Clone into your project's `Plugins/` directory:
   ```
   cd YourProject/Plugins
   git clone https://github.com/aanicholas/DeltaCode.git
   ```
2. Regenerate project files and rebuild.
3. Launch the editor, open **Edit → Plugins**, confirm **DeltaCode** is enabled.
4. Open **Edit → Project Settings → Plugins → DeltaCode** and paste your
   Anthropic API key.

## Safety model

DeltaCode enforces a small set of hard constraints that cannot be overridden by
any prompt:

1. Safe Mode is strictly additive — no deletes, replaces, or modifications to
   existing level actors.
2. Danger Zone always shows a confirmation modal before clearing a level.
3. The Anthropic API key is never logged, embedded in source, or committed to
   version control.
4. The Runtime module never includes Editor-only headers.

## License

DeltaCode is released under the **GNU General Public License v3.0**. See
[`LICENSE`](LICENSE) for the full text.

Copyright (C) 2026 Andrew Nicholas.

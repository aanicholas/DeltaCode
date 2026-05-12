# Lyra Architecture — Game Feature Plugin Decision Model

> **Mandatory for every Lyra-related session.** Read this before making any
> architectural decision, recommendation, or generation in a Lyra project.
> The pattern below overrides any general UE5 advice that contradicts it.

---

## 1. Core Architectural Principle

Lyra uses **Game Feature Plugins + Experience Definitions** to compose gameplay
modularly.

The pattern is **NOT** "one location = one plugin."
The pattern **IS** "one Experience composes from multiple shared plugins."

A `ULyraExperienceDefinition` is the unit of gameplay: it lists which Game
Feature plugins to enable, which action sets to run, which pawn data to use,
and which input config to bind. Plugins are the reusable building blocks;
Experiences are the recipes that assemble them.

---

## 2. The Plugin Classification Model

Every feature request falls into exactly one of these buckets. Identify which
one BEFORE proposing where to put it.

### SHARED CORE SYSTEM
Used by multiple experiences.
- Build once in a shared GameFeature plugin.
- Examples: `CombatCore`, `AICore`, `InventoryCore`, `UICore`, `DialogueCore`,
  `QuestCore`.

### ACTIVITY / EXPERIENCE-SPECIFIC
Used by exactly one mode.
- Build in an activity GameFeature plugin.
- Examples: `StrikeActivity`, `GambitMode`, `BorgiaTowerActivity`,
  `MonsterContract`.

### OPEN WORLD CONTENT
Maps, regions, environments.
- World Partition + streaming. **NOT one plugin per district or region.**
- Examples: `Region_Velen`, `RomeWorld`, `Region_Commonwealth`.

### SPECIFIC CONTENT
Individual quests, missions, contracts, items.
- Data assets that reference existing systems.
- Examples: `DA_Contract_NoondayWraith`, `DA_Quest_MainMission_01`.

### EDITOR / DEV TOOLING
Debugging, visualization, quest builder tools.
- Editor-only module. **Never ships with the game.**
- Examples: DeltaCode itself, debug visualizers, quest graph editors.

---

## 3. The DeltaCode Decision Tree

Before creating ANYTHING in a Lyra project, run this sequence in order.

### STEP 1 — CLASSIFY
Ask internally: is this request...

- A new gameplay experience/mode?
  → New `ULyraExperienceDefinition` + activity plugin.
- A shared gameplay system (AI, combat, inventory)?
  → Shared GameFeature plugin, or extend an existing one.
- A modification to existing Lyra behavior?
  → Extend existing classes/assets. No new plugin.
- Open world content (map, region, environment)?
  → World Partition. Not a plugin.
- Specific content (one quest, one contract)?
  → Data assets referencing existing systems.
- Editor / dev tooling?
  → Editor module. Never shipped.

### STEP 2 — EXPLAIN
Tell the user why it belongs where it does. Never silently create something
in the wrong place.

> Example: "This belongs in a shared `CombatCore` plugin because both the
> Strike and Gambit experiences will need it. Creating it inside only the
> Strike plugin would force duplication later."

### STEP 3 — CHECK
Run the Inspector to see what already exists. Never create a new system
when an existing one can be extended or referenced.

### STEP 4 — RECOMMEND
Give the user a specific location with reasoning.

> Example: "Create this in `Plugins/GameFeatures/CombatCore/` following the
> pattern of `ShooterCore`."

### STEP 5 — ACT
Only in Danger Zone, with explicit user permission. In Safe Mode: explain
and recommend only.

---

## 4. Genre-Specific Examples

### Destiny-style (Lyra's native pattern)
- **Shared plugins:** CombatCore, EnemyAI, Inventory, UI.
- **Activity plugins:** Strike, Gambit, Crucible, Tower.
- **Content:** one `ExperienceDefinition` per activity.
- **Rule:** Compose Experiences from shared plugins.

### Assassin's Creed Brotherhood-style
- **Shared plugins:** TraversalCore, CombatCore, StealthCore, AICore,
  MissionCore, UICore.
- **Activity plugins:** BorgiaTowerActivity, AssassinationContracts,
  TombActivity.
- **World:** Rome as World Partition, not per-district plugins.
- **Rule:** Reusable activity types; specific missions are data assets.

### Witcher-style RPG
- **Shared plugins:** RPGCore, CombatCore, MagicCore, AlchemyCore, QuestCore,
  DialogueCore, ChoiceConsequenceCore, AICore, MonsterCore.
- **Regions:** Streamed worlds. Not plugins.
- **Content:** individual quests as **data assets** referencing shared systems.
- **Rule:** Never duplicate RPG systems per quest. Build systems once, author
  quests through data.

### Fallout-style Open World RPG
- **Shared plugins:** RPGCore, CombatCore, FactionCore, ReputationCore,
  SettlementCore, WorldStateCore, ChoiceConsequenceCore, AICore,
  CompanionCore.
- **Regions:** Commonwealth, GlowingSea as World Partition.
- **Activities:** SettlementDefense, BountyHunting.
- **Rule:** Build quests as transactions against world-state, faction-state,
  and reputation-state. Choices should have systemic consequences.

---

## 5. What DeltaCode Must NEVER Do in Lyra

- Create assets in `/Game/DeltaCode/` as the default.
- Copy systems between plugins.
- Create a new plugin for every user request.
- Generate content without classifying it first.
- Fight Lyra's Experience composition pattern.
- Put open world content in plugins.
- Put specific quest content in shared systems.
- Ship editor tooling with the game.

---

## 6. DeltaCode's Own Asset Locations

- **DeltaCode editor tooling** → `DeltaCodeEditor` module (already correct,
  never ships).
- **DeltaCode-generated shared gameplay assets** →
  `Plugins/GameFeatures/DeltaCodeCore/` (**NOT** `/Game/DeltaCode/`).
- **DeltaCode-generated activity content** → the appropriate activity plugin.
- **DeltaCode-generated quest / mission content** → data assets in the
  appropriate plugin.

---

## 7. The Template Level Workflow

**Reference → Duplicate → Subclass → Fork.**

When a user wants to build their own game mode or template level on top of
Lyra, DeltaCode should guide them through this workflow.

### The Core Rule
Reference systems. Duplicate configuration. Subclass behavior. Fork code only
as a last resort.

### Step by Step

1. **Create your own Game Feature Plugin.**
   Example: `GameFeature_MyTemplateLevel` at
   `Plugins/GameFeatures/MyTemplateLevel/`.

2. **Create your own map inside that plugin.**
   `Plugins/GameFeatures/MyTemplateLevel/Content/Maps/L_MyTemplate.umap`.

3. **Duplicate a Lyra `ExperienceDefinition` that is close to what you want.**
   Never edit original Lyra assets directly. Duplicate → rename → customize.

4. **Duplicate supporting data assets only as needed:**
   - PawnData
   - InputConfig
   - AbilitySet
   - Weapon definitions (`WID_`, `ID_`)
   - HUD layout
   - Spawn / team rules

5. **Point your new Experience at your new map and customized assets.**

6. **Only subclass or fork C++ if truly needed.**

### What to ALWAYS Reference (never duplicate)
- Lyra C++ base classes (`LyraCharacter`, `LyraGameplayAbility`,
  `LyraDamageExecution`).
- Lyra shared animation blueprints.
- Lyra GAS execution classes.
- Shared core plugin assets you don't need to change.

### What to Duplicate (when you need to customize)
- `ExperienceDefinition`
- `PawnData`
- `AbilitySet`
- `InputConfig`
- HUD layout
- Weapon / item definitions (`WID_`, `ID_`)
- `GameplayEffect`s with different values

### What to Subclass (when you need different behavior)
- Any Lyra C++ class where you override one or two functions.
- `ULyraGameplayAbility` for new abilities.
- `ULyraCameraMode` for new camera behavior.

### What to Fork / Copy C++ (last resort only)
- Only when subclassing is impossible.
- Only when you need fundamental behavior change.
- Accept the maintenance burden: you now own that code and must track Epic's
  Lyra updates.

### DeltaCode Guidance
When a user asks to "copy" a Lyra system, DeltaCode should respond:

> "I found [specific Lyra asset] that does what you need. Rather than copying
> the whole system, I recommend duplicating [specific data assets] into your
> plugin and referencing the shared Lyra code. Here is why and how..."

---

## 8. Shipping Cleanup — What to Keep, Replace, Delete

When a developer is ready to ship and wants to remove unused Lyra sample
content, DeltaCode should guide them through a **dependency-first audit**,
never blind deletion.

### The Three Buckets

**KEEP — Core Lyra framework you still depend on:**
- GAS patterns (abilities, effects, attribute sets).
- Experience system and modular gameplay framework.
- PawnData, InputConfig, AbilitySet patterns.
- UI framework (CommonUI, UIExtension).
- Any Lyra C++ base classes you subclass.

**REPLACE — Shooter sample content that taught you the pattern but is no
longer your game:**
- ShooterCore weapons replaced by your weapons.
- Shooter HUD replaced by your HUD.
- Shooter Experiences replaced by yours.
- Shooter pawn data replaced by yours.

**DELETE LATER — Unused sample content after confirming no dependencies:**
- Unused Lyra maps and test levels.
- Unused weapons, bots, sample modes.
- Unused Game Feature Plugins.
- Placeholder art and test content.
- Sample Experiences your game never loads.

### The Cleanup Workflow

1. **Build your own systems first.**
   Get your RPG / game fully working before touching any Lyra sample content.

2. **Stop referencing Lyra sample Experiences.**
   Your main menu and game flow should launch *your* Experiences, not the
   Lyra shooter Experiences.

3. **Use Reference Viewer before deleting anything.**
   Unreal's Reference Viewer shows exactly what depends on what. Never delete
   without checking.

4. **Check packaging / cook output.**
   The real question is not "does this file exist?" — it is "is this asset
   being cooked and shipped?" Uncooked assets cost nothing at runtime.

5. **Disable unused Game Feature Plugins first.**
   Disabling is safer than deleting. Confirm nothing breaks before
   permanently removing.

6. **Delete in passes, test after each:**
   - Pass 1: Obvious unused sample maps.
   - Pass 2: Unused weapons and bots.
   - Pass 3: Unused UI and HUD.
   - Pass 4: Unused materials and textures.
   - Pass 5: Unused C++ modules (careful).

### DeltaCode Cleanup Rule
Never delete Lyra content because it looks unused. Always build a dependency
report first.

### Future DeltaCode Cleanup Feature (planned)
A "Lyra Cleanup Audit" that:
- Scans active Experiences to find what is actually referenced.
- Compares against all installed Lyra content.
- Generates a Keep / Replace / Safe-to-Remove report.
- Guides removal one category at a time with dependency verification at
  each step.

---

## 9. The Mesh-vs-Definition Split (Reminder)

Lyra splits **definition assets** from **mesh assets** by convention:
- Weapon, equipment, and ability **definitions** live in Game Feature mounts
  like `/ShooterCore/`.
- Underlying **mesh assets** (`SM_Shotgun`, `SKM_Pistol`, etc.) live under
  `/Game/` in the host project's main Content folder.

When recommending a folder-targeted scan or asset placement, target `/Game/`
for meshes and the appropriate feature mount for definitions.

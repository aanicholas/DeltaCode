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

## 7. The Mesh-vs-Definition Split (Reminder)

Lyra splits **definition assets** from **mesh assets** by convention:
- Weapon, equipment, and ability **definitions** live in Game Feature mounts
  like `/ShooterCore/`.
- Underlying **mesh assets** (`SM_Shotgun`, `SKM_Pistol`, etc.) live under
  `/Game/` in the host project's main Content folder.

When recommending a folder-targeted scan or asset placement, target `/Game/`
for meshes and the appropriate feature mount for definitions.

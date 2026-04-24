# DeltaCode Playable Level Archetypes

## Purpose

DeltaCode should be able to generate playable prototype levels from a fresh Unreal Engine first-person or third-person template.

These prototypes do not need final art, animations, lighting, audio, VFX, or polished UI. They should be functional graybox levels using placeholder geometry, colored materials, simple AI behavior, triggers, loot, NPCs, vendors, and basic mission logic.

Each archetype requires two levels:

- **Gameplay Level** — where combat, exploration, objectives, or story events happen.
- **Hub / Return Level** — where the player returns, stores loot, talks to NPCs, accepts missions, or prepares for the next run.

## Global Prototype Rules

### Character Color Code

Use simple colored materials on capsule/mesh placeholders.

| Type | Color |
|---|---|
| Enemy unaware | Green |
| Enemy alert / suspicious | Yellow |
| Enemy attacking | Red |
| Boss enemy | Dark Red |
| Vendor | Purple |
| Quest giver | Purple |
| Important dialogue NPC / rescue target | Blue |
| Basic crowd NPC | White |
| Friendly ally / companion | Cyan |
| Neutral guard / patrol | Orange |
| Loot container | Light Green |
| Objective item / terminal | Gold |
| Extraction / exit point | Bright Green or Teal |
| Locked / gated area | Gray |
| Hazard | Black or Red/Black |

### Enemy AI Rules

Enemies should not detect or attack the player from across the entire map.

Each enemy should have:

- A patrol route or idle area.
- A limited detection radius.
- A limited attack radius.
- A simple line-of-sight check if possible.

Three basic awareness states:

- **Unaware** — green, patrols or idles.
- **Alert** — yellow, investigates or turns toward player.
- **Combat** — red, moves toward and attacks player.

Enemies should return to patrol if the player leaves detection range for a set time. Enemies should only activate when the player enters their local encounter area or perception range.

Prototype behavior is acceptable:

- Walk back and forth.
- Chase player when detected.
- Stop and attack when close.
- Drop simple loot when killed.

### Loot Rules

All gameplay archetypes should support basic loot.

Loot can be represented as:

- Floating colored cube.
- Simple pickup actor.
- Text label above item.
- Data fields such as name, type, rarity, value.

Basic loot types:

- Currency
- Ammo
- Health item
- Weapon item
- Crafting material
- Mission item
- Vendor trash
- Rare item

Loot should be collectable and added to a basic inventory array.

### Hub / Storage Rules

Each archetype has a second level that acts as a hub or return location.

The hub should include:

- Player spawn point.
- Storage container.
- Vendor NPC.
- Quest / mission NPC when appropriate.
- Level transition back to gameplay level.
- Simple inventory transfer between player and storage.

Storage can be prototype-only:

- A chest actor.
- UI list or debug text list.
- "Press E to store all loot."
- "Press E to retrieve loot."

## Archetype 1 — Extraction Zone

### Description

A replayable combat-and-loot level where the player enters a dangerous area, fights enemies, collects loot, and escapes through an extraction point.

### Required Levels

#### Level 1: `L_ExtractionZone_Play`

Gameplay area where the player searches for loot and escapes.

#### Level 2: `L_ExtractionZone_Base`

Home base where the player returns after extraction.

### Gameplay Level Structure

Create a medium-sized graybox combat map.

Include:

- Player spawn area.
- Multiple loot zones.
- Blocking geometry for cover.
- Enemy patrol areas.
- Optional hazard zone.
- Extraction point.
- Boundary walls or terrain blockers.

Basic layout:

- Safe-ish entry zone.
- First loot area with light enemies.
- Central combat area with more cover.
- Higher-value loot area with stronger enemies.
- Extraction zone placed away from spawn.

The player objective:

1. Enter the zone.
2. Collect loot.
3. Avoid or fight enemies.
4. Reach extraction point.
5. Use extraction point to return to base.

### Enemy Setup

Enemies should patrol small areas.

Enemy behavior:

- Patrol while green.
- Turn yellow when player enters suspicion range.
- Turn red and attack when player enters combat range.
- Drop loot when killed.

Enemy placement:

- Do not place enemies so they all aggro at once.
- Use encounter zones.
- Each encounter should activate locally.

### Extraction Logic

The extraction point should be a visible marker.

Suggested behavior:

- Bright green or teal cylinder / platform.
- Text label: **Extraction Point**
- Trigger volume.
- Player enters extraction zone.
- Player must remain inside for 5 seconds.
- If still inside after countdown, load `L_ExtractionZone_Base`.

Optional:

- Spawn a small enemy wave when extraction starts.
- Cancel extraction if player leaves the zone.

### Base Level Structure

Create a small home base.

Include:

- Player spawn point.
- Storage container.
- Vendor NPC.
- Mission board or deployment terminal.
- Exit/launch point back to extraction gameplay level.

Base functions:

- Store collected loot.
- Sell loot to vendor.
- Start another extraction run.
- Display basic stats:
  - Loot collected.
  - Enemies defeated.
  - Successful extractions.

## Archetype 2 — Arena Gauntlet

### Description

A mission level made of combat arenas connected by calmer traversal sections. The player moves through several enemy encounters, reaches a boss arena, defeats waves, damages the boss in phases, then receives loot.

### Required Levels

#### Level 1: `L_ArenaGauntlet_Play`

Mission level with encounter rooms, traversal paths, and boss fight.

#### Level 2: `L_ArenaGauntlet_Hub`

Social preparation area with bounty giver, vendors, and mission launch point.

### Gameplay Level Structure

Create a linear or semi-linear mission.

Basic structure:

1. Player spawn.
2. Combat arena 1.
3. Relaxation / traversal path.
4. Combat arena 2.
5. Relaxation / hallway / outdoor path.
6. Combat arena 3.
7. Final boss arena.
8. Loot reward area.
9. Return-to-hub trigger.

### Combat Arena Rules

Each arena should have:

- Cover geometry.
- Enemy spawn points.
- Trigger volume at entrance.
- Exit gate or door.
- Objective text.

Arena logic:

1. Player enters arena trigger.
2. Enemies activate or spawn.
3. Exit remains locked.
4. Player kills required enemies.
5. Exit unlocks.
6. Player continues.

Enemies should not activate before the player enters the arena.

### Relaxation Path Rules

Between arenas, create lower-pressure traversal spaces.

These can be:

- Hallways.
- Caves.
- Outdoor paths.
- Bridges.
- Platforming sections.
- Light exploration corridors.

Purpose:

- Let player recover.
- Provide pacing.
- Give room for dialogue, pickups, or environmental storytelling.

Use little or no combat here.

### Boss Fight Rules

Boss should be dark red.

Boss has three damage phases.

Example:

```
Boss health:          300
Phase 1 damage limit: 100
Phase 2 damage limit: 200
Phase 3 damage limit: 300
```

Boss fight flow:

1. Player enters boss arena.
2. Wave 1 enemies spawn.
3. Player defeats wave 1.
4. Boss shield turns off.
5. Player damages boss to 66% health.
6. Boss shield turns on.
7. Wave 2 enemies spawn.
8. Player defeats wave 2.
9. Boss shield turns off.
10. Player damages boss to 33% health.
11. Boss shield turns on.
12. Final wave spawns.
13. Boss shield turns off.
14. Boss and enemies fight at the same time.
15. Player kills boss.
16. Loot chest appears.
17. Exit opens.

Boss shield can be represented by:

- Blue forcefield material.
- Floating shield ring.
- Text label: **Shielded**
- Damage ignored while shield is active.

### Hub Level Structure

Create a small mission hub.

Include:

- Player spawn.
- Bounty NPC.
- Vendor NPC.
- Storage chest.
- Mission launch terminal.
- Return-from-mission location.

Hub functions:

- Accept bounty.
- Buy supplies.
- Store loot.
- Launch arena mission.
- Turn in completed mission.

## Archetype 3 — Quest Hub World

### Description

A small open-world RPG prototype focused on exploration, vendors, loot storage, NPC conversations, and simple quests.

### Required Levels

#### Level 1: `L_QuestHubWorld_Play`

Small open-world area with NPCs, enemies, vendors, quests, and loot.

#### Level 2: `L_QuestHubWorld_Home`

Player home or safe storage area.

### Gameplay Level Structure

Create a compact open-world graybox map.

Include:

- Starting area.
- Settlement or town.
- Vendor area.
- Quest giver.
- Important lore NPC.
- Enemy area.
- Loot area.
- Return path to home/storage level.

Basic flow:

1. Player starts with short backstory text.
2. Player meets first quest giver.
3. Quest giver assigns a simple mission.
4. Player travels to nearby objective area.
5. Player fights or avoids enemies.
6. Player collects objective item.
7. Player returns to quest giver.
8. Quest giver sends player to vendor.
9. Vendor interaction introduces buying/selling.
10. Player is introduced to item storage.
11. Player continues exploring.

### Quest Example

**Quest name:** Recover Supplies

Quest steps:

1. Talk to quest giver.
2. Go to supply cache.
3. Defeat or avoid enemies.
4. Pick up supply item.
5. Return to quest giver.
6. Receive reward.
7. Talk to vendor.
8. Store extra items.

### NPC Setup

Include several NPC categories:

- Purple quest giver.
- Purple vendor.
- Blue lore NPC.
- White background NPCs.
- Red/green/yellow enemies depending on aggro state.

Important dialogue NPCs should provide lore.

Example lore topics:

- What happened here.
- Who controls the area.
- Why enemies are nearby.
- What the player should care about.
- What larger conflict exists.

### Home Level Structure

Create a small player home or safehouse.

Include:

- Player spawn.
- Storage chest.
- Bed or rest area.
- Workbench placeholder.
- Exit back to open-world gameplay level.

Home functions:

- Store loot.
- Retrieve loot.
- Rest/heal.
- Return to world.

## Archetype 4 — Reactive Story World

### Description

A narrative open-world prototype where quests can change the state of the world. The world should react through lighting, music placeholder states, NPC behavior, unlocked areas, and visual changes.

### Required Levels

#### Level 1: `L_ReactiveStoryWorld_Play`

Open-world story area with quests, NPCs, enemies, and world-state changes.

#### Level 2: `L_ReactiveStoryWorld_Home`

Player home, camp, base, or safehouse where loot can be stored.

### Gameplay Level Structure

Create a small open-world area with at least two visible world states.

Example world states:

- **Oppressed / unsafe state**
  - Darker lighting.
  - More enemies.
  - Fewer NPCs walking around.
  - Locked vendor or limited services.
  - Warning text.
- **Improved / safer state**
  - Brighter lighting.
  - Fewer enemies.
  - More white crowd NPCs.
  - Vendor becomes available.
  - New dialogue appears.
  - Optional music state changes.

### Core Quest Flow

Create a main local quest that changes the area.

**Example quest:** Secure the District

Steps:

1. Talk to blue important NPC.
2. Learn that enemies control the area.
3. Travel to enemy-controlled zone.
4. Defeat enemy leader or destroy objective.
5. Return to important NPC.
6. World state changes from unsafe to safer.
7. NPC population changes.
8. Vendor unlocks.
9. Lighting becomes brighter.
10. New dialogue becomes available.

### World Reactivity Rules

When the quest is completed, change the level state.

Prototype changes can include:

- Change sky light intensity.
- Change directional light color/intensity.
- Hide some enemy spawners.
- Spawn more white crowd NPCs.
- Unlock vendor.
- Open previously locked gate.
- Change background music placeholder variable.
- Change NPC animation state placeholder.
- Update dialogue text.
- Enable new quest marker.

This does not need full systemic simulation. It only needs a visible before/after state.

### Home Level Structure

Create a player home, camp, or base.

Include:

- Player spawn.
- Storage chest.
- Important companion or advisor NPC.
- Loot storage.
- Exit back to story world.

Home functions:

- Store loot.
- Retrieve loot.
- Rest/heal.
- Review current world state.
- Return to story world.

## Recommended DeltaCode Build Order

For each archetype, DeltaCode should build in this order:

1. Create required level names.
2. Create graybox terrain/floor.
3. Add walls, cover, paths, and boundaries.
4. Add player start.
5. Add colored placeholder materials.
6. Add NPC/enemy placeholder actors.
7. Add patrol points.
8. Add enemy perception ranges.
9. Add loot actors.
10. Add quest/objective actors.
11. Add triggers.
12. Add level travel points.
13. Add storage container.
14. Add vendor interaction.
15. Add mission/quest logic.
16. Add debug UI text.
17. Add win/complete conditions.
18. Add simple failure/retry logic if needed.

## Minimum Functional Systems

DeltaCode should create or reuse these systems:

### Required

- Basic enemy AI
- Patrol points
- Detection radius
- Aggro state color switching
- Health component
- Damage handling
- Loot pickup
- Inventory array
- Storage container
- Vendor interaction
- Quest state tracking
- Level transition
- Trigger volumes
- Objective text/debug UI

### Optional Later

- Dialogue UI
- Save/load
- Better combat AI
- Faction reputation
- Companion behavior
- Real animations
- Real loot stats
- Real economy
- Real procedural spawning
- Real music/lighting transitions

## Important Implementation Note

For the first prototype pass, DeltaCode should not depend on final Unreal assets.

Use:

- Cubes
- Capsules
- Planes
- Trigger boxes
- Text render components
- Colored materials
- Simple Blueprint or C++ actors
- Data-driven spawn points

Avoid relying on copied asset text as the primary workflow. That can be useful later for duplicating known asset setups, but the first prototype should be generated from clean systems and placeholder actors.

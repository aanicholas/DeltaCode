# SKILL: deltacode-dependency-graph
# DeltaCode Plugin — System Generation Order, Dependencies, and Data Flow Comments

## Purpose
This skill governs the ORDER in which DeltaCode generates systems, the PREREQUISITES
each system requires before it can be built, and the COMMENT CONVENTION that makes
data flow legible in every generated C++ file.

DeltaCode must consult this skill before generating any system. If a requested system
has unmet prerequisites, DeltaCode must surface them and offer to generate foundations
first — never silently skip them or generate a broken partial system.

---

## The Cardinal Rule

DeltaCode generates systems in dependency order. Always.
It does not generate ten systems at once from one prompt.
It does not infer a complete game from a single sentence.
It does not treat greybox scaffolds as production-ready output.

The value of DeltaCode is correct, extendable foundations — not volume of output.

---

## Data Flow Comment Convention

Every generated C++ file that receives input from another system or sends output to
another system MUST include standardized flow comments at the relevant call sites.

### Format
```cpp
// [INPUT]  From: ClassName → via MethodName() or delegate name
// [OUTPUT] To:   ClassName → via MethodName() or delegate name
// [EVENT]  Fires: DelegateName — consumed by ClassName
// [DEPENDS ON] SystemName must exist and be initialized before this runs
```

### Rules
- Place [INPUT] comments immediately above the function or parameter that receives data
- Place [OUTPUT] comments immediately above the call or broadcast that sends data
- Place [EVENT] comments above delegate declarations to document who listens
- Place [DEPENDS ON] in BeginPlay() or InitializeComponent() where order matters
- Be specific — name the actual class and method, not just "the player" or "the UI"
- Keep comments one line each — no paragraphs

### Examples

```cpp
// [INPUT]  From: ADCPlayerController → via UDCInteractionComponent::TryInteract()
void UDCInteractionComponent::TryInteract()
{
    // [OUTPUT] To: UDCDialogueComponent::StartConversation() on hit NPC actor
    // [OUTPUT] To: ADCPickupBase::OnPickedUp() on hit pickup actor
    // [OUTPUT] To: ADCDoor::OnInteracted() on hit door actor
    if (HitActor->Implements<UDCInteractable>())
    {
        IDCInteractable::Execute_OnInteracted(HitActor, OwnerPawn);
    }
}
```

```cpp
// [EVENT] Fires: OnQuestObjectiveUpdated — consumed by W_DC_QuestLog (UI update)
//                                        — consumed by UDCQuestSubsystem (state tracking)
UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
FOnQuestObjectiveUpdated OnQuestObjectiveUpdated;
```

```cpp
void UDCInventoryComponent::BeginPlay()
{
    // [DEPENDS ON] UDCItemDefinitionRegistry must be initialized in GameInstance
    //              before any AddItem() calls are valid
    Super::BeginPlay();
    ItemRegistry = GetGameInstance()->GetSubsystem<UDCItemDefinitionRegistry>();
}
```

```cpp
// [INPUT]  From: UDCWeaponComponent::OnFirePressed() → via damage event
// [INPUT]  From: ADCEnemyBase::ApplyDamage() → direct call on hit
void UDCHealthComponent::ApplyDamage(float DamageAmount, AActor* DamageSource)
{
    // [OUTPUT] To: ADCEnemyBase::OnDeath_Implementation() when health reaches zero
    // [OUTPUT] To: W_DC_BossHealthBar → via OnHealthChanged delegate (if boss)
    OnHealthChanged.Broadcast(CurrentHealth, -DamageAmount);
}
```

---

## Generation Layers

### Layer 0 — Concept Resolution (No Code)
This is not code generation. This is architecture planning.

DeltaCode must complete this layer before writing a single line of C++.

Inputs the user provides:
- Freeform game concept description
- Answers to clarifying questions

Outputs DeltaCode produces (as a structured spec for user confirmation):
- Game type (FPS, RPG, extraction, arena, etc.)
- Interaction model (how does the player affect the world)
- Core gameplay systems needed (from the list below)
- Progression model (level-based, skill-based, class-based, hybrid, none)
- World structure (linear, open world, hub-and-spoke, procedural)
- Architecture profile (which layers will be needed)

DeltaCode asks the user to confirm this spec before proceeding to Layer 1.
It does NOT auto-proceed. The spec confirmation is a required gate.

### Layer 1 — Core Project Skeleton
**Prerequisites:** Confirmed concept spec from Layer 0.
**This layer has no runtime dependencies — it is the root.**

Required outputs:
- Plugin/module folder structure (per ue5-plugin-architecture skill)
- Naming convention decisions (per deltacode-asset-naming skill)
- `ADCGameModeBase` — C++
- `ADCPlayerControllerBase` — C++
- `ADCCharacterBase` — C++
- `ADCHUDBase` — C++
- `UDCGameInstance` — C++ (or `UDCGameInstanceSubsystem` pattern)
- Shared interfaces: `IDCInteractable`, `IDCDamageable`
- Shared enums/structs: `EDCItemType`, `FDCItemHandle`, `FDCStatBlock`
- Gameplay Tags setup if GAS is selected
- `UDCInputConfig` + Enhanced Input setup (IA_DC_ and IMC_DC_ assets described)
- Base HUD widget shell: `W_DC_HUDRoot` (empty, mounts child widgets)
- SaveGame stub: `UDCSaveGame` (empty struct, wired to GameInstance)

```
Layer 1 Output Map:

ADCGameModeBase ──────────────────────── sets PlayerController, Pawn, HUD classes
ADCPlayerControllerBase ──────────────── owns UDCInteractionComponent (Layer 2A)
                                          owns UDCInventoryComponent (Layer 2C)
ADCCharacterBase ──────────────────────── pawn/character, Enhanced Input bound here
ADCHUDBase ────────────────────────────── mounts W_DC_HUDRoot, manages widget stack
UDCGameInstance ───────────────────────── owns persistent subsystems
Shared interfaces ─────────────────────── IDCInteractable, IDCDamageable
Shared structs ────────────────────────── FDCItemHandle, FDCStatBlock, etc.
```

### Layer 2 — Shared Gameplay Foundations
**Prerequisites:** Layer 1 complete and confirmed working (compiles, opens in editor).

#### 2A. Interaction System
Used by: pickups, NPCs, doors, trading terminals, quest givers, world objects.

Outputs:
- `IDCInteractable` interface (may already exist from Layer 1)
- `UDCInteractionComponent` — attaches to `ADCPlayerControllerBase`
- Line trace or overlap detection logic (configurable)
- Interact prompt hook → fires event for HUD to display "Press E to interact"
- Generic `OnInteracted(APawn* Interactor)` event pathway

```cpp
// [INPUT]  From: ADCPlayerControllerBase → via Enhanced Input IA_DC_Interact action
// [OUTPUT] To:   IDCInteractable::Execute_OnInteracted() on the hit/overlapped actor
// [OUTPUT] To:   ADCHUDBase::ShowInteractPrompt() when valid target is in range
void UDCInteractionComponent::TryInteract() { ... }
```

Dependencies: Player framework, Input setup, HUD shell.

#### 2B. Data Definition Layer
The single source of truth for all game content definitions.
All systems read from here. Nothing invents its own data format.

Outputs:
- `UDCItemDefinition` — base data asset (UPrimaryDataAsset)
- `UDCWeaponDefinition` — inherits UDCItemDefinition
- `UDCQuestDefinition` — UPrimaryDataAsset
- `FDCDialogueNode` — struct (not a full asset yet)
- `UDCNPCDefinition` — UPrimaryDataAsset
- `UDCVendorDefinition` — UPrimaryDataAsset
- `FDCEconomyEntry` — struct (price, currency type, stack rules)
- `UDCItemDefinitionRegistry` — UGameInstanceSubsystem, loads and indexes all definitions

```cpp
// [OUTPUT] To: UDCInventoryComponent::AddItem() — provides FDCItemHandle
// [OUTPUT] To: UDCWeaponComponent::Initialize() — provides weapon stats
// [OUTPUT] To: UDCVendorComponent::BuildStock() — provides vendor inventory
UDCItemDefinitionRegistry* Registry =
    GetGameInstance()->GetSubsystem<UDCItemDefinitionRegistry>();
```

Dependencies: Folder structure, shared structs/enums.

#### 2C. Inventory Foundation
One of the most critical early systems — many others depend on it.

Outputs:
- `UDCInventoryComponent` — attaches to `ADCPlayerControllerBase`
- Item container: `TArray<FDCInventoryEntry>` (definition handle + instance data)
- `AddItem()`, `RemoveItem()`, `HasItem()`, `GetItemCount()`
- Stack rules (stackable vs unique items)
- Use/drop hooks
- `OnInventoryChanged` delegate — fires to UI and any listener

```cpp
// [INPUT]  From: ADCPickupBase::OnPickedUp() → via OwnerController->InventoryComponent
// [INPUT]  From: UDCVendorComponent::CompletePurchase() → adds bought item
// [INPUT]  From: UDCLootComponent::DropLoot() → awards loot to player
// [OUTPUT] To:   W_DC_InventoryScreen → via OnInventoryChanged delegate
// [OUTPUT] To:   UDCQuestSubsystem → via OnInventoryChanged (quest item tracking)
// [EVENT]  Fires: OnInventoryChanged — consumed by UI and quest system
UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Inventory")
FOnInventoryChanged OnInventoryChanged;
```

Dependencies: Interaction system, data definition layer, player framework.

#### 2D. UI Foundation
Not the full UI feature set — just the scaffolding all UI panels mount into.

Outputs:
- `W_DC_HUDRoot` — root widget, manages child widget stack
- Menu manager pattern — `ADCHUDBase::PushWidget()` / `PopWidget()`
- Input mode switching hooks (game-only ↔ game+UI ↔ UI-only)
- Basic notification framework: `ShowMessage(FText, float Duration)`

```cpp
// [INPUT]  From: Any gameplay system → via ADCHUDBase::PushWidget(WidgetClass)
// [OUTPUT] To:   UDCInputSubsystem → SetInputMode() when widget stack changes
void ADCHUDBase::PushWidget(TSubclassOf<UUserWidget> WidgetClass) { ... }
```

Dependencies: PlayerController, core project skeleton.

---

### Layer 3 — Core Playable Systems

**Prerequisites:** All of Layer 2 confirmed working.
**Generate one system at a time. Confirm each before starting the next.**

#### 3A. Weapon System

```
Dependencies:         Player framework, Inventory foundation, Data definition layer, UI foundation
Optional later:       Animation system, VFX/SFX, attachments, damage types
Greybox rule:         Weapon logic must function WITHOUT animation in greybox mode.
                      Do not make animation a prerequisite for firing.
```

Outputs:
- `ADCWeaponBase` — C++ actor base (see ue5-gameplay-framework skill)
- `UDCWeaponComponent` — attaches to `ADCCharacterBase`, manages equipped weapon
- Fire, reload, ammo tracking, weapon switching, equip/unequip
- Hit detection (line trace for hitscan, projectile actor for projectile)
- Damage application via `IDCDamageable::ApplyDamage()`

```cpp
// [INPUT]  From: ADCPlayerControllerBase → via Enhanced Input IA_DC_Fire action
// [INPUT]  From: UDCInventoryComponent → weapon item must exist in inventory to equip
void UDCWeaponComponent::OnFirePressed()
{
    // [OUTPUT] To: ADCProjectileBase::Launch() if projectile weapon
    // [OUTPUT] To: IDCDamageable::Execute_ApplyDamage() on hit actor if hitscan
    // [OUTPUT] To: W_DC_AmmoReadout → via OnAmmoChanged delegate
}
```

#### 3B. Dialogue System

```
Dependencies:         Interaction system, UI foundation, Data definition layer
Optional later:       Quest system integration, reputation/faction, voiceover, cinematic camera
Architecture note:    Dialogue state lives on the NPC (UDCDialogueComponent), NOT the player.
                      The player's interaction system invokes it. This allows each NPC to own
                      its conversation tree independently.
```

Outputs:
- `UDCDialogueComponent` — attaches to NPC actors
- `FDCDialogueNode` struct — text, response options, condition hooks, next node ID
- `W_DC_DialoguePanel` widget — displays current node, response buttons
- Conversation start/end, node progression, response selection

```cpp
// [INPUT]  From: UDCInteractionComponent::TryInteract() → player interacts with NPC
// [INPUT]  From: UDCQuestSubsystem → via ForceStartDialogue() for scripted sequences
void UDCDialogueComponent::StartConversation(APawn* Initiator)
{
    // [OUTPUT] To: ADCHUDBase::PushWidget(W_DC_DialoguePanel) — shows dialogue UI
    // [OUTPUT] To: UDCInputSubsystem::SetUIInputMode() — switches input to cursor mode
}

// [INPUT]  From: W_DC_DialoguePanel → player selects a response option
void UDCDialogueComponent::SelectResponse(int32 ResponseIndex)
{
    // [OUTPUT] To: UDCQuestSubsystem::OnDialogueConditionMet() if node has quest hook
    // [OUTPUT] To: UDCReputationSubsystem::ApplyDialogueImpact() if node has rep hook
}
```

#### 3C. Quest System

```
Dependencies:         Data definition layer, UI foundation, Interaction system
Optional later:       Dialogue system, inventory system, combat events, faction/reputation
Architecture note:    Quest logic must NOT be hard-coded only inside dialogue nodes.
                      UDCQuestSubsystem owns all quest state independently.
                      Dialogue nodes may TRIGGER quest events — they do not contain quest logic.
```

Outputs:
- `UDCQuestSubsystem` — `UGameInstanceSubsystem`, persists across level loads
- `FDCQuestState` — runtime struct tracking active quest progress
- `AcceptQuest()`, `UpdateObjective()`, `CompleteQuest()`, `FailQuest()`
- `OnQuestStateChanged` delegate
- `W_DC_QuestLog` widget

```cpp
// [INPUT]  From: UDCDialogueComponent::SelectResponse() — dialogue triggers quest accept
// [INPUT]  From: ADCMissionObjective::SetObjectiveState() — world event updates quest
// [INPUT]  From: UDCInventoryComponent::OnInventoryChanged — item pickup updates quest
void UDCQuestSubsystem::UpdateObjective(FName QuestID, FName ObjectiveID)
{
    // [OUTPUT] To: W_DC_QuestLog → via OnQuestStateChanged delegate
    // [OUTPUT] To: ADCHUDBase::ShowNotification() — "Objective updated" message
    // [OUTPUT] To: UDCSaveGame::MarkDirty() — flags save data for update
}
```

#### 3D. Trading / Economy System

```
Dependencies:         Inventory foundation, Data definition layer, Interaction system, UI foundation
Optional later:       Reputation modifiers, dynamic pricing, vendor stock refresh, barter skills
```

Outputs:
- `UDCVendorComponent` — attaches to vendor NPC actors
- `BuildStock()` from `UDCVendorDefinition` data asset
- `GetBuyPrice()`, `GetSellPrice()`, currency handling
- `CompletePurchase()`, `CompleteSale()` — transfer items via inventory component
- `W_DC_VendorPanel` widget

```cpp
// [INPUT]  From: UDCInteractionComponent::TryInteract() → player opens vendor
void UDCVendorComponent::OpenVendorSession(UDCInventoryComponent* PlayerInventory)
{
    // [OUTPUT] To: ADCHUDBase::PushWidget(W_DC_VendorPanel)
    // [OUTPUT] To: W_DC_VendorPanel::PopulateStock() — passes current vendor stock
}

// [INPUT]  From: W_DC_VendorPanel → player confirms purchase
void UDCVendorComponent::CompletePurchase(FDCItemHandle ItemHandle, int32 Quantity)
{
    // [OUTPUT] To: PlayerInventory->RemoveItem() — deduct currency
    // [OUTPUT] To: PlayerInventory->AddItem() — add purchased item
    // [OUTPUT] To: W_DC_VendorPanel::RefreshStock() — update panel display
}
```

#### 3E. NPC Interaction Framework

```
Dependencies:         Interaction system, dialogue system hooks, quest hooks, data definition layer
Optional later:       Behavior trees, schedules, social simulation, faction behavior, combat AI
Architecture note:    This is gameplay-facing NPC interaction support — not full AI.
                      Full AI (behavior trees, perception) is a Layer 6 system.
```

Outputs:
- `ADCNPCBase` — C++ Character base for all interactable NPCs
- `ENPCRole` enum: `QuestGiver`, `Vendor`, `Ambient`, `HostileOnTrigger`
- Component attachment pattern: each role adds the appropriate component
  (`UDCDialogueComponent`, `UDCVendorComponent`, `UDCQuestGiverComponent`)

```cpp
// [INPUT]  From: UDCInteractionComponent — player initiates interaction
// [OUTPUT] To:   UDCDialogueComponent::StartConversation() if NPC has dialogue
// [OUTPUT] To:   UDCVendorComponent::OpenVendorSession() if NPC is a vendor
// [OUTPUT] To:   UDCQuestGiverComponent::PresentAvailableQuests() if NPC is quest giver
void ADCNPCBase::OnInteracted_Implementation(APawn* Interactor) { ... }
```

---

### Layer 4 — Secondary Systems

**Prerequisites:** All of Layer 3 core systems the secondary system depends on must exist.
**Each system here is independent — generate only what the game concept needs.**

#### 4A. Loot System
```
Depends on: Inventory foundation, Data definition layer, Interaction system
Connects to: Enemy death logic, containers, rarity tables
```

#### 4B. Equipment / Gear System
```
Depends on: Inventory foundation, Player framework, Data definition layer
Connects to: Stat modifiers, visual mesh swaps, durability
```

#### 4C. Reputation / Faction System
```
Depends on: Data definition layer, Quest system, Dialogue system, Trading system
Connects to: World-state responses, combat hostility, access gating, pricing modifiers
Note: Almost always a mid-pass system. Never scaffold this before quests and dialogue work.
```

#### 4D. Save / Load System
```
Depends on: STABLE definitions of inventory, quest state, player progression, world-state flags
Note: A stub can exist from Layer 1. Real implementation waits until state model is stable.
      Premature serialization of unstable structs causes cascading save corruption bugs.
```

#### 4E. Character Progression System
```
Depends on: Player stats foundation, Data definition layer, Inventory/equipment, gameplay event hooks
Variants: level-based, skill-based, class-based, hybrid
Note: If GURPS-like or deep simulationist — treat as a major subsystem, not a minor add-on.
```

---

### Layer 5 — Advanced / Expansion Systems

**Prerequisites:** Core playable loop confirmed working end-to-end.
**Do not generate these until Layers 1–3 are stable.**

| System | Key Dependencies |
|---|---|
| Animation Integration | Weapon system, interaction framework, skeleton setup |
| Audio / VFX | Stable gameplay events exist to hook into |
| AI Behavior (full) | NPC framework, combat system, perception, navigation |
| Weather / Time of Day | World structure defined, not a default inclusion |
| Character Customization | Character data model, UI foundation, save/load support |
| Crafting | Inventory, data layer, UI foundation |

---

## Guardrails — What DeltaCode Must Never Do

These are hard constraints, not suggestions:

❌ Generate advanced systems before their prerequisites exist
❌ Make animation a dependency for weapon fire logic in greybox mode
❌ Put system state logic inside UI widgets (widgets display state — they don't own it)
❌ Hard-code quest flow only inside dialogue nodes
❌ Let each system invent its own data conventions (all data flows through Layer 2B)
❌ Generate cross-coupled manager classes that own everything
❌ Treat reference games (Skyrim, RDR2, Destiny) as fixed templates to copy verbatim
❌ Generate all possible systems from a single vague prompt
❌ Auto-proceed from concept to code without user confirmation of the structured spec

---

## What DeltaCode Must Always Do

✅ Surface dependencies before generation begins
✅ Warn explicitly if a requested system has unmet prerequisites
✅ Auto-offer to generate missing foundations before the requested system
✅ Keep systems modular — each system can be removed without destroying others
✅ Generate deliberate extension points (delegates, interfaces, virtual functions)
✅ Add [INPUT] / [OUTPUT] / [EVENT] / [DEPENDS ON] comments at every data flow boundary
✅ Confirm the Layer 0 concept spec with the user before writing Layer 1 code
✅ Generate one system at a time in Layer 3, confirm it compiles before the next

---

## Complete Dependency Chain (Quick Reference)

```
Concept Resolution
        ↓
Project Skeleton (GameMode, Controller, Character, HUD, Interfaces, Structs)
        ↓
Interaction System ──────────────────────────────┐
Data Definition Layer ───────────────────────────┤
UI Foundation ───────────────────────────────────┤
Player Framework (from skeleton) ────────────────┘
        ↓
Inventory Foundation ← depends on all four above
        ↓
Weapon System     ← inventory + data + player + UI
Dialogue System   ← interaction + UI + data  (state on NPC, not player)
Quest System      ← data + UI + interaction  (owned by GameInstance subsystem)
Trading System    ← inventory + data + interaction + UI
NPC Framework     ← interaction + dialogue/quest hooks + data
        ↓
Loot, Equipment, Reputation, Save/Load, Progression
        ↓
Animation, AI, Audio/VFX, Weather, Customization, Crafting
```

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.7+
Sources: DeltaCode design document, Epic UE5 subsystem docs, Lyra architecture analysis
Plugin: DeltaCode
Last Updated: 2026-03

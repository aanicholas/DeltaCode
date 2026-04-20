# DeltaCode Technical Reference

Version 1.0.0 | UE5.5+ (Targeting UE5.7)

---

## 1. OVERVIEW

### What DeltaCode Is

DeltaCode is an Unreal Engine 5 editor plugin that uses the Anthropic Claude API to generate C++ source files, Blueprint scaffolding data, and complete playable levels from natural-language prompts. It ships as a two-module plugin (Runtime + Editor) that integrates directly into the UE5 level editor toolbar and Project Settings.

### Two Modes

**Safe Mode** — Additive only. Generates new C++ files and Blueprint stubs into allowed directories. Never deletes, replaces, or modifies existing level actors. Protected paths prevent overwriting core assets.

**Danger Zone** — Full level authoring. Can clear and rebuild entire levels via a Python scripting pipeline. Always shows a confirmation modal before destructive actions. Supports mission templates (Extraction, Destiny, Fallout, Open World RPG).

### Architecture Summary

```
DeltaCode.uplugin
├── DeltaCode          (Runtime module, LoadingPhase: Default)
│   ├── Actors/        Character hierarchy, AI, weapons, pickups, HUD
│   ├── Components/    Health, inventory, equipment, skills, dialogue, vendor
│   ├── Subsystems/    Item registry, quest tracking, reputation
│   ├── Data/          Data asset definitions (items, weapons, quests, etc.)
│   ├── SaveSystem/    Slot-based save/load with per-component capture
│   ├── UI/            UMG widget base classes (HUD, menus, notifications)
│   ├── AI/            Behavior tree tasks, blackboard keys
│   ├── GAS/           Gameplay Ability System attribute set
│   ├── Types/         Core enums, structs, gameplay tags
│   ├── Input/         Enhanced Input config data asset
│   └── Interfaces/    IDCInteractable, IDCDamageable
│
├── DeltaCodeEditor    (Editor module, LoadingPhase: PostEngineInit)
│   ├── Settings/      UDeltaCodeSettings (Project Settings page)
│   ├── API/           Anthropic HTTP client and key validator
│   ├── Prompts/       System prompt builder per mode/template
│   ├── Advisor/       Response parser, code block extraction, file writer
│   ├── Python/        Danger Zone level scripting bridge
│   └── UI/            Slate panel, toolbar commands, plugin style
│
├── Content/Python/    dc_danger_zone.py (level generation script)
├── Resources/         Icon40.png (toolbar icon)
└── Config/            Plugin default configuration
```

### Plugin Dependencies

| Plugin | Reason |
|--------|--------|
| EnhancedInput | Input action/mapping context system |
| GameplayAbilities | GAS attribute set and ability system |
| PythonScriptPlugin | Danger Zone level scripting pipeline |

### Module Dependencies

**Runtime (DeltaCode):**
- Public: Core, CoreUObject, Engine, EnhancedInput, GameplayAbilities, GameplayTags, GameplayTasks, UMG, InputCore, AIModule
- Private: Slate, SlateCore, AssetRegistry, NavigationSystem

**Editor (DeltaCodeEditor):**
- Public: Core, CoreUObject, Engine, DeltaCode
- Private: UnrealEd, Slate, SlateCore, InputCore, EditorSubsystem, ToolMenus, HTTP, Json, JsonUtilities, LevelEditor, DeveloperSettings, PythonScriptPlugin, Projects

---

## 2. COMPLETE CLASS REFERENCE

### 2.1 Runtime Module — Types & Core

---

#### EDCCameraMode
**Module:** Runtime | **File:** Types/DCCoreTypes.h

```
ThirdPerson, FirstPerson, DualPerspective
```

---

#### EDCItemType
**Module:** Runtime | **File:** Types/DCCoreTypes.h

```
Weapon, Armor, Consumable, QuestItem, Ammo, Currency, Miscellaneous
```

---

#### EDCItemRarity
**Module:** Runtime | **File:** Types/DCCoreTypes.h

```
Common, Uncommon, Rare, Epic, Legendary
```

---

#### EDCEquipmentSlot
**Module:** Runtime | **File:** Types/DCCoreTypes.h

```
Weapon, Helmet, Chest, Legs, Hands, Feet, Accessory1, Accessory2
```

---

#### FDCStatBlock
**Module:** Runtime | **File:** Types/DCCoreTypes.h

Stat container attached to characters.

| Property | Type |
|----------|------|
| MaxHealth | float |
| MaxShield | float |
| MoveSpeed | float |
| AttackPower | float |
| Defense | float |
| StatTags | FGameplayTagContainer |

---

#### FDCStatModifier
**Module:** Runtime | **File:** Types/DCCoreTypes.h

Additive stat bonuses applied by equipment.

| Property | Type |
|----------|------|
| MaxHealthBonus | float |
| MaxStaminaBonus | float |
| AttackPowerBonus | float |
| DefenseBonus | float |

---

#### FDCItemHandle
**Module:** Runtime | **File:** Types/DCCoreTypes.h

Lightweight item reference (ID only, no asset pointer).

| Property | Type |
|----------|------|
| ItemID | FName |

Methods: `IsValid()`, `operator==`

---

#### FDCInventoryEntry
**Module:** Runtime | **File:** Types/DCCoreTypes.h

One slot in an inventory.

| Property | Type |
|----------|------|
| ItemHandle | FDCItemHandle |
| StackCount | int32 |
| Durability | float |

---

#### DCGameplayTags
**Module:** Runtime | **File:** Types/DCGameplayTags.h

Native gameplay tag declarations (UE_DEFINE_GAMEPLAY_TAG_COMMENT).

| Tag | Purpose |
|-----|---------|
| Input_Move | Movement input |
| Input_Look | Camera input |
| Input_Jump | Jump input |
| Input_Interact | Interaction input |
| Input_Fire | Weapon fire input |
| Input_Reload | Weapon reload input |
| Input_TogglePerspective | Camera perspective swap |
| Input_Pause | Pause menu toggle |
| UI_Prompt_Interact | Interaction prompt display |
| UI_Notification_General | General HUD notification |
| Interaction_Pickup | Pickup interaction channel |
| Interaction_NPC | NPC interaction channel |
| Interaction_Door | Door interaction channel |
| Interaction_Vendor | Vendor interaction channel |
| Interaction_QuestGiver | Quest giver interaction channel |

---

#### FDCDialogueNode
**Module:** Runtime | **File:** Types/DCDataDefinitions.h

One node in a dialogue tree.

| Property | Type |
|----------|------|
| NodeID | FName |
| SpeakerText | FText |
| Responses | TArray\<FDCDialogueResponse\> |
| ConditionTags | FGameplayTagContainer |
| ExecutionTags | FGameplayTagContainer |

---

#### FDCDialogueResponse
**Module:** Runtime | **File:** Types/DCDataDefinitions.h

| Property | Type |
|----------|------|
| ResponseText | FText |
| NextNodeID | FName |
| RequiredTags | FGameplayTagContainer |
| GrantedTags | FGameplayTagContainer |
| RequiredItem | FDCItemHandle |

---

#### FDCEconomyEntry
**Module:** Runtime | **File:** Types/DCDataDefinitions.h

| Property | Type |
|----------|------|
| ItemHandle | FDCItemHandle |
| Price | int32 |
| CurrencyType | FName |
| StockQuantity | int32 |
| RestockIntervalSeconds | float |

---

#### FDCQuestObjective
**Module:** Runtime | **File:** Types/DCDataDefinitions.h

| Property | Type |
|----------|------|
| ObjectiveID | FName |
| DisplayText | FText |
| ObjectiveType | EDCQuestObjectiveType |
| RequiredCount | int32 |
| TargetItem | FDCItemHandle |
| TargetClass | TSoftClassPtr\<AActor\> |
| CompletionTag | FGameplayTag |
| bHiddenUntilUnlocked | bool |

EDCQuestObjectiveType: `ReachLocation, KillTarget, CollectItem, TalkToNPC, InteractWithObject, Custom`

---

#### FDCQuestReward
**Module:** Runtime | **File:** Types/DCDataDefinitions.h

| Property | Type |
|----------|------|
| RewardItem | FDCItemHandle |
| RewardItemCount | int32 |
| ExperiencePoints | int32 |
| CurrencyAmount | int32 |
| CurrencyType | FName |
| GrantedTags | FGameplayTagContainer |

---

#### UDCInputConfig
**Module:** Runtime | **File:** Input/DCInputConfig.h

**Purpose:** Data asset mapping gameplay tags to Enhanced Input actions.

| UPROPERTY | Type |
|-----------|------|
| DefaultMappingContext | TObjectPtr\<UInputMappingContext\> |
| ActionBindings | TArray\<FDCInputActionBinding\> |

| Method | Returns |
|--------|---------|
| FindActionByTag(FGameplayTag) | const UInputAction* |

- **[INPUT]** From: Project configuration (Content/DeltaCode/ data assets)
- **[OUTPUT]** To: ADCPlayerControllerBase (input binding on possess)

---

#### UDCAttributeSet
**Module:** Runtime | **File:** GAS/DCAttributeSet.h

**Purpose:** GAS attribute set for stamina, attack power, defense, experience, and level.

| Attribute | Type |
|-----------|------|
| Stamina | FGameplayAttributeData |
| MaxStamina | FGameplayAttributeData |
| AttackPower | FGameplayAttributeData |
| Defense | FGameplayAttributeData |
| Experience | FGameplayAttributeData |
| Level | FGameplayAttributeData |

Overrides: `PreAttributeChange`, `PostGameplayEffectExecute`

- **[INPUT]** From: Gameplay effects, DCProgressionComponent level-ups
- **[OUTPUT]** To: Combat calculations, UI display
- **Dependencies:** GameplayAbilities plugin

---

### 2.2 Runtime Module — Interfaces

---

#### IDCInteractable
**Module:** Runtime | **File:** Interfaces/DCInteractable.h

**Purpose:** Interface for any actor that can be interacted with by the player.

| Method | Returns | Parameters |
|--------|---------|------------|
| OnInteracted | void | APawn* Interactor |
| GetInteractionPrompt | FText | — |
| CanInteract | bool | APawn* Interactor |

Implementors: ADCNPCBase, ADCPickupBase

---

#### IDCDamageable
**Module:** Runtime | **File:** Interfaces/DCDamageable.h

**Purpose:** Interface for any actor that can receive damage.

| Method | Returns | Parameters |
|--------|---------|------------|
| ApplyDamage | void | float DamageAmount, AActor* DamageSource, AActor* DamageCauser |
| IsDead | bool | — |

Implementors: ADCCharacterBase

---

### 2.3 Runtime Module — Components

---

#### UDCHealthComponent
**Module:** Runtime | **File:** Components/DCHealthComponent.h

**Purpose:** Manages health and shield for any actor, with death/revive lifecycle.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| DefaultMaxHealth | float | 100.0 |
| DefaultMaxShield | float | 50.0 |
| bShieldEnabled | bool | false |
| ShieldRegenRate | float | 10.0 |
| ShieldRegenDelay | float | 3.0 |

| Method | Returns | Parameters |
|--------|---------|------------|
| ApplyDamage | void | float Amount, AActor* Source, AActor* Causer |
| Heal | void | float Amount |
| Kill | void | — |
| Revive | void | float HealthPercent |
| AdjustMaxHealth | void | float NewMax |
| GetCurrentHealth | float | — |
| GetMaxHealth | float | — |
| GetHealthPercent | float | — |
| GetCurrentShield | float | — |
| GetMaxShield | float | — |
| IsDead | bool | — |

| Delegate | Signature |
|----------|-----------|
| FDCOnHealthChanged | float NewHealth, float MaxHealth, float Delta |
| FDCOnDied | AActor* Killer |
| FDCOnRevived | — |

- **[INPUT]** From: Combat system, environment hazards
- **[OUTPUT]** To: HUD health bars, death/respawn logic, DCEnemyBase loot drops
- **Dependencies:** None (self-contained)

---

#### UDCInventoryComponent
**Module:** Runtime | **File:** Components/DCInventoryComponent.h

**Purpose:** Manages item storage with weight limits, stacking rules, and save/load.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| MaxSlots | int32 | 20 |
| MaxWeight | float | 100.0 |

| Method | Returns | Parameters |
|--------|---------|------------|
| AddItem | EDCInventoryResult | FDCItemHandle, int32 Quantity |
| RemoveItem | EDCInventoryResult | FDCItemHandle, int32 Quantity |
| DropItem | EDCInventoryResult | FDCItemHandle, int32 Quantity |
| UseItem | EDCInventoryResult | FDCItemHandle |
| ClearAll | void | — |
| RestoreFromSave | void | TArray\<FDCInventoryEntry\> |
| HasItem | bool | FDCItemHandle, int32 MinQuantity |
| GetItemCount | int32 | FDCItemHandle |
| GetNumSlotsUsed | int32 | — |
| GetCurrentWeight | float | — |
| GetEntries | TArray\<FDCInventoryEntry\> | — |
| ResolveDefinition | UDCItemDefinition* | FDCItemHandle |

| Delegate | Signature |
|----------|-----------|
| FDCOnInventoryChanged | — |
| FDCOnItemAdded | FDCItemHandle, int32 Quantity |
| FDCOnItemRemoved | FDCItemHandle, int32 Quantity |

- **[INPUT]** From: Pickup actors, vendor transactions, quest rewards
- **[OUTPUT]** To: Equipment component, vendor component, save game, UI
- **Dependencies:** UDCItemDefinitionRegistry (resolves handles to definitions)

---

#### UDCEquipmentComponent
**Module:** Runtime | **File:** Components/DCEquipmentComponent.h

**Purpose:** Manages equipped items across 8 slots with stat modifiers, durability, and weapon spawning.

| Method | Returns | Parameters |
|--------|---------|------------|
| EquipItem | bool | EDCEquipmentSlot, FDCItemHandle |
| UnequipSlot | bool | EDCEquipmentSlot |
| IsSlotOccupied | bool | EDCEquipmentSlot |
| GetEquippedHandle | FDCItemHandle | EDCEquipmentSlot |
| GetSlotState | FDCEquippedSlot | EDCEquipmentSlot |
| DamageDurability | void | EDCEquipmentSlot, float Amount |
| RepairSlot | void | EDCEquipmentSlot, float Amount |
| IsSlotBroken | bool | EDCEquipmentSlot |
| EquipWeapon | void | FDCItemHandle |
| UnequipWeapon | void | — |
| FireEquipped | void | — |
| ReloadEquipped | void | — |
| GetEquippedWeapon | ADCWeaponBase* | — |
| HasEquippedWeapon | bool | — |
| CaptureForSave | TArray\<FDCSavedEquipmentSlot\> | — |
| RestoreFromSave | void | TArray\<FDCSavedEquipmentSlot\> |

| Delegate | Signature |
|----------|-----------|
| FDCOnWeaponEquipped | ADCWeaponBase* |
| FDCOnWeaponUnequipped | — |
| FDCOnEquipmentChanged | EDCEquipmentSlot |
| FDCOnDurabilityChanged | EDCEquipmentSlot, float Current, float Max |

- **[INPUT]** From: Inventory component, save game
- **[OUTPUT]** To: Character stat modifiers, weapon actors, save game
- **Dependencies:** UDCItemDefinitionRegistry, UDCInventoryComponent

---

#### UDCProgressionComponent
**Module:** Runtime | **File:** Components/DCProgressionComponent.h

**Purpose:** Tracks XP, levels, and skill point allocation with a configurable level curve.

| UPROPERTY | Type |
|-----------|------|
| LevelCurve | TSoftObjectPtr\<UDCLevelCurve\> |
| StartingLevel | int32 |

| Method | Returns | Parameters |
|--------|---------|------------|
| GrantXP | void | int32 Amount |
| SetLevelDirect | void | int32 NewLevel |
| SpendSkillPoints | bool | int32 Cost |
| GetCurrentLevel | int32 | — |
| GetCurrentXP | int32 | — |
| GetUnspentSkillPoints | int32 | — |
| GetXPForNextLevel | int32 | — |
| GetXPToNextLevel | int32 | — |
| CaptureForSave | void | int32& Level, int32& XP, int32& SP |
| RestoreFromSave | void | int32 Level, int32 XP, int32 SP |

| Delegate | Signature |
|----------|-----------|
| FDCOnExperienceGained | int32 Amount, int32 NewTotal |
| FDCOnLeveledUp | int32 NewLevel, FDCLevelUpBonus Bonus |
| FDCOnSkillPointsChanged | int32 Available |

- **[INPUT]** From: Quest rewards, combat kills, save game
- **[OUTPUT]** To: Attribute set (stat bonuses), skill tree (level gating), save game
- **Dependencies:** UDCLevelCurve data asset

---

#### UDCSkillTreeComponent
**Module:** Runtime | **File:** Components/DCSkillTreeComponent.h

**Purpose:** Manages skill unlock state against a skill tree definition with prerequisite checking.

| UPROPERTY | Type |
|-----------|------|
| SkillTree | TSoftObjectPtr\<UDCSkillTreeDefinition\> |

| Method | Returns | Parameters |
|--------|---------|------------|
| UnlockSkill | EDCSkillUnlockResult | FName SkillID |
| IsUnlocked | bool | FName SkillID |
| CheckCanUnlock | EDCSkillUnlockResult | FName SkillID |
| GetUnlockedSkills | TSet\<FName\> | — |
| GetUnlockedCount | int32 | — |
| CaptureForSave | TArray\<FName\> | — |
| RestoreFromSave | void | TArray\<FName\> |

EDCSkillUnlockResult: `Success, Failed_NotFound, Failed_AlreadyUnlocked, Failed_PrerequisitesNotMet, Failed_LevelTooLow, Failed_NotEnoughSkillPoints`

- **[INPUT]** From: Player UI, save game
- **[OUTPUT]** To: Character stat modifiers (via skill definitions), save game
- **Dependencies:** UDCSkillTreeDefinition, UDCProgressionComponent (level + skill points)

---

#### UDCInteractionComponent
**Module:** Runtime | **File:** Components/DCInteractionComponent.h

**Purpose:** Detects interactable actors via line trace or overlap, manages focus target.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| DetectionMode | EDCInteractionDetectionMode | LineTraceFromCamera |
| TraceDistance | float | 300.0 |
| OverlapRadius | float | 200.0 |
| ScanInterval | float | 0.1 |
| TraceChannel | ECollisionChannel | ECC_Visibility |
| bDrawDebug | bool | false |

| Method | Returns | Parameters |
|--------|---------|------------|
| TryInteract | bool | — |
| GetFocusedTarget | AActor* | — |
| HasFocusedTarget | bool | — |

| Delegate | Signature |
|----------|-----------|
| FDCOnInteractionFocusChanged | AActor* NewFocus |
| FDCOnInteracted | AActor* Target |

- **[INPUT]** From: Player input (Interact action), timer-driven scan
- **[OUTPUT]** To: Focused actor's IDCInteractable interface, HUD prompt
- **Dependencies:** IDCInteractable interface on target actors

---

#### UDCDialogueComponent
**Module:** Runtime | **File:** Components/DCDialogueComponent.h

**Purpose:** State machine for dialogue tree traversal on NPCs.

| UPROPERTY | Type |
|-----------|------|
| DialogueWidgetClass | TSubclassOf\<UUserWidget\> |

| Method | Returns | Parameters |
|--------|---------|------------|
| SetNPCDefinition | void | UDCNPCDefinition* |
| GetNPCDefinition | UDCNPCDefinition* | — |
| StartConversation | void | APawn* Initiator |
| EndConversation | void | — |
| ChooseResponse | void | int32 Index |
| IsConversationActive | bool | — |
| GetCurrentNodeID | FName | — |
| GetCurrentNode | FDCDialogueNode | — |
| GetAvailableResponses | TArray\<FDCDialogueResponse\> | — |
| GetInitiator | APawn* | — |

| Delegate | Signature |
|----------|-----------|
| FDCOnConversationStarted | APawn* Initiator |
| FDCOnDialogueNodeEntered | FName NodeID |
| FDCOnConversationEnded | — |

- **[INPUT]** From: NPC interaction, UDCNPCDefinition (dialogue data)
- **[OUTPUT]** To: Narrative state (tag grants), quest subsystem (accept quests), UI
- **Dependencies:** UDCNPCDefinition, UDCNarrativeStateComponent (tag checks)

---

#### UDCVendorComponent
**Module:** Runtime | **File:** Components/DCVendorComponent.h

**Purpose:** Runtime shop with stock mutation, buy/sell transactions, and restock timers.

| UPROPERTY | Type |
|-----------|------|
| VendorWidgetClass | TSubclassOf\<UUserWidget\> |

| Method | Returns | Parameters |
|--------|---------|------------|
| SetVendorDefinition | void | UDCVendorDefinition* |
| GetVendorDefinition | UDCVendorDefinition* | — |
| OpenShop | void | APawn* Customer |
| CloseShop | void | — |
| IsShopOpen | bool | — |
| GetCurrentCustomer | APawn* | — |
| GetStock | TArray\<FDCEconomyEntry\> | — |
| GetBuybackPrice | int32 | FDCItemHandle |
| TryBuy | EDCVendorResult | FDCItemHandle, int32 Quantity |
| TrySell | EDCVendorResult | FDCItemHandle, int32 Quantity |
| RestockNow | void | — |

EDCVendorResult: `Success, Failed_InsufficientFunds, Failed_InsufficientStock, Failed_InventoryFull, Failed_ItemNotAccepted, Failed_ShopClosed`

| Delegate | Signature |
|----------|-----------|
| FDCOnVendorStockChanged | — |
| FDCOnPurchaseCompleted | FDCItemHandle, int32 |
| FDCOnSaleCompleted | FDCItemHandle, int32 |
| FDCOnShopOpened | APawn* |
| FDCOnShopClosed | — |

- **[INPUT]** From: NPC interaction, UDCVendorDefinition (stock/pricing)
- **[OUTPUT]** To: Inventory component (item transfer), currency system
- **Dependencies:** UDCVendorDefinition, UDCInventoryComponent (buyer), UDCItemDefinitionRegistry

---

#### UDCFactionComponent
**Module:** Runtime | **File:** Components/DCFactionComponent.h

**Purpose:** Assigns faction identity to an actor for team-based AI perception and reputation.

| UPROPERTY | Type |
|-----------|------|
| FactionAsset | TSoftObjectPtr\<UDCFactionDefinition\> |

| Method | Returns | Parameters |
|--------|---------|------------|
| SetFaction | void | UDCFactionDefinition* |
| GetFaction | UDCFactionDefinition* | — |
| GetFactionTag | FGameplayTag | — |
| GetFactionID | FName | — |
| GetTeamID | uint8 | — |
| GetStanceToward | EDCFactionStance | UDCFactionComponent* Other |

- **[INPUT]** From: Actor configuration, UDCFactionDefinition
- **[OUTPUT]** To: AI perception (team filtering), reputation subsystem
- **Dependencies:** UDCFactionDefinition, UDCReputationSubsystem (player stance)

---

#### UDCNarrativeStateComponent
**Module:** Runtime | **File:** Components/DCNarrativeStateComponent.h

**Purpose:** Persistent gameplay flag system using gameplay tags. Gates dialogue, quests, and world state.

| Method | Returns | Parameters |
|--------|---------|------------|
| AddTag | void | FGameplayTag |
| AddTags | void | FGameplayTagContainer |
| RemoveTag | void | FGameplayTag |
| RemoveTags | void | FGameplayTagContainer |
| HasTag | bool | FGameplayTag |
| HasAllTags | bool | FGameplayTagContainer |
| HasAnyTag | bool | FGameplayTagContainer |
| GetTags | FGameplayTagContainer | — |
| SetTagsFromSave | void | FGameplayTagContainer |

| Delegate | Signature |
|----------|-----------|
| FDCOnNarrativeTagsChanged | FGameplayTagContainer NewTags |

- **[INPUT]** From: Dialogue choices, quest completion, save game
- **[OUTPUT]** To: Dialogue gating, quest prerequisites, world state checks
- **Dependencies:** None (self-contained tag store)

---

### 2.4 Runtime Module — Actors

---

#### ADCCharacterBase
**Module:** Runtime | **File:** Actors/DCCharacterBase.h

**Purpose:** Base character with GAS integration, stats, and damage interface.

Implements: `IAbilitySystemInterface`, `IDCDamageable`

| UPROPERTY | Type |
|-----------|------|
| BaseStats | FDCStatBlock |
| DefaultMappingContext | TObjectPtr\<UInputMappingContext\> |
| MoveAction | TObjectPtr\<UInputAction\> |
| LookAction | TObjectPtr\<UInputAction\> |
| JumpAction | TObjectPtr\<UInputAction\> |

| Component | Type |
|-----------|------|
| AbilitySystemComponent | UAbilitySystemComponent* |
| EquipmentComponent | UDCEquipmentComponent* |
| HealthComponent | UDCHealthComponent* |
| AttributeSet | UDCAttributeSet* |

- **[INPUT]** From: Enhanced Input, combat system
- **[OUTPUT]** To: Derived character classes, movement system
- **Dependencies:** GameplayAbilities plugin, EnhancedInput plugin

---

#### ADCThirdPersonCharacter
**Module:** Runtime | **File:** Actors/DCThirdPersonCharacter.h

**Purpose:** Third-person character with spring arm camera.

Extends: ADCCharacterBase

| Component | Type |
|-----------|------|
| CameraBoom | USpringArmComponent* |
| FollowCamera | UCameraComponent* |

| UPROPERTY | Type | Default |
|-----------|------|---------|
| DefaultCameraArmLength | float | 300.0 |

---

#### ADCFirstPersonCharacter
**Module:** Runtime | **File:** Actors/DCFirstPersonCharacter.h

**Purpose:** First-person character with attached camera and separate first-person mesh.

Extends: ADCCharacterBase

| Component | Type |
|-----------|------|
| FirstPersonCamera | UCameraComponent* |
| FirstPersonMesh | USkeletalMeshComponent* |

| UPROPERTY | Type |
|-----------|------|
| CameraOffset | FVector |

---

#### ADCDualPerspectiveCharacter
**Module:** Runtime | **File:** Actors/DCDualPerspectiveCharacter.h

**Purpose:** Character that can switch between first-person and third-person at runtime.

Extends: ADCThirdPersonCharacter

| UPROPERTY | Type | Default |
|-----------|------|---------|
| InitialCameraMode | EDCCameraMode | ThirdPerson |
| bAllowPlayerPerspectiveToggle | bool | true |
| PerspectiveBlendTime | float | 0.3 |
| TogglePerspectiveAction | TObjectPtr\<UInputAction\> | — |

| Method | Returns | Parameters |
|--------|---------|------------|
| TogglePerspective | void | — |
| SetPerspective | void | EDCCameraMode |
| GetCurrentPerspective | EDCCameraMode | — |
| GetFirstPersonCamera | UCameraComponent* | — |

| Delegate | Signature |
|----------|-----------|
| FDCOnPerspectiveChanged | EDCCameraMode NewMode |

- **[INPUT]** From: Player input (TogglePerspective), configuration
- **[OUTPUT]** To: Camera system, first-person mesh visibility

---

#### ADCPlayerControllerBase
**Module:** Runtime | **File:** Actors/DCPlayerControllerBase.h

**Purpose:** Player controller with all gameplay components and input routing.

Implements: `IGenericTeamAgentInterface`

| Component | Type |
|-----------|------|
| InteractionComponent | UDCInteractionComponent* |
| InventoryComponent | UDCInventoryComponent* |
| NarrativeState | UDCNarrativeStateComponent* |
| ProgressionComponent | UDCProgressionComponent* |
| SkillTreeComponent | UDCSkillTreeComponent* |

| UPROPERTY | Type |
|-----------|------|
| InputConfig | TSoftObjectPtr\<UDCInputConfig\> |
| PauseMenuClass | TSubclassOf\<UDCPauseMenuWidget\> |

- **[INPUT]** From: Player input, Enhanced Input system
- **[OUTPUT]** To: All owned components, HUD, pause menu
- **Dependencies:** UDCInputConfig, all listed components

---

#### ADCHUDBase
**Module:** Runtime | **File:** Actors/DCHUDBase.h

**Purpose:** HUD with widget stack for menus and notification system.

| Method | Returns | Parameters |
|--------|---------|------------|
| PushWidget | void | UUserWidget* |
| PopWidget | void | — |
| ShowNotification | void | FText Message |
| GetHUDRootWidget | UDCHUDRootWidget* | — |
| HasActiveWidgets | bool | — |

- **[INPUT]** From: Player controller, gameplay events
- **[OUTPUT]** To: UMG widget display, input mode switching

---

#### ADCGameModeBase
**Module:** Runtime | **File:** Actors/DCGameModeBase.h

**Purpose:** Sets default player controller, pawn, and HUD classes for DeltaCode levels.

---

#### ADCGameInstance
**Module:** Runtime | **File:** Actors/DCGameInstance.h

**Purpose:** Slot-based save/load system with per-component capture and metadata.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| SaveSlotPrefix | FString | "DCSave" |
| MaxSaveSlots | int32 | 10 |
| SaveUserIndex | int32 | 0 |
| DefaultAutoLoadSlot | int32 | -1 |

| Method | Returns | Parameters |
|--------|---------|------------|
| GetSaveGame | UDCSaveGame* | — |
| GetCurrentSlotIndex | int32 | — |
| SaveGameToDisk | bool | int32 SlotIndex |
| LoadGameFromDisk | bool | int32 SlotIndex |
| NewGameInSlot | void | int32 SlotIndex |
| LoadSlot | void | int32 SlotIndex |
| DeleteSlot | bool | int32 SlotIndex |
| EnumerateSlots | TArray\<FDCSaveSlotInfo\> | — |
| GetSlotInfo | FDCSaveSlotInfo | int32 SlotIndex |
| CaptureFromPlayer | void | — |
| ApplySaveToPlayer | void | — |

| Delegate | Signature |
|----------|-----------|
| FDCOnSaveCompleted | int32 SlotIndex, bool bSuccess |
| FDCOnLoadCompleted | int32 SlotIndex, bool bSuccess |

- **[INPUT]** From: Pause menu save/load, checkpoint volumes, auto-save
- **[OUTPUT]** To: All player components (restore), UDCSaveGame (capture)
- **Dependencies:** UDCSaveGame, ADCPlayerControllerBase (component access)

---

#### ADCNPCBase
**Module:** Runtime | **File:** Actors/DCNPCBase.h

**Purpose:** Non-player character with dialogue and interaction support.

Implements: `IDCInteractable`

| UPROPERTY | Type |
|-----------|------|
| NPCDefinitionAsset | TSoftObjectPtr\<UDCNPCDefinition\> |

| Component | Type |
|-----------|------|
| DialogueComponent | UDCDialogueComponent* |

- **[INPUT]** From: Player interaction, UDCNPCDefinition
- **[OUTPUT]** To: Dialogue system, quest system (via NPC role)
- **Dependencies:** UDCNPCDefinition

---

#### ADCEnemyBase
**Module:** Runtime | **File:** Actors/DCEnemyBase.h

**Purpose:** Base enemy actor with loot drops, behavior tree, and death cleanup.

| UPROPERTY | Type |
|-----------|------|
| LootTable | TArray\<FDCLootDrop\> |
| SharedLootTable | TSoftObjectPtr\<UDCLootTableDefinition\> |
| BehaviorTree | TSoftObjectPtr\<UBehaviorTree\> |
| CorpseLifetime | float |
| LootPickupClass | TSubclassOf\<ADCPickupBase\> |

| Event (BlueprintImplementableEvent) | Parameters |
|--------------------------------------|------------|
| OnHitReact | float Damage, AActor* Source |
| OnDeathFX | — |

- **[INPUT]** From: Combat damage, AI controller
- **[OUTPUT]** To: Loot pickup spawning, quest subsystem (kill notifications)
- **Dependencies:** ADCEnemyAIController, UDCHealthComponent

---

#### ADCEnemyAIController
**Module:** Runtime | **File:** Actors/DCEnemyAIController.h

**Purpose:** AI controller with perception, behavior tree, and team affiliation.

Implements: `IGenericTeamAgentInterface`

| UPROPERTY | Type | Default |
|-----------|------|---------|
| SightRadius | float | 1500.0 |
| LoseSightRadius | float | 2000.0 |
| PeripheralVisionAngle | float | 60.0 |

| Component | Type |
|-----------|------|
| DCPerceptionComponent | UAIPerceptionComponent* |
| BehaviorTreeComponent | UBehaviorTreeComponent* |
| BlackboardComponent | UBlackboardComponent* |

| Method | Returns | Parameters |
|--------|---------|------------|
| GetCurrentTarget | AActor* | — |
| HasTarget | bool | — |
| GetGenericTeamId | FGenericTeamId | — |
| SetGenericTeamId | void | FGenericTeamId |
| GetTeamAttitudeTowards | ETeamAttitude::Type | const AActor& |

| Delegate | Signature |
|----------|-----------|
| FDCOnTargetPerceived | AActor* Target, bool bSensed |

- **[INPUT]** From: AI perception stimuli, behavior tree
- **[OUTPUT]** To: Blackboard (target actor, last known location), enemy actions
- **Dependencies:** AIModule, behavior tree asset, ADCEnemyBase

---

#### ADCWeaponBase
**Module:** Runtime | **File:** Actors/DCWeaponBase.h

**Purpose:** Weapon actor with fire modes, ammo management, and reload mechanics.

| UPROPERTY | Type |
|-----------|------|
| MuzzleSocketName | FName |

| Method | Returns | Parameters |
|--------|---------|------------|
| Initialize | void | UDCWeaponDefinition* |
| TryFire | bool | — |
| StartReload | void | — |
| GetCurrentMagazine | int32 | — |
| GetReserveAmmo | int32 | — |
| GetDefinition | UDCWeaponDefinition* | — |
| IsReloading | bool | — |
| IsOnCooldown | bool | — |

| Event (BlueprintImplementableEvent) | Parameters |
|--------------------------------------|------------|
| OnFireFX | FVector MuzzleLocation |

| Delegate | Signature |
|----------|-----------|
| FDCOnWeaponFired | — |
| FDCOnWeaponReloadStart | — |
| FDCOnWeaponReloadComplete | — |
| FDCOnAmmoChanged | int32 Magazine, int32 Reserve |

- **[INPUT]** From: Equipment component (equip/fire/reload), weapon definition
- **[OUTPUT]** To: Damage application, FX events, ammo UI
- **Dependencies:** UDCWeaponDefinition

---

#### ADCPickupBase
**Module:** Runtime | **File:** Actors/DCPickupBase.h

**Purpose:** World-placed item pickup with interaction support.

Implements: `IDCInteractable`

| UPROPERTY | Type |
|-----------|------|
| ItemHandle | FDCItemHandle |
| Quantity | int32 |
| InteractionRadius | float |

| Component | Type |
|-----------|------|
| MeshComponent | UStaticMeshComponent* |
| InteractionSphere | USphereComponent* |

| Event (BlueprintImplementableEvent) | Parameters |
|--------------------------------------|------------|
| OnPickedUp | APawn* Collector |

- **[INPUT]** From: Player interaction, enemy loot drops
- **[OUTPUT]** To: Player inventory (item transfer)
- **Dependencies:** UDCItemDefinitionRegistry (mesh lookup)

---

#### ADCCheckpointVolume
**Module:** Runtime | **File:** Actors/DCCheckpointVolume.h

**Purpose:** Trigger volume that initiates a save when the player enters.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| bOneShot | bool | true |
| CooldownSeconds | float | 5.0 |
| CheckpointID | FName | — |

| Component | Type |
|-----------|------|
| Trigger | UBoxComponent* |

| Delegate | Signature |
|----------|-----------|
| FDCOnCheckpointTriggered | APawn* Player, FName CheckpointID |

- **[INPUT]** From: Player overlap
- **[OUTPUT]** To: ADCGameInstance (save trigger)

---

### 2.5 Runtime Module — Subsystems

---

#### UDCItemDefinitionRegistry
**Module:** Runtime | **File:** Subsystems/DCItemDefinitionRegistry.h

**Purpose:** Central registry of all UDCItemDefinition assets, discovered via asset registry.

| Method | Returns | Parameters |
|--------|---------|------------|
| FindItemByHandle | UDCItemDefinition* | FDCItemHandle |
| FindItemByID | UDCItemDefinition* | FName ItemID |
| GetAllItems | TArray\<UDCItemDefinition*\> | — |
| GetAllItemsOfType | TArray\<UDCItemDefinition*\> | EDCItemType |
| IsReady | bool | — |
| RegisterItem | void | UDCItemDefinition* |

| Delegate | Signature |
|----------|-----------|
| FDCOnItemRegistryReady | — |

- **[INPUT]** From: Asset registry scan at initialization
- **[OUTPUT]** To: Every system that resolves FDCItemHandle to a definition
- **Dependencies:** AssetRegistry module

---

#### UDCQuestSubsystem
**Module:** Runtime | **File:** Subsystems/DCQuestSubsystem.h

**Purpose:** Tracks active/completed/failed quests with event-driven objective progression.

| Method | Returns | Parameters |
|--------|---------|------------|
| AcceptQuest | bool | UDCQuestDefinition* |
| CompleteQuest | void | FName QuestID |
| FailQuest | void | FName QuestID |
| AbandonQuest | void | FName QuestID |
| NotifyEnemyKilled | void | TSubclassOf\<AActor\>, int32 Count |
| NotifyNPCTalked | void | FName NPCID |
| NotifyInteracted | void | TSubclassOf\<AActor\> |
| NotifyLocationReached | void | FGameplayTag |
| IsQuestActive | bool | FName QuestID |
| IsQuestCompleted | bool | FName QuestID |
| IsQuestFailed | bool | FName QuestID |
| GetQuestState | EDCQuestState | FName QuestID |
| GetActiveQuests | TArray\<FDCActiveQuestState\> | — |
| CaptureForSave | TArray\<FDCSavedQuestState\> | — |
| RestoreFromSave | void | TArray\<FDCSavedQuestState\> |

| Delegate | Signature |
|----------|-----------|
| FDCOnQuestAccepted | FName QuestID |
| FDCOnObjectiveProgress | FName QuestID, FName ObjectiveID, int32 Current, int32 Required |
| FDCOnObjectiveCompleted | FName QuestID, FName ObjectiveID |
| FDCOnQuestCompleted | FName QuestID |
| FDCOnQuestFailed | FName QuestID |

- **[INPUT]** From: Gameplay events (kills, interactions, locations), dialogue, save game
- **[OUTPUT]** To: Narrative state (completion tags), rewards, UI, reputation subsystem
- **Dependencies:** UDCNarrativeStateComponent (prerequisite tag checks)

---

#### UDCReputationSubsystem
**Module:** Runtime | **File:** Subsystems/DCReputationSubsystem.h

**Purpose:** Faction-based reputation tracking with stance bands (Allied → Hostile).

| Method | Returns | Parameters |
|--------|---------|------------|
| RegisterFaction | void | UDCFactionDefinition* |
| FindFactionByTag | UDCFactionDefinition* | FGameplayTag |
| GetReputation | float | FGameplayTag FactionTag |
| ModifyReputation | void | FGameplayTag FactionTag, float Delta |
| SetReputation | void | FGameplayTag FactionTag, float Value |
| GetPlayerStance | EDCFactionStance | FGameplayTag FactionTag |
| IsPlayerHostileWith | bool | FGameplayTag FactionTag |
| CaptureForSave | TArray\<FDCSavedFactionReputation\> | — |
| RestoreFromSave | void | TArray\<FDCSavedFactionReputation\> |

| Delegate | Signature |
|----------|-----------|
| FDCOnReputationChanged | FGameplayTag Faction, float NewRep, float Delta |
| FDCOnFactionStanceChanged | FGameplayTag Faction, EDCFactionStance NewStance |
| FDCOnQuestCompletedForReputation | FName QuestID |
| FDCOnQuestFailedForReputation | FName QuestID |

- **[INPUT]** From: Quest completion/failure, dialogue choices, combat actions
- **[OUTPUT]** To: AI perception (hostility), dialogue gating, vendor access
- **Dependencies:** UDCFactionDefinition assets

---

### 2.6 Runtime Module — Data Assets

---

#### UDCItemDefinition
**Module:** Runtime | **File:** Data/DCItemDefinition.h

**Purpose:** Base data asset for all items.

| UPROPERTY | Type |
|-----------|------|
| ItemID | FName |
| DisplayName | FText |
| Description | FText |
| ItemType | EDCItemType |
| Rarity | EDCItemRarity |
| MaxStackSize | int32 |
| bIsUnique | bool |
| Weight | float |
| BaseValue | int32 |
| Icon | TSoftObjectPtr\<UTexture2D\> |
| WorldMesh | TSoftObjectPtr\<UStaticMesh\> |
| ItemTags | FGameplayTagContainer |

---

#### UDCEquipmentDefinition
**Module:** Runtime | **File:** Data/DCEquipmentDefinition.h

**Purpose:** Equipment item with slot, stat modifiers, and durability.

Extends: UDCItemDefinition

| UPROPERTY | Type |
|-----------|------|
| EquipmentSlot | EDCEquipmentSlot |
| AttachSocketOverride | FName |
| Modifiers | FDCStatModifier |
| MaxDurability | float |
| DurabilityPerUse | float |
| EquippedSkeletalMesh | TSoftObjectPtr\<USkeletalMesh\> |
| EquippedStaticMesh | TSoftObjectPtr\<UStaticMesh\> |

---

#### UDCWeaponDefinition
**Module:** Runtime | **File:** Data/DCWeaponDefinition.h

**Purpose:** Weapon parameters — damage, fire mode, ammo, and actor class.

Extends: UDCEquipmentDefinition

| UPROPERTY | Type |
|-----------|------|
| Damage | float |
| FireRate | float |
| Range | float |
| FireMode | EDCWeaponFireMode |
| HitMode | EDCWeaponHitMode |
| MagazineSize | int32 |
| ReloadTime | float |
| AmmoType | FName |
| WeaponActorClass | TSubclassOf\<ADCWeaponBase\> |
| WeaponMesh | TSoftObjectPtr\<USkeletalMesh\> |
| FireMontage | TSoftObjectPtr\<UAnimMontage\> |
| ReloadMontage | TSoftObjectPtr\<UAnimMontage\> |

EDCWeaponFireMode: `SemiAuto, FullAuto, Burst, Charged`
EDCWeaponHitMode: `Hitscan, Projectile, Melee`

---

#### UDCSkillDefinition
**Module:** Runtime | **File:** Data/DCSkillDefinition.h

| UPROPERTY | Type |
|-----------|------|
| SkillID | FName |
| DisplayName | FText |
| Description | FText |
| Icon | TSoftObjectPtr\<UTexture2D\> |
| Cost | int32 |
| RequiredPlayerLevel | int32 |
| PrerequisiteSkills | TArray\<FName\> |
| Modifiers | FDCStatModifier |
| GrantedTags | FGameplayTagContainer |

---

#### UDCSkillTreeDefinition
**Module:** Runtime | **File:** Data/DCSkillTreeDefinition.h

| UPROPERTY | Type |
|-----------|------|
| TreeID | FName |
| DisplayName | FText |
| Skills | TArray\<UDCSkillDefinition*\> |

| Method | Returns | Parameters |
|--------|---------|------------|
| FindSkill | UDCSkillDefinition* | FName SkillID |

---

#### UDCQuestDefinition
**Module:** Runtime | **File:** Data/DCQuestDefinition.h

| UPROPERTY | Type |
|-----------|------|
| QuestID | FName |
| DisplayName | FText |
| Description | FText |
| Icon | TSoftObjectPtr\<UTexture2D\> |
| bIsMainQuest | bool |
| RecommendedLevel | int32 |
| PrerequisiteTags | FGameplayTagContainer |
| BlockingTags | FGameplayTagContainer |
| Objectives | TArray\<FDCQuestObjective\> |
| Rewards | TArray\<FDCQuestReward\> |
| CompletionTags | FGameplayTagContainer |
| bCanFail | bool |
| FollowUpQuest | TSoftObjectPtr\<UDCQuestDefinition\> |

---

#### UDCFactionDefinition
**Module:** Runtime | **File:** Data/DCFactionDefinition.h

| UPROPERTY | Type |
|-----------|------|
| FactionID | FName |
| DisplayName | FText |
| Description | FText |
| FactionTag | FGameplayTag |
| TeamID | uint8 |
| DefaultStances | TArray\<FDCFactionDefaultStance\> |
| FallbackStance | EDCFactionStance |
| InitialPlayerReputation | float |
| Min/MaxReputation | float |
| Allied/Friendly/Unfriendly/HostileThreshold | float |

EDCFactionStance: `Allied, Friendly, Neutral, Unfriendly, Hostile`

---

#### UDCLevelCurve
**Module:** Runtime | **File:** Data/DCLevelCurve.h

| UPROPERTY | Type |
|-----------|------|
| Levels | TArray\<FDCLevelUpBonus\> |

FDCLevelUpBonus: Level, XPThreshold, HealthBonus, StaminaBonus, AttackPowerBonus, DefenseBonus, SkillPointsAwarded

---

#### UDCLootTableDefinition
**Module:** Runtime | **File:** Data/DCLootTableDefinition.h

| UPROPERTY | Type |
|-----------|------|
| GuaranteedDrops | TArray\<FDCLootEntry\> |
| WeightedPool | TArray\<FDCWeightedLootEntry\> |
| MinRolls | int32 |
| MaxRolls | int32 |
| bAllowDuplicateWeightedPicks | bool |

| Method | Returns | Parameters |
|--------|---------|------------|
| RollDrops | TArray\<FDCLootDrop\> | — |

---

#### UDCVendorDefinition
**Module:** Runtime | **File:** Data/DCVendorDefinition.h

| UPROPERTY | Type |
|-----------|------|
| VendorID | FName |
| DisplayName | FText |
| CurrencyType | FName |
| BuybackMultiplier | float |
| bAcceptsSales | bool |
| RestrictedItemTypes | TArray\<EDCItemType\> |
| DefaultStock | TArray\<FDCEconomyEntry\> |

---

#### UDCNPCDefinition
**Module:** Runtime | **File:** Data/DCNPCDefinition.h

| UPROPERTY | Type |
|-----------|------|
| NPCID | FName |
| DisplayName | FText |
| Role | EDCNPCRole |
| NPCClass | TSubclassOf\<ADCNPCBase\> |
| NPCMesh | TSoftObjectPtr\<USkeletalMesh\> |
| RootDialogueNodeID | FName |
| DialogueNodes | TArray\<FDCDialogueNode\> |
| VendorDefinition | TSoftObjectPtr\<UDCVendorDefinition\> |
| AvailableQuests | TArray\<TSoftObjectPtr\<UDCQuestDefinition\>\> |
| FactionTags | FGameplayTagContainer |

EDCNPCRole: `Ambient, QuestGiver, Vendor, VendorAndQuests, HostileOnTrigger`

---

### 2.7 Runtime Module — Save System

---

#### UDCSaveGame
**Module:** Runtime | **File:** SaveSystem/DCSaveGame.h

**Purpose:** Serializable save game object holding all player state.

| UPROPERTY | Type | Category |
|-----------|------|----------|
| PlayerName | FString | Metadata |
| TotalPlayTimeSeconds | float | Metadata |
| LastSaveTimestamp | FDateTime | Metadata |
| LevelName | FName | Metadata |
| InventoryEntries | TArray\<FDCInventoryEntry\> | Inventory |
| NarrativeTags | FGameplayTagContainer | Narrative |
| PlayerLevel | int32 | Progression |
| TotalExperience | int32 | Progression |
| UnspentSkillPoints | int32 | Progression |
| UnlockedSkills | TArray\<FName\> | Skills |
| EquippedItems | TArray\<FDCSavedEquipmentSlot\> | Equipment |
| QuestStates | TArray\<FDCSavedQuestState\> | Quests |
| FactionReputations | TArray\<FDCSavedFactionReputation\> | Reputation |

---

#### FDCSaveSlotInfo
**Module:** Runtime | **File:** SaveSystem/DCSaveSlotInfo.h

**Purpose:** Lightweight metadata projection for save slot UI.

| Property | Type |
|----------|------|
| SlotIndex | int32 |
| SlotName | FString |
| bExists | bool |
| PlayerName | FString |
| PlayerLevel | int32 |
| TotalPlayTimeSeconds | float |
| LastSaveTimestamp | FDateTime |
| LevelName | FName |

---

### 2.8 Runtime Module — UI Widgets

---

#### UDCUserWidgetBase
**Module:** Runtime | **File:** UI/DCUserWidgetBase.h

**Purpose:** Base UMG widget with typed accessors for DeltaCode player systems.

Methods: `GetDCPlayerController`, `GetDCHUD`, `GetInventory`, `GetInteraction`

---

#### UDCMenuWidgetBase
**Module:** Runtime | **File:** UI/DCMenuWidgetBase.h

**Purpose:** Base for menu widgets that integrate with the HUD widget stack.

Extends: UDCUserWidgetBase

Methods: `CloseMenu`, `IsMenuOpen`
Events: `OnMenuOpened`, `OnMenuClosed`

---

#### UDCHUDRootWidget
**Module:** Runtime | **File:** UI/DCHUDRootWidget.h

**Purpose:** Persistent root HUD widget for notifications and interaction prompts.

| UPROPERTY | Type |
|-----------|------|
| NotificationWidgetClass | TSubclassOf\<UDCNotificationEntryWidget\> |

| BindWidget | Type |
|------------|------|
| NotificationContainer | UPanelWidget* |
| InteractionPromptText | UTextBlock* |

Methods: `ShowNotification`, `SetInteractionPrompt`, `ClearInteractionPrompt`

---

#### UDCPauseMenuWidget
**Module:** Runtime | **File:** UI/DCPauseMenuWidget.h

**Purpose:** Pause menu with save/load, quick save, main menu return, and quit.

Extends: UDCMenuWidgetBase

| UPROPERTY | Type |
|-----------|------|
| SaveSlotPanelClass | TSubclassOf\<UDCSaveSlotPanelWidget\> |
| MainMenuLevelName | FName |

| BindWidgetOptional | Type |
|--------------------|------|
| ResumeButton | UButton* |
| SaveButton | UButton* |
| LoadButton | UButton* |
| QuickSaveButton | UButton* |
| MainMenuButton | UButton* |
| QuitButton | UButton* |

---

#### UDCSaveSlotPanelWidget
**Module:** Runtime | **File:** UI/DCSaveSlotPanelWidget.h

**Purpose:** Save/load slot selection panel with slot enumeration.

| UPROPERTY | Type |
|-----------|------|
| SlotEntryClass | TSubclassOf\<UDCSaveSlotEntryWidget\> |
| NewGameLevelName | FName |

Methods: `ConfigureMode`, `GetMode`, `RefreshSlots`, `GetSelectedSlotIndex`

EDCSaveSlotPanelMode: `Save, Load`

---

#### UDCSaveSlotEntryWidget
**Module:** Runtime | **File:** UI/DCSaveSlotEntryWidget.h

**Purpose:** Individual save slot display with metadata.

| BindWidgetOptional | Type |
|--------------------|------|
| SelectButton | UButton* |
| SlotIndexText | UTextBlock* |
| PlayerNameText | UTextBlock* |
| LevelNameText | UTextBlock* |
| PlayerLevelText | UTextBlock* |
| PlayTimeText | UTextBlock* |
| TimestampText | UTextBlock* |

---

#### UDCNotificationEntryWidget
**Module:** Runtime | **File:** UI/DCNotificationEntryWidget.h

**Purpose:** Single notification toast with auto-dismiss timer.

Methods: `SetMessage`, `GetMessage`, `GetDuration`
Events: `OnNotificationShown`, `OnNotificationDismissing`

---

### 2.9 Runtime Module — AI

---

#### DCAIBlackboardKeys
**Module:** Runtime | **File:** AI/DCAIBlackboardKeys.h

| Key | Type | Purpose |
|-----|------|---------|
| TargetActor | Object | Current perceived enemy |
| LastKnownLocation | Vector | Where target was last seen |
| HomeLocation | Vector | Spawn/patrol origin |
| bIsInvestigating | Bool | Currently investigating |

---

#### UBTTask_DCMoveBase (Abstract)
**Module:** Runtime | **File:** AI/DCBTTask_MoveBase.h

**Purpose:** Abstract base for movement BT tasks with move request lifecycle.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| AcceptanceRadius | float | 50.0 |
| bUsePathfinding | bool | true |

Pure virtual: `ResolveDestination() → FVector`

---

#### UBTTask_DCWanderFromHome
**Module:** Runtime | **File:** AI/DCBTTask_WanderFromHome.h

**Purpose:** Random patrol around home location.

| UPROPERTY | Type | Default |
|-----------|------|---------|
| WanderRadius | float | 500.0 |

---

#### UBTTask_DCMoveToTarget
**Module:** Runtime | **File:** AI/DCBTTask_MoveToTarget.h

**Purpose:** Move toward the blackboard TargetActor.

---

#### UBTTask_DCAttackTarget
**Module:** Runtime | **File:** AI/DCBTTask_AttackTarget.h

**Purpose:** Fire weapon at current target, checking cooldown/reload.

---

#### UBTTask_DCInvestigateLastKnown
**Module:** Runtime | **File:** AI/DCBTTask_InvestigateLastKnown.h

**Purpose:** Move to LastKnownLocation and set investigating flag.

---

### 2.10 Editor Module

---

#### UDeltaCodeSettings
**Module:** Editor | **File:** Settings/DeltaCodeSettings.h

**Purpose:** Project Settings page for API key, model selection, and generation defaults.

UCLASS: `Config=Game, DefaultConfig, meta=(DisplayName="DeltaCode")`
Container: `Project` | Category: `Plugins` | Section: `DeltaCode`

| UPROPERTY | Type | Default |
|-----------|------|---------|
| AnthropicAPIKey | FString | — |
| ClaudeModel | FString | "claude-sonnet-4-20250514" |
| ValidationModel | FString | "claude-haiku-4-5-20251001" |
| RequestTimeoutSeconds | float | 60.0 |
| MaxOutputTokens | int32 | 4096 |
| bVerboseLogging | bool | false |
| DefaultGenerationMode | EDCGenerationMode | Safe |
| DefaultMissionTemplate | EDCMissionTemplate | Extraction |
| DefaultCameraMode | EDCCameraMode | ThirdPerson |

| Method | Returns |
|--------|---------|
| Get() | UDeltaCodeSettings* (static) |
| HasValidAPIKey() | bool |
| ValidateAPIKey() | void (CallInEditor) |

EDCGenerationMode: `Safe, Danger`
EDCMissionTemplate: `Extraction, Destiny, Fallout, OpenWorld`

---

#### FDCAnthropicClient
**Module:** Editor | **File:** API/DCAnthropicClient.h

**Purpose:** Static HTTP client for Anthropic Messages API.

| Method | Returns | Parameters |
|--------|---------|------------|
| Send | FHttpRequestPtr | FDCAnthropicRequest, FDCOnAnthropicResponse Callback |

FDCAnthropicRequest:

| Field | Type |
|-------|------|
| Model | FString |
| SystemPrompt | FString |
| Messages | TArray\<FDCAnthropicMessage\> |
| MaxTokens | int32 |
| Temperature | float |
| TimeoutSeconds | float |

FDCAnthropicResponse:

| Field | Type |
|-------|------|
| bSuccess | bool |
| HttpStatus | int32 |
| Text | FString |
| StopReason | FString |
| InputTokens | int32 |
| OutputTokens | int32 |
| ErrorMessage | FString |

Endpoint: `POST https://api.anthropic.com/v1/messages`
Headers: `x-api-key`, `anthropic-version: 2023-06-01`, `Content-Type: application/json`

- **[INPUT]** From: SDeltaCodeGeneratorPanel (user prompt + settings)
- **[OUTPUT]** To: FDCResponseParser (raw response text)

---

#### FDCAnthropicValidator
**Module:** Editor | **File:** API/DCAnthropicValidator.h

**Purpose:** Lightweight async API key validation ping.

| Method | Returns | Parameters |
|--------|---------|------------|
| PingAsync | void | FString APIKey, FString Model, float Timeout, bool bVerbose, FDCOnAnthropicPingComplete Callback |

- **[INPUT]** From: UDeltaCodeSettings::ValidateAPIKey()
- **[OUTPUT]** To: FMessageDialog (success/failure report)

---

#### FDCPromptBuilder
**Module:** Editor | **File:** Prompts/DCPromptBuilder.h

**Purpose:** Builds system prompts tailored to generation mode and mission template.

| Method | Returns | Parameters |
|--------|---------|------------|
| BuildSystemPrompt | FString | EDCGenerationMode, EDCMissionTemplate |
| ModeDisplayName | FText | EDCGenerationMode |
| TemplateDisplayName | FText | EDCMissionTemplate |

- **[INPUT]** From: SDeltaCodeGeneratorPanel (mode/template selection)
- **[OUTPUT]** To: FDCAnthropicClient (system prompt field)

---

#### FDCResponseParser
**Module:** Editor | **File:** Advisor/DCResponseParser.h

**Purpose:** Extracts code blocks from Claude's markdown response.

| Method | Returns | Parameters |
|--------|---------|------------|
| Parse | TArray\<FDCCodeBlock\> | const FString& ResponseText |

Recognized patterns:
- \`\`\`lang — language-only fence
- \`\`\`lang:path — language + file path
- C/C++ comment paths (`// File: path`)
- Script comment paths (`# File: path`)

FDCCodeBlock:

| Field | Type |
|-------|------|
| Language | FString |
| ProposedPath | FString |
| Content | FString |
| ParseNote | FString |

- **[INPUT]** From: FDCAnthropicResponse::Text
- **[OUTPUT]** To: SDeltaCodeGeneratorPanel (parsed file list), FDCFileWriter

---

#### FDCFileWriter
**Module:** Editor | **File:** Advisor/DCFileWriter.h

**Purpose:** Writes parsed code blocks to disk with path validation and safety checks.

| Method | Returns | Parameters |
|--------|---------|------------|
| Apply | EDCApplyResult | FDCCodeBlock, FString& OutPath, FString& OutErrorDetail |
| ResultToText | FText | EDCApplyResult, const FString& Detail |

EDCApplyResult: `Success, FailedNoPath, FailedPathTraversal, FailedPathNotAllowed, FailedProtectedCore, FailedExtensionNotAllowed, FailedAlreadyExists, FailedWriteError`

Allowed destinations: `Source/`, `Content/DeltaCode/`
Protected path: `Content/DeltaCode/Core/`

- **[INPUT]** From: FDCCodeBlock (parsed content + proposed path)
- **[OUTPUT]** To: Filesystem (new source files)

---

#### FDCLevelScriptingBridge
**Module:** Editor | **File:** Python/DCLevelScriptingBridge.h

**Purpose:** Bridge between the editor and the Danger Zone Python pipeline.

| Method | Returns | Parameters |
|--------|---------|------------|
| IsPythonAvailable | bool | — |
| ExecuteDangerZoneScript | bool | EDCMissionTemplate, FString& OutMessage |
| GetScriptPath | FString | — |
| TemplateSlug | FString | EDCMissionTemplate |

Script location: `Content/Python/dc_danger_zone.py`
Template slugs: `extraction`, `destiny`, `fallout`, `openworld`

- **[INPUT]** From: SDeltaCodeGeneratorPanel (Build Mission button)
- **[OUTPUT]** To: Python script execution (level generation)
- **Dependencies:** PythonScriptPlugin

---

#### SDeltaCodeGeneratorPanel
**Module:** Editor | **File:** UI/SDeltaCodeGeneratorPanel.h

**Purpose:** Main Slate panel — prompt input, API call, response display, file application.

| Widget | Purpose |
|--------|---------|
| Mode combobox | Safe Mode / Danger Zone selection |
| Template combobox | Mission template (Danger Zone only) |
| Prompt input | Multiline text input for user prompt |
| Response display | Multiline read-only response view |
| Parsed file list | SListView of extracted code blocks |
| Generate button | Sends prompt to Anthropic API |
| Cancel button | Cancels in-flight HTTP request |
| Apply / Apply All | Writes code blocks to disk |
| Build Mission | Executes Danger Zone Python pipeline |
| Status text | Current operation status |

FDCCodeBlockEntry:

| Field | Type |
|-------|------|
| Block | FDCCodeBlock |
| bApplied | bool |
| LastResult | EDCApplyResult |
| OutputPath | FString |
| ErrorDetail | FString |

- **[INPUT]** From: User interaction, UDeltaCodeSettings (defaults)
- **[OUTPUT]** To: FDCAnthropicClient, FDCResponseParser, FDCFileWriter, FDCLevelScriptingBridge

---

#### FDeltaCodeCommands
**Module:** Editor | **File:** UI/DeltaCodeCommands.h

**Purpose:** UI command definitions for toolbar and menu integration.

Commands: `OpenGeneratorPanel`

---

#### FDeltaCodeStyle
**Module:** Editor | **File:** UI/DeltaCodeStyle.h

**Purpose:** Slate style set that loads the plugin toolbar icon.

Registers: `"DeltaCode.OpenGeneratorPanel"` brush from `Resources/Icon40.png`
Style set name: `"DeltaCodeStyle"`

---

#### FDeltaCodeEditorModule
**Module:** Editor | **File:** DeltaCodeEditorModule.h

**Purpose:** Editor module entry point — registers commands, tab spawner, toolbar, and menus.

Startup order:
1. `FDeltaCodeStyle::Initialize()`
2. `FDeltaCodeCommands::Register()`
3. `GetMutableDefault<UDeltaCodeSettings>()`
4. Register nomad tab spawner
5. Defer `RegisterMenus()` via UToolMenus callback

---

## 3. SYSTEM DEPENDENCY MAP

```
                        ┌──────────────────────────────────────┐
                        │        UDeltaCodeSettings            │
                        │   (API key, model, mode defaults)    │
                        └──────────┬───────────────────────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    ▼              ▼              ▼
          ┌─────────────┐  ┌────────────┐  ┌──────────────────┐
          │ FDCAnthro-  │  │ FDCAnthro- │  │ SDeltaCode-      │
          │ picClient   │  │ picValid-  │  │ GeneratorPanel   │
          │ (HTTP POST) │  │ ator       │  │ (Main UI)        │
          └──────┬──────┘  └────────────┘  └────┬──────┬──────┘
                 │                              │      │
                 ▼                              │      ▼
          ┌─────────────┐                       │  ┌──────────────┐
          │ FDCResponse │◄──────────────────────┘  │ FDCLevel-    │
          │ Parser      │                          │ Scripting-   │
          └──────┬──────┘                          │ Bridge       │
                 │                                 └──────┬───────┘
                 ▼                                        │
          ┌─────────────┐                                 ▼
          │ FDCFile-    │                          ┌──────────────┐
          │ Writer      │                          │ dc_danger_   │
          │ (Safe Mode) │                          │ zone.py      │
          └──────┬──────┘                          │ (Danger Zone)│
                 │                                 └──────────────┘
                 ▼
          ┌─────────────┐
          │ Source/      │
          │ Content/     │
          │ (Generated   │
          │  files)      │
          └─────────────┘


  ═══════════════ RUNTIME DEPENDENCY MAP ═══════════════

  UDCItemDefinitionRegistry ◄──── AssetRegistry scan
         │
         │ resolves FDCItemHandle
         ▼
  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
  │ UDCInventory │───►│ UDCEquipment │───►│ ADCWeapon-   │
  │ Component    │    │ Component    │    │ Base         │
  └──────┬───────┘    └──────┬───────┘    └──────────────┘
         │                   │
         │    ┌──────────────┘
         ▼    ▼
  ┌──────────────┐
  │ UDCSaveGame  │◄──── ADCGameInstance
  └──────┬───────┘              │
         │                      │
         ▼                      ▼
  ┌──────────────┐    ┌──────────────┐
  │ UDCQuest-    │    │ ADCPlayer-   │
  │ Subsystem    │    │ Controller-  │
  └──────┬───────┘    │ Base         │
         │            └──────┬───────┘
         ▼                   │
  ┌──────────────┐           │ owns
  │ UDCReputa-   │           ▼
  │ tion-        │    ┌──────────────────────────────────┐
  │ Subsystem    │    │ Components:                      │
  └──────────────┘    │  Interaction → IDCInteractable   │
                      │  Inventory   → ItemRegistry      │
                      │  NarrativeState (tag store)      │
                      │  Progression → LevelCurve        │
                      │  SkillTree   → SkillTreeDef      │
                      └──────────────────────────────────┘

  ┌──────────────┐    ┌──────────────┐
  │ ADCNPCBase   │───►│ UDCDialogue  │───► NarrativeState
  │              │    │ Component    │───► QuestSubsystem
  │              │    └──────────────┘
  │              │───►┌──────────────┐
  │              │    │ UDCVendor-   │───► InventoryComp
  └──────────────┘    │ Component    │
                      └──────────────┘

  ┌──────────────┐    ┌──────────────┐
  │ ADCEnemy-    │───►│ ADCEnemy-    │
  │ Base         │    │ AIController │
  │ (loot, BT)  │    │ (perception) │
  └──────────────┘    └──────────────┘

  ┌──────────────┐    ┌──────────────┐
  │ ADCCharacter │───►│ UDCHealth-   │
  │ Base         │    │ Component    │
  │ (GAS, stats) │    └──────────────┘
  └──────┬───────┘
         │ extends
         ▼
  ┌────────────┐  ┌────────────┐  ┌──────────────────┐
  │ ThirdPerson│  │ FirstPerson│  │ DualPerspective  │
  │ Character  │  │ Character  │  │ Character        │
  └────────────┘  └────────────┘  └──────────────────┘
```

---

## 4. EDITOR PLUGIN REFERENCE

### DeltaCode Panel UI

Accessed via: **Toolbar button** (DeltaCode icon) or **Window menu > DeltaCode**

| Element | Description |
|---------|-------------|
| Mode dropdown | Safe Mode or Danger Zone |
| Template dropdown | Mission template (visible in Danger Zone only) |
| Prompt input | Multiline text field for natural-language requests |
| Generate button | Sends prompt to Claude API with system context |
| Cancel button | Aborts in-flight HTTP request |
| Response display | Read-only view of Claude's full response |
| Parsed files list | Extracted code blocks with language, path, and status |
| Apply button | Writes individual code block to disk |
| Apply All button | Writes all parsed code blocks to disk |
| Build Mission button | Executes Danger Zone Python pipeline (Danger Zone only) |
| Status bar | Shows current operation, errors, and provider info |

### Safe Mode Workflow

1. User selects **Safe Mode** from the mode dropdown
2. User types a prompt describing what to generate
3. Click **Generate** — plugin builds a system prompt (Safe Mode rules) and sends to Claude
4. Claude response appears in the response panel
5. `FDCResponseParser` extracts code blocks with file paths
6. Parsed files appear in the list view
7. User clicks **Apply** or **Apply All**
8. `FDCFileWriter` validates paths (must be in `Source/` or `Content/DeltaCode/`, not in `Core/`)
9. Files are written to disk
10. Status shows success/failure per file

**Safe Mode constraints:**
- Never deletes, replaces, or modifies existing level actors
- Cannot write to `Content/DeltaCode/Core/`
- Path traversal (`../`) is blocked
- Only allowed file extensions

### Danger Zone Workflow

1. User selects **Danger Zone** from the mode dropdown
2. Template dropdown becomes visible — user selects a mission template
3. **Option A — Prompt generation:** Same as Safe Mode but with Danger Zone system prompt
4. **Option B — Build Mission:** Click **Build Mission** button
5. Confirmation modal: "Danger Zone can produce scripts that clear and rebuild level content."
6. If confirmed, `FDCLevelScriptingBridge::ExecuteDangerZoneScript()` runs
7. Python script `dc_danger_zone.py` executes with the selected template slug
8. Level content is generated/rebuilt

**Mission Templates:**

| Template | Slug | Description |
|----------|------|-------------|
| Extraction Shooter | `extraction` | Extraction-style shooter level |
| Destiny Arena | `destiny` | Destiny-style arena mission |
| Fallout Open World | `fallout` | Fallout-style open world mission |
| Narrative RPG | `openworld` | Skyrim/Witcher/Elden Ring/RDR2 hybrid |

### API Key Setup

1. Open **Project Settings > Plugins > DeltaCode**
2. Enter your Anthropic API key in the **Anthropic API Key** field
3. Click **Validate API Key** — sends a minimal ping to confirm the key works
4. Key is stored in `Saved/Config/.../Game.ini` — never committed to source control

### Project Settings Fields

| Field | Category | Default |
|-------|----------|---------|
| Anthropic API Key | API | (empty) |
| Generation Model | API | claude-sonnet-4-20250514 |
| Validation Model | API | claude-haiku-4-5-20251001 |
| Request Timeout | API | 60.0s |
| Max Output Tokens | API | 4096 |
| Verbose HTTP Logging | API | false |
| Default Generation Mode | Generation | Safe |
| Default Mission Template | Generation | Extraction |
| Default Camera Mode | Generation | ThirdPerson |

---

## 5. DATA FLOW REFERENCE

### Safe Mode: Prompt → API → File Write

```
User types prompt
       │
       ▼
SDeltaCodeGeneratorPanel
       │
       ├─► FDCPromptBuilder::BuildSystemPrompt(Safe, Template)
       │           │
       │           ▼
       │   System prompt with Safe Mode rules
       │           │
       ├───────────┘
       │
       ▼
FDCAnthropicClient::Send()
       │
       ├─► HTTP POST api.anthropic.com/v1/messages
       │   Headers: x-api-key, anthropic-version
       │   Body: {model, system, messages, max_tokens}
       │
       ▼
FDCAnthropicResponse (async callback)
       │
       ▼
FDCResponseParser::Parse(ResponseText)
       │
       ├─► Extract fenced code blocks
       │   Detect language + file path
       │
       ▼
TArray<FDCCodeBlock> displayed in list view
       │
       ▼ (user clicks Apply)
FDCFileWriter::Apply(Block)
       │
       ├─► Validate: path in Source/ or Content/DeltaCode/
       ├─► Validate: no path traversal
       ├─► Validate: not in Content/DeltaCode/Core/
       ├─► Validate: allowed file extension
       │
       ▼
File written to disk → Status: Success
```

### Danger Zone: Template → Python → Level Build

```
User selects Danger Zone + Template
       │
       ▼
Click "Build Mission"
       │
       ▼
FMessageDialog: "Proceed with Danger Zone generation?"
       │
       ├─► No → Status: "Danger Zone request cancelled."
       │
       ▼ Yes
FDCLevelScriptingBridge::ExecuteDangerZoneScript(Template)
       │
       ├─► IsPythonAvailable() check
       │
       ├─► TemplateSlug(Template)
       │   extraction / destiny / fallout / openworld
       │
       ▼
IPythonScriptPlugin::ExecPythonCommand()
       │
       ├─► Runs: Content/Python/dc_danger_zone.py
       │   with template slug parameter
       │
       ▼
Python script:
  ├─► Clears existing level content (if template requires)
  ├─► Spawns actors, geometry, lighting
  ├─► Configures gameplay elements
  │
  ▼
Level rebuilt → Status: "Danger Zone pipeline complete."
```

### Save/Load Data Flow

```
Save trigger (checkpoint, pause menu, auto-save)
       │
       ▼
ADCGameInstance::CaptureFromPlayer()
       │
       ├─► InventoryComponent::GetEntries()
       ├─► EquipmentComponent::CaptureForSave()
       ├─► NarrativeState::GetTags()
       ├─► ProgressionComponent::CaptureForSave()
       ├─► SkillTreeComponent::CaptureForSave()
       ├─► QuestSubsystem::CaptureForSave()
       ├─► ReputationSubsystem::CaptureForSave()
       │
       ▼
UDCSaveGame (populated)
       │
       ▼
UGameplayStatics::SaveGameToSlot()
       │
       ▼
Saved/SaveGames/DCSave_{SlotIndex}.sav


Load trigger (pause menu, startup)
       │
       ▼
ADCGameInstance::LoadGameFromDisk(SlotIndex)
       │
       ▼
UGameplayStatics::LoadGameFromSlot()
       │
       ▼
UDCSaveGame (loaded)
       │
       ▼
ADCGameInstance::ApplySaveToPlayer()
       │
       ├─► InventoryComponent::RestoreFromSave()
       ├─► EquipmentComponent::RestoreFromSave()
       ├─► NarrativeState::SetTagsFromSave()
       ├─► ProgressionComponent::RestoreFromSave()
       ├─► SkillTreeComponent::RestoreFromSave()
       ├─► QuestSubsystem::RestoreFromSave()
       ├─► ReputationSubsystem::RestoreFromSave()
       │
       ▼
Player state fully restored
```

---

## 6. BLUEPRINT ASSETS REQUIRED

The following Blueprint assets must be created in the Unreal Editor to complete the runtime game loop. C++ base classes exist — these are the Blueprint children with visual configuration.

### Characters

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| B_DC_ThirdPersonCharacter | ADCThirdPersonCharacter | Default third-person pawn with mesh/animations |
| B_DC_FirstPersonCharacter | ADCFirstPersonCharacter | Default first-person pawn with arms mesh |
| B_DC_DualPerspectiveCharacter | ADCDualPerspectiveCharacter | Switchable perspective pawn |

### Game Framework

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| B_DC_GameMode | ADCGameModeBase | Default game mode pointing to character/controller/HUD |
| B_DC_PlayerController | ADCPlayerControllerBase | Player controller with InputConfig and PauseMenuClass set |
| B_DC_GameInstance | ADCGameInstance | Game instance with save slot configuration |
| B_DC_HUD | ADCHUDBase | HUD class with root widget assignment |

### Enemies & AI

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| B_DC_EnemyBase | ADCEnemyBase | Base enemy with mesh, loot table, behavior tree |
| B_DC_EnemyAIController | ADCEnemyAIController | AI controller with perception settings |
| BT_DC_EnemyDefault | UBehaviorTree | Default enemy behavior tree using DC BT tasks |

### NPCs & Vendors

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| B_DC_NPC | ADCNPCBase | NPC with mesh and NPCDefinition assignment |

### Weapons & Pickups

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| B_DC_WeaponBase | ADCWeaponBase | Weapon actor with mesh and fire FX |
| B_DC_Pickup | ADCPickupBase | World pickup with mesh and interaction sphere |

### UI Widgets

| Asset | Parent Class | Purpose |
|-------|-------------|---------|
| W_DC_HUDRoot | UDCHUDRootWidget | Root HUD with notification container and interaction prompt |
| W_DC_PauseMenu | UDCPauseMenuWidget | Pause menu with button bindings |
| W_DC_SaveSlotPanel | UDCSaveSlotPanelWidget | Save/load slot selection panel |
| W_DC_SaveSlotEntry | UDCSaveSlotEntryWidget | Individual slot display |
| W_DC_NotificationEntry | UDCNotificationEntryWidget | Toast notification |

### Data Assets

| Asset | Type | Purpose |
|-------|------|---------|
| DA_DC_InputConfig | UDCInputConfig | Enhanced Input action-to-tag mapping |
| DA_DC_LevelCurve | UDCLevelCurve | XP thresholds and level-up bonuses |
| DA_DC_SkillTree | UDCSkillTreeDefinition | Skill tree with prerequisites |
| DA_DC_Item_* | UDCItemDefinition | Item definitions (one per item) |
| DA_DC_Weapon_* | UDCWeaponDefinition | Weapon definitions |
| DA_DC_Equipment_* | UDCEquipmentDefinition | Equipment definitions |
| DA_DC_Quest_* | UDCQuestDefinition | Quest definitions |
| DA_DC_Faction_* | UDCFactionDefinition | Faction definitions with stance bands |
| DA_DC_NPC_* | UDCNPCDefinition | NPC definitions with dialogue trees |
| DA_DC_Vendor_* | UDCVendorDefinition | Vendor stock and pricing |
| DA_DC_LootTable_* | UDCLootTableDefinition | Loot table definitions |
| DA_DC_Skill_* | UDCSkillDefinition | Individual skill definitions |

### Input Actions & Mapping Context

| Asset | Type | Purpose |
|-------|------|---------|
| IMC_DC_Default | UInputMappingContext | Default key/gamepad bindings |
| IA_DC_Move | UInputAction | Movement (2D axis) |
| IA_DC_Look | UInputAction | Camera look (2D axis) |
| IA_DC_Jump | UInputAction | Jump (digital) |
| IA_DC_Interact | UInputAction | Interact (digital) |
| IA_DC_Fire | UInputAction | Fire weapon (digital) |
| IA_DC_Reload | UInputAction | Reload weapon (digital) |
| IA_DC_TogglePerspective | UInputAction | Camera perspective swap (digital) |
| IA_DC_Pause | UInputAction | Pause menu toggle (digital) |

---

*Generated from DeltaCode v1.0.0 source code. All class references reflect the compiled state of the plugin.*

# SKILL: ue5-gameplay-framework
# DeltaCode Plugin — Gameplay Actor Hierarchy, GAS, Pickups, Enemies, Boss

## Purpose
This skill governs every C++ class DeltaCode generates that exists at runtime in the
game world: pickups, mission objectives, enemies, bosses, weapons, and their components.
It enforces Epic's C++ Coding Standard naming rules, correct GC-safe pointer patterns,
Gameplay Ability System (GAS) integration, and the Enhanced Input System.

This skill is active during Safe Mode code generation and all Danger Zone actor spawning.

---

## DeltaCode Actor Hierarchy

All DeltaCode-generated actors follow this inheritance chain.
C++ base → Blueprint child (Lyra-style). Designers work in Blueprint; logic lives in C++.

```
AActor
├── ADCPickupBase          (C++) → B_DC_Pickup_[Name]     (Blueprint child)
├── ADCMissionObjective    (C++) → B_DC_Objective_[Name]  (Blueprint child)
├── ADCWeaponBase          (C++) → B_DC_Weapon_[Name]     (Blueprint child)
└── ACharacter
    ├── ADCEnemyBase       (C++) → B_DC_Enemy_[Name]      (Blueprint child)
    │   └── ADCEnemyRanged (C++) → B_DC_Enemy_Ranged      (Blueprint child)
    └── ADCBossBase        (C++) → B_DC_Boss_[Name]       (Blueprint child)
```

---

## Naming Conventions (Epic C++ Standard + Lyra)

Per Epic's mandatory C++ Coding Standard:
- Classes inheriting AActor: `A` prefix          → `ADCEnemyBase`
- Classes inheriting UObject: `U` prefix         → `UDCHealthComponent`
- Structs: `F` prefix                            → `FDCMissionObjectiveData`
- Enums: `E` prefix                              → `EDCEnemyState`
- Interfaces: `I` prefix                         → `IDCInteractable`
- Boolean variables: `b` prefix                  → `bIsDead`, `bIsActivated`
- PascalCase for all names, no underscores in code identifiers

Blueprint asset naming (Lyra conventions):
- Blueprint Actor children: `B_DC_[BaseName]`    → `B_DC_HealthPickup`
- Widget Blueprints: `W_DC_[Name]`               → `W_DC_ObjectiveMarker`
- Gameplay Ability: `GA_DC_[Name]`               → `GA_DC_FireWeapon`
- Gameplay Effect: `GE_DC_[Name]`                → `GE_DC_ApplyDamage`

---

## ADCPickupBase

```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCPickupBase.generated.h"

// Implement IDCInteractable if the pickup supports manual interaction (E key)
UCLASS(Abstract, BlueprintType, Blueprintable)
class DELTACODE_API ADCPickupBase : public AActor
{
    GENERATED_BODY()

public:
    ADCPickupBase();

    // Called when a valid Pawn overlaps the pickup trigger
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Pickup")
    void OnPickedUp(APawn* Collector);
    virtual void OnPickedUp_Implementation(APawn* Collector);

    // Time in seconds before respawn. 0 = no respawn.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Pickup")
    float RespawnDelay = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Pickup")
    bool bDestroyOnPickup = true;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USphereComponent> OverlapSphere;

private:
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                        bool bFromSweep, const FHitResult& SweepResult);

    void HandleRespawn();
    FTimerHandle RespawnTimerHandle;
};
```

### Key rules for pickups:
- Always use `TObjectPtr<>` for UPROPERTY member UObject references (UE5.5+ standard)
- Never use raw `UStaticMeshComponent*` without UPROPERTY — GC will collect it
- Use `BlueprintNativeEvent` for `OnPickedUp` so Blueprint children can override it
- Timer handles (`FTimerHandle`) are value types — never wrap in TObjectPtr

---

## ADCMissionObjective

```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCMissionObjective.generated.h"

UENUM(BlueprintType)
enum class EDCObjectiveState : uint8
{
    Inactive    UMETA(DisplayName = "Inactive"),
    Active      UMETA(DisplayName = "Active"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveStateChanged,
    EDCObjectiveState, NewState);

UCLASS(Abstract, BlueprintType, Blueprintable)
class DELTACODE_API ADCMissionObjective : public AActor
{
    GENERATED_BODY()

public:
    ADCMissionObjective();

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Mission")
    FOnObjectiveStateChanged OnObjectiveStateChanged;

    UFUNCTION(BlueprintCallable, Category = "DeltaCode|Mission")
    void SetObjectiveState(EDCObjectiveState NewState);

    UFUNCTION(BlueprintPure, Category = "DeltaCode|Mission")
    EDCObjectiveState GetObjectiveState() const { return CurrentState; }

    // Display text shown in HUD for this objective
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Mission")
    FText ObjectiveDisplayName;

    // Sequence index — lower numbers complete first
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Mission")
    int32 SequenceIndex = 0;

    // If true, failing this objective fails the entire mission
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Mission")
    bool bIsMandatory = true;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = "DeltaCode|Mission")
    void OnObjectiveActivated();
    virtual void OnObjectiveActivated_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, Category = "DeltaCode|Mission")
    void OnObjectiveCompleted();
    virtual void OnObjectiveCompleted_Implementation() {}

private:
    EDCObjectiveState CurrentState = EDCObjectiveState::Inactive;
};
```

---

## ADCEnemyBase — GAS-Ready

```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "DCEnemyBase.generated.h"

// Implement IAbilitySystemInterface for GAS integration
UCLASS(Abstract, BlueprintType, Blueprintable)
class DELTACODE_API ADCEnemyBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ADCEnemyBase();

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Called by ADCBossBase or wave spawners after spawn
    UFUNCTION(BlueprintCallable, Category = "DeltaCode|Enemy")
    virtual void InitializeEnemy();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy")
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy")
    float PatrolRadius = 1000.0f;    // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy")
    float DetectionRange = 2000.0f;  // cm

    // Loot table — Blueprint-assignable array of pickup classes to spawn on death
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy|Loot")
    TArray<TSubclassOf<ADCPickupBase>> LootTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy|Loot",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LootDropChance = 0.3f;

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Enemy")
    void OnDeath();
    virtual void OnDeath_Implementation();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UDCHealthComponent> HealthComponent;

private:
    float CurrentHealth;
};
```

---

## ADCBossBase

```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DCEnemyBase.h"
#include "DCBossBase.generated.h"

UENUM(BlueprintType)
enum class EDCBossPhase : uint8
{
    Phase1  UMETA(DisplayName = "Phase 1 — Full Health"),
    Phase2  UMETA(DisplayName = "Phase 2 — Below 50%"),
    Phase3  UMETA(DisplayName = "Phase 3 — Below 25%"),
    Enraged UMETA(DisplayName = "Enraged — Final")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, EDCBossPhase, NewPhase);

UCLASS(Abstract, BlueprintType, Blueprintable)
class DELTACODE_API ADCBossBase : public ADCEnemyBase
{
    GENERATED_BODY()

public:
    ADCBossBase();

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Boss")
    FOnBossPhaseChanged OnBossPhaseChanged;

    // Health thresholds for phase transitions (% of MaxHealth)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Boss|Phases")
    float Phase2Threshold = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Boss|Phases")
    float Phase3Threshold = 0.25f;

    // Minion spawner class — boss can summon adds in Phase 2+
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Boss|Spawning")
    TSubclassOf<ADCEnemyBase> MinionClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Boss|Spawning")
    int32 MinionsPerWave = 3;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = "DeltaCode|Boss")
    void OnPhaseTransition(EDCBossPhase NewPhase);
    virtual void OnPhaseTransition_Implementation(EDCBossPhase NewPhase) {}

    UFUNCTION(BlueprintCallable, Category = "DeltaCode|Boss")
    void SpawnMinionWave();

    virtual void OnDeath_Implementation() override;

private:
    EDCBossPhase CurrentPhase = EDCBossPhase::Phase1;
    void CheckPhaseTransition(float HealthPercent);
};
```

---

## UDCHealthComponent

Reusable component — attach to any actor that needs health tracking.

```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup=(DeltaCode), BlueprintType, meta=(BlueprintSpawnableComponent))
class DELTACODE_API UDCHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDCHealthComponent();

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Health")
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Health")
    FOnDeath OnDeath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Health")
    float MaxHealth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
    void ApplyDamage(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
    void ApplyHealing(float HealAmount);

    UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
    bool IsDead() const { return bIsDead; }

private:
    float CurrentHealth;
    bool bIsDead = false;
};
```

---

## Pointer Safety Rules (UE5.5+ Standard)

Per Epic's C++ standard and UE5.5 GC requirements:

| Use case | Correct type |
|---|---|
| UPROPERTY member pointing to UObject subclass | `TObjectPtr<UMyClass>` |
| Non-UPROPERTY temporary reference | raw pointer `UMyClass*` (scoped only) |
| Optional reference, may be null | `TWeakObjectPtr<UMyClass>` |
| Owning reference to non-UObject | `TUniquePtr<FMyStruct>` |
| Array of UObject references | `TArray<TObjectPtr<UMyClass>>` |
| Class reference (for spawning) | `TSubclassOf<AMyActor>` |
| Lazy-loaded asset reference | `TSoftObjectPtr<UStaticMesh>` |

NEVER use raw `UObject*` as a UPROPERTY — GC will not track it and will collect the object.
NEVER use `TSharedPtr` for UObject subclasses — use `TObjectPtr` or `TWeakObjectPtr`.

---

## Blueprint NativeEvent Pattern

Use `BlueprintNativeEvent` when:
- The function MUST be callable from both C++ and Blueprint
- Blueprint children should be able to override the behavior
- There is a sensible default implementation in C++

Pattern:
```cpp
// .h — Declaration
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode")
void OnActivated();
virtual void OnActivated_Implementation(); // Always declare the _Implementation

// .cpp — Default behavior
void ADCMissionObjective::OnActivated_Implementation()
{
    // Sensible default — Blueprint children will call this via Super or override entirely
    SetObjectiveState(EDCObjectiveState::Active);
}
```

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Raw `UStaticMeshComponent*` member without UPROPERTY
❌ `TSharedPtr<AActor>` or `TSharedPtr<UObject>` — use TObjectPtr for UObjects
❌ Gameplay logic in constructors — use BeginPlay()
❌ `GetAllActorsOfClass()` in Tick — cache results, never query every frame
❌ Cast<> in Tick without caching the result
❌ Calling SpawnActor in Tick without a cooldown or one-shot gate
❌ Blueprint base class for anything in the DC actor hierarchy
❌ FString for actor/class identifiers where FName suffices
❌ Missing `_Implementation` suffix on BlueprintNativeEvent definitions

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Epic C++ Coding Standard, Epic GAS documentation, UE5 Object Pointer docs
Plugin: DeltaCode
Last Updated: 2026-03

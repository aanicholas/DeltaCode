# SKILL: ue5-cpp-vs-blueprint
# DeltaCode Plugin — Safe Mode Advisor

## Purpose
This skill governs every decision DeltaCode makes when advising a user on whether
a given Unreal Engine 5 system, class, or feature should be implemented in C++ or
Blueprint. It also governs the explanation DeltaCode provides to the user, and the
code scaffold it generates afterward.

This skill is ALWAYS active during Safe Mode. It must be consulted before any code
generation response is issued.

---

## Core Philosophy

C++ and Blueprint are not competing tools. They are layers in a deliberate architecture:

  C++ defines the contract.
  Blueprint fulfills the contract for designers.

DeltaCode must never treat this as a preference question. It is an engineering
decision with correct answers based on the specific characteristics of the request.

---

## The Decision Framework

When a user asks "should this be C++ or Blueprint?", apply the following tests in order.
Stop at the first test that gives a definitive answer.

### TEST 1 — Will other classes inherit from this?
  YES → C++
  Reason: Blueprint inheritance from Blueprint parents is fragile, slow to compile,
  and breaks easily across engine versions. Any class intended to be a base
  (weapons, characters, game modes, components) must be C++.

  Example triggers: "master weapon class", "base enemy", "base pickup", "character base"

### TEST 2 — Is this called every frame (Tick) with significant logic?
  YES → C++
  Reason: Blueprint VM overhead per tick is non-trivial at scale. Any tick-heavy
  system (movement, physics queries, AI perception updates) belongs in C++.

  Example triggers: "floats back and forth", "follows player", "rotates constantly",
  "checks proximity every frame"

### TEST 3 — Does this touch the network / replication directly?
  YES → C++
  Reason: Blueprint replication is supported but unreliable for complex cases.
  RPCs, ReplicatedUsing callbacks, and NetMulticast events should be declared in C++
  and called from Blueprint if needed.

  Example triggers: "multiplayer", "synced between players", "server authoritative"

### TEST 4 — Is this a one-off designer-facing object with no children?
  YES → Blueprint
  Reason: Single-use actors, level-specific triggers, UI widgets, and VFX-coupled
  objects are faster to iterate in Blueprint and safe without a C++ parent.

  Example triggers: "a specific door in this level", "this one pickup", "a trigger
  volume for this room", "a particle burst on collect"

### TEST 5 — Does this need to be tweaked frequently by a non-programmer?
  YES → Blueprint (with C++ parent if it has siblings)
  Reason: Blueprint exposes properties visually. If the user or a designer will
  be tuning values (speeds, ranges, damage numbers), Blueprint is the correct
  surface even if the parent is C++.

  Example triggers: "I want to be able to change the patrol speed easily",
  "the mission designer needs to set objectives", "tweak damage in editor"

### TEST 6 — Is this a utility function / math / string operation called frequently?
  YES → C++ (as a BlueprintCallable UFunction or static Blueprint Function Library)
  Reason: Heavy utility logic (pathfinding helpers, geometry math, data parsing)
  should live in C++ and be exposed to Blueprint via UFUNCTION(BlueprintCallable).

  Example triggers: "calculate line of sight", "find nearest pickup", "parse
  save data", "format UI string from struct"

### DEFAULT — Ambiguous or simple, no inheritance concerns
  → Blueprint
  Reason: When no test fires definitively, default to Blueprint. It is faster to
  prototype, easier to debug visually, and can always be converted to C++ later
  if performance demands it.

---

## Response Format for Safe Mode Advisor

When DeltaCode advises on C++ vs Blueprint, the response MUST follow this structure:

```
RECOMMENDATION: [C++ | Blueprint | C++ parent + Blueprint child]

REASON: [1-3 sentences. Reference the specific test that fired. Be direct.]

TRADEOFFS:
  ✓ [What this choice gives the user]
  ✗ [What this choice costs or limits]

UPGRADE PATH: [One sentence on when/how to migrate if needs change.]
```

Do NOT give wishy-washy answers like "it depends on your use case."
Always commit to a recommendation. The user is asking because they want a decision.

---

## Code Generation Rules (Post-Decision)

### If generating C++:

ALWAYS include:
  - UCLASS() macro with correct specifiers
  - GENERATED_BODY()
  - Module API macro (e.g., DELTACODE_API)
  - Proper #include chain (CoreMinimal.h first, then engine headers, then project)
  - Constructor declaration in .h, definition in .cpp
  - UPROPERTY() on all designer-exposed members with EditAnywhere or EditDefaultsOnly
  - UFUNCTION() on all Blueprint-callable methods

NEVER:
  - Use raw pointers for UObject references without a UPROPERTY() — this causes GC crashes
  - Put gameplay logic in constructors — use BeginPlay()
  - Use FString where FName suffices for identifiers
  - Hardcode asset paths as strings — use TSoftObjectPtr or UDataAsset references

### If generating Blueprint (as text scaffold / instructions):

ALWAYS specify:
  - Parent class (even if it's AActor or UObject — never leave this implicit)
  - Which UPROPERTY variables to expose and their category names
  - Which events to implement (BeginPlay, Tick, custom dispatchers)
  - Component hierarchy (Scene root → Mesh → Collision → etc.)

NEVER:
  - Generate Blueprint as raw .uasset binary
  - Suggest "just drag it in" without specifying exact component setup
  - Omit the parent class

---

## DeltaCode-Specific Patterns

### The Floating Platform (example — Safe Mode)
User: "a platform that is 1m x 1m x 200mm that floats back and forth 50m
       while being 400mm above the ground"

TEST RESULTS:
  Test 1: No inheritance planned → not decisive
  Test 2: Moves every frame → C++
  Test 5: Speed/distance likely tweaked → expose via UPROPERTY

RECOMMENDATION: C++ actor with Blueprint-exposed properties

```cpp
// ADeltaFloatingPlatform.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeltaFloatingPlatform.generated.h"

UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADeltaFloatingPlatform : public AActor
{
    GENERATED_BODY()

public:
    ADeltaFloatingPlatform();

    // Travel distance in each direction from origin (cm). Default = 5000cm (50m)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Platform")
    float TravelDistance = 5000.0f;

    // Speed of platform movement (cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Platform")
    float MoveSpeed = 200.0f;

    // Height above ground (cm). Default = 40cm (400mm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Platform")
    float HeightAboveGround = 40.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* PlatformMesh;

    FVector StartLocation;
    FVector TargetA;
    FVector TargetB;
    bool bMovingToB = true;
};
```

### The Master Weapon Class (example — Safe Mode)
User: "create a Master Weapon Class"

TEST RESULTS:
  Test 1: Will definitely have children (pistol, rifle, shotgun) → C++
  Test 3: May need replication later → C++

RECOMMENDATION: C++ base class, always. No exceptions for a master weapon class.

---

## UE5.5+ Compatibility Notes

- Use `FObjectPtr` instead of raw `TObjectPtr` where forward declarations suffice (UE5.5+)
- `UE_LOG` remains valid; prefer structured logging via `UE_LOGFMT` in 5.5+
- Enhanced Input System is now default — never generate legacy `BindAction` calls
- Chaos Physics is the only physics engine — no PhysX references
- `PRAGMA_DISABLE_OPTIMIZATION` is deprecated in 5.5 — use `UE_DISABLE_OPTIMIZATION`
- Motion Warping and Chooser are stable in 5.5 — safe to reference in animation-adjacent classes

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Blueprint parent class for anything with planned children
❌ `Tick()` with gameplay logic in Blueprint (suggest C++ migration)
❌ `Cast<>` in a tight loop — cache the result
❌ Spawning actors in `Tick()` without a cooldown gate
❌ `UGameplayStatics::GetAllActorsOfClass()` in Tick (extremely expensive)
❌ Advice to "just use Blueprint" for a base class because it's "easier"
❌ Generating code that compiles but violates UE5 GC rules (raw UObject pointers)

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Plugin: DeltaCode
Last Updated: 2026-03

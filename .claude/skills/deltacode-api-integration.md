# SKILL: deltacode-api-integration
# DeltaCode Plugin — Anthropic API HTTP Integration in UE5 C++

## Purpose
This skill governs every aspect of DeltaCode's communication with the Anthropic API:
async HTTP requests via UE5's FHttpModule, JSON construction and parsing, streaming
response handling, the system prompts for Safe Mode vs Danger Zone, and the per-user
API key validation flow. This is Editor-module only — never Runtime.

---

## Module Dependencies Required

In `DeltaCodeEditor.Build.cs`, these must be present:
```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "HTTP",
    "Json",
    "JsonUtilities",
});
```

---

## UDCAnthropicClient — Core HTTP Handler

### UDCAnthropicClient.h
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Http.h"
#include "DCAnthropicClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponseReceived, const FString&, ResponseText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestFailed, const FString&, ErrorMessage);

UCLASS()
class DELTACODEEDITOR_API UDCAnthropicClient : public UObject
{
    GENERATED_BODY()

public:
    // Send a prompt to Claude. Response arrives via OnResponseReceived delegate.
    void SendPrompt(const FString& SystemPrompt,
                    const FString& UserMessage,
                    const FString& APIKey,
                    const FString& ModelID = TEXT("claude-sonnet-4-20250514"));

    // Validate API key with a minimal test call (1 token max)
    void ValidateAPIKey(const FString& APIKey,
                        TFunction<void(bool bIsValid, FString Error)> Callback);

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|API")
    FOnResponseReceived OnResponseReceived;

    UPROPERTY(BlueprintAssignable, Category = "DeltaCode|API")
    FOnRequestFailed OnRequestFailed;

private:
    void OnMessageResponse(FHttpRequestPtr Request,
                           FHttpResponsePtr Response,
                           bool bConnectedSuccessfully);

    FString BuildRequestBody(const FString& SystemPrompt,
                             const FString& UserMessage,
                             const FString& ModelID,
                             int32 MaxTokens = 4096);

    static const FString AnthropicBaseURL;
    static const FString AnthropicAPIVersion;
};
```

### UDCAnthropicClient.cpp
```cpp
// Copyright DeltaCode. All Rights Reserved.
#include "DCAnthropicClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

const FString UDCAnthropicClient::AnthropicBaseURL =
    TEXT("https://api.anthropic.com/v1/messages");
const FString UDCAnthropicClient::AnthropicAPIVersion =
    TEXT("2023-06-01");

void UDCAnthropicClient::SendPrompt(
    const FString& SystemPrompt,
    const FString& UserMessage,
    const FString& APIKey,
    const FString& ModelID)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(AnthropicBaseURL);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Request->SetHeader(TEXT("x-api-key"),     APIKey);
    Request->SetHeader(TEXT("anthropic-version"), AnthropicAPIVersion);

    Request->SetContentAsString(BuildRequestBody(SystemPrompt, UserMessage, ModelID));

    Request->OnProcessRequestComplete().BindUObject(
        this, &UDCAnthropicClient::OnMessageResponse);

    Request->ProcessRequest();
}

void UDCAnthropicClient::ValidateAPIKey(
    const FString& APIKey,
    TFunction<void(bool bIsValid, FString Error)> Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(AnthropicBaseURL);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"),       TEXT("application/json"));
    Request->SetHeader(TEXT("x-api-key"),          APIKey);
    Request->SetHeader(TEXT("anthropic-version"),  AnthropicAPIVersion);

    // Minimal validation call — 1 token, cheapest model
    Request->SetContentAsString(
        TEXT("{\"model\":\"claude-haiku-4-5-20251001\","
             "\"max_tokens\":1,"
             "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}"));

    Request->OnProcessRequestComplete().BindLambda(
        [Callback](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            if (!bSuccess || !Resp.IsValid())
            {
                Callback(false, TEXT("Network error — check your internet connection."));
                return;
            }
            int32 Code = Resp->GetResponseCode();
            if (Code == 200 || Code == 400) // 400 = valid key, bad params (still authenticated)
            {
                Callback(true, TEXT(""));
            }
            else if (Code == 401)
            {
                Callback(false, TEXT("Invalid API key. Check your key at console.anthropic.com"));
            }
            else
            {
                Callback(false, FString::Printf(TEXT("Unexpected response code: %d"), Code));
            }
        });

    Request->ProcessRequest();
}

void UDCAnthropicClient::OnMessageResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bConnectedSuccessfully)
{
    if (!bConnectedSuccessfully || !Response.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("Connection failed. Check network and API key."));
        return;
    }

    int32 StatusCode = Response->GetResponseCode();
    FString Body = Response->GetContentAsString();

    if (StatusCode != 200)
    {
        OnRequestFailed.Broadcast(
            FString::Printf(TEXT("API Error %d: %s"), StatusCode, *Body));
        return;
    }

    // Parse response JSON — extract content[0].text
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("Failed to parse API response JSON."));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* ContentArray;
    if (!JsonObject->TryGetArrayField(TEXT("content"), ContentArray) ||
        ContentArray->Num() == 0)
    {
        OnRequestFailed.Broadcast(TEXT("API response contained no content."));
        return;
    }

    FString ResponseText;
    const TSharedPtr<FJsonObject>* FirstContent;
    if ((*ContentArray)[0]->TryGetObject(FirstContent))
    {
        (*FirstContent)->TryGetStringField(TEXT("text"), ResponseText);
    }

    OnResponseReceived.Broadcast(ResponseText);
}

FString UDCAnthropicClient::BuildRequestBody(
    const FString& SystemPrompt,
    const FString& UserMessage,
    const FString& ModelID,
    int32 MaxTokens)
{
    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);
    Root->SetStringField(TEXT("model"), ModelID);
    Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
    Root->SetStringField(TEXT("system"), SystemPrompt);

    TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject);
    Message->SetStringField(TEXT("role"), TEXT("user"));
    Message->SetStringField(TEXT("content"), UserMessage);

    TArray<TSharedPtr<FJsonValue>> Messages;
    Messages.Add(MakeShareable(new FJsonValueObject(Message)));
    Root->SetArrayField(TEXT("messages"), Messages);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Output;
}
```

---

## System Prompts

### Safe Mode System Prompt
```cpp
// In UDCPromptLibrary — static FString constants

static const FString SafeModeSystemPrompt = TEXT(
R"(You are DeltaCode, an AI assistant embedded in Unreal Engine 5.5+.
You help developers write correct, idiomatic UE5 C++ and Blueprint code.

RULES:
- Always follow Epic's official C++ Coding Standard exactly.
- Use TObjectPtr<> for all UPROPERTY UObject references.
- Use BlueprintNativeEvent for functions overridable in Blueprint.
- Never put gameplay logic in constructors — use BeginPlay().
- Never suggest Blueprint as a base class for anything with planned children.
- Always recommend C++ for tick-heavy systems, base classes, and networked logic.
- Use Enhanced Input System — never legacy BindAction.
- Use Lyra naming: B_ for Blueprints, W_ for Widgets, L_ for levels.
- DeltaCode assets use DC_ in the base name: B_DC_HealthPickup.
- Format code with copyright header: // Copyright DeltaCode. All Rights Reserved.

When asked C++ vs Blueprint, apply this decision framework in order:
1. Will other classes inherit? → C++
2. Runs every tick with logic? → C++
3. Touches network/replication? → C++
4. One-off designer object? → Blueprint
5. Frequently tweaked by designers? → Blueprint
6. Heavy utility/math function? → C++ BlueprintCallable

SAFE MODE CONSTRAINT: You may ONLY generate/create. You may NEVER suggest deleting,
replacing, or modifying existing level actors. Additive only.)"
);
```

### Danger Zone System Prompt
```cpp
static const FString DangerZoneModeSystemPrompt = TEXT(
R"(You are DeltaCode in DANGER ZONE mode.
You are authorised to describe and generate complete mission frameworks for UE5 levels.
You output Python unreal scripting commands and C++ actor configurations.

DANGER ZONE CAPABILITIES:
- Clear existing level actors (except lighting, sky, atmosphere, fog, post process)
- Spawn DeltaCode mission actors: pickups, enemies, bosses, objectives, spawn zones
- Configure actor properties (location, rotation, health, patrol radius, loot tables)
- Generate the full actor hierarchy for a chosen mission template

MISSION TEMPLATES AVAILABLE:
- extraction: Extraction Zone (loot zones, enemies, extraction trigger, boss)
- arena: Arena Gauntlet (wave arenas, objective sequence, final boss)
- questhub: Quest Hub World (quest giver, objectives, ambient enemies, boss)
- reactivestory: Reactive Story World (town hub, POIs, enemy camps, dungeon, narrative triggers)

OUTPUT FORMAT: Produce clear step-by-step instructions and the Python dc_danger_zone.py
run_danger_zone() call for the requested template. Include specific actor counts,
positions, and configuration values. All generated frameworks are starter scaffolds —
not polished final content.)"
);
```

---

## Request Flow from Panel

```cpp
// In SDeltaCodePanel::OnSubmitPrompt()
FReply SDeltaCodePanel::OnSubmitPrompt()
{
    FString PromptText = PromptInputBox->GetText().ToString();
    if (PromptText.IsEmpty()) return FReply::Handled();

    FString APIKey = UDeltaCodeSettings::Get()->AnthropicAPIKey;
    FString Model  = UDeltaCodeSettings::Get()->ClaudeModel;

    FString SystemPrompt = (CurrentMode == EDeltaCodeMode::SafeMode)
        ? UDCPromptLibrary::SafeModeSystemPrompt
        : UDCPromptLibrary::DangerZoneModeSystemPrompt;

    // Show "Generating..." in response area
    ResponseTextBlock->SetText(LOCTEXT("Generating", "Generating..."));

    // Create client and bind delegates
    UDCAnthropicClient* Client = NewObject<UDCAnthropicClient>();
    Client->OnResponseReceived.AddDynamic(
        this, &SDeltaCodePanel::OnAPIResponseReceived);
    Client->OnRequestFailed.AddDynamic(
        this, &SDeltaCodePanel::OnAPIRequestFailed);

    // Keep client alive — store as member so GC doesn't collect it mid-request
    ActiveClient = Client;
    Client->SendPrompt(SystemPrompt, PromptText, APIKey, Model);

    return FReply::Handled();
}

void SDeltaCodePanel::OnAPIResponseReceived(const FString& ResponseText)
{
    // Update UI on game thread (HTTP callbacks may arrive off game thread in some builds)
    AsyncTask(ENamedThreads::GameThread, [this, ResponseText]()
    {
        ResponseTextBlock->SetText(FText::FromString(ResponseText));
        ResponseScrollBox->ScrollToEnd();
        ActiveClient = nullptr; // Release reference
    });
}
```

---

## Critical Rules

### API Key Security
- NEVER log the API key via `UE_LOG` at any verbosity level
- NEVER include the key in error messages sent to the response display
- NEVER serialize the key to any cooked or runtime-accessible config path
- ALWAYS read from `UDeltaCodeSettings::Get()->AnthropicAPIKey` — never cache in local FString members

### HTTP Thread Safety
- HTTP callbacks (`OnProcessRequestComplete`) arrive on the HTTP thread, NOT the game thread
- Any Slate UI update (SetText, SetVisibility) MUST be marshalled to the game thread via `AsyncTask(ENamedThreads::GameThread, ...)`
- Failing to marshal causes intermittent crashes that are extremely hard to debug

### UObject Lifetime During Async Requests
- `UDCAnthropicClient` must be held by a `UPROPERTY` or `TStrongObjectPtr` during the request
- Never create it as a local variable and fire a request — GC will collect it before the callback
- Pattern: `TObjectPtr<UDCAnthropicClient> ActiveClient;` as a panel member

### Model Selection
- Default model: `claude-sonnet-4-20250514` — best balance of quality and speed for code gen
- Validation ping model: `claude-haiku-4-5-20251001` — cheapest, used only for key validation
- Never hardcode model strings elsewhere — always read from `UDeltaCodeSettings`

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Storing the API key in a local FString variable as a class member
❌ Logging the API key at any log level
❌ Updating Slate UI directly from the HTTP callback thread
❌ Creating UDCAnthropicClient as a local variable before an async request
❌ Using synchronous HTTP (FHttpModule blocking calls) — freezes the Editor
❌ Calling the API with `max_tokens` above 8192 for code generation tasks
❌ Missing `anthropic-version` header — causes 400 errors
❌ Using deprecated model strings from before claude-3 era

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Anthropic API docs, UE5 HTTP module docs, UE5 Slate threading rules
Plugin: DeltaCode
Last Updated: 2026-03

// Copyright DeltaCode. All Rights Reserved.

using UnrealBuildTool;

public class DeltaCode : ModuleRules
{
    public DeltaCode(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "EnhancedInput",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "UMG",
            "InputCore",
            "AIModule",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "AssetRegistry",
            "NavigationSystem",
        });
    }
}

// Copyright DeltaCode. All Rights Reserved.

using UnrealBuildTool;

public class DeltaCodeEditor : ModuleRules
{
    public DeltaCodeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeltaCode",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "Slate",
            "SlateCore",
            "InputCore",
            "EditorSubsystem",
            "ToolMenus",
            "HTTP",
            "Json",
            "JsonUtilities",
            "LevelEditor",
            "DeveloperSettings",
            "PythonScriptPlugin",
            "Projects",
            "SourceControl",
            "AIModule",
            "AIGraph",
            "BehaviorTreeEditor",
            "SubobjectDataInterface",
        });
    }
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CarrierLabEditor : ModuleRules
{
	public CarrierLabEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CarrierLab"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorFramework",
			"InputCore",
			"LevelEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}

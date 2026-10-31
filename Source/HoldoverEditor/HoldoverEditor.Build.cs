// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HoldoverEditor : ModuleRules
{
	public HoldoverEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Holdover",
			"ToolsetRegistry"
		});

		PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "Holdover"));
	}
}

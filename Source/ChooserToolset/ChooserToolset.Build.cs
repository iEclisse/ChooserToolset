// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

using UnrealBuildTool;

public class ChooserToolset : ModuleRules
{
	public ChooserToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Chooser",
				"Core",
				"CoreUObject",
				"Engine",
				"ToolsetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"GameplayTags",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}

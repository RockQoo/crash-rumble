using UnrealBuildTool;

public class CrashRumble : ModuleRules
{
	public CrashRumble(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "DeveloperSettings", "MeshDescription", "StaticMeshDescription", "MeshConversion", "Json", "RenderCore", "RHI" });

		PublicIncludePaths.Add(ModuleDirectory);
	}
}

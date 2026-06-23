using UnrealBuildTool;

public class OpenClawBlenderGeometryNodes : ModuleRules
{
	public OpenClawBlenderGeometryNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent",
			"Projects",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore",
			"RHI",
			"RenderCore"
		});
	}
}

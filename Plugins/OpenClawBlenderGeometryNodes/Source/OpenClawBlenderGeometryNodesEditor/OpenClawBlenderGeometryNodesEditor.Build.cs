using UnrealBuildTool;

public class OpenClawBlenderGeometryNodesEditor : ModuleRules
{
	public OpenClawBlenderGeometryNodesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenClawBlenderGeometryNodes",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"AssetTools",
			"ContentBrowser",
			"EditorScriptingUtilities",
			"InputCore",
			"Json",
			"JsonUtilities",
			"MeshDescription",
			"Projects",
			"PropertyEditor",
			"RawMesh",
			"RenderCore",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			"ToolMenus",
			"UnrealEd"
		});
	}
}

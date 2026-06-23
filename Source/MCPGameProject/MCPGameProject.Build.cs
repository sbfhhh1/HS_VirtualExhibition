using UnrealBuildTool;

public class MCPGameProject : ModuleRules
{
    public MCPGameProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "UMG",
            "OpenClawBlenderGeometryNodes",
            "ProceduralMeshComponent",
            "Niagara",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "Json",
            "UltraleapTracking",
            "AudioExtensions",
        });
    }
}

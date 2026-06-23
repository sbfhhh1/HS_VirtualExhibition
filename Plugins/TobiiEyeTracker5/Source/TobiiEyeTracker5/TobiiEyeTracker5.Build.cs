using UnrealBuildTool;
using System.IO;

public class TobiiEyeTracker5 : ModuleRules
{
	public TobiiEyeTracker5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG" });
		PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "Projects", "Slate", "SlateCore", "XmlParser" });

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DllPath = Path.Combine(PluginDirectory, "ThirdParty", "Win64", "Tobii.GameIntegration.dll");
			RuntimeDependencies.Add("$(BinaryOutputDir)/Tobii.GameIntegration.dll", DllPath, StagedFileType.NonUFS);
		}
	}
}

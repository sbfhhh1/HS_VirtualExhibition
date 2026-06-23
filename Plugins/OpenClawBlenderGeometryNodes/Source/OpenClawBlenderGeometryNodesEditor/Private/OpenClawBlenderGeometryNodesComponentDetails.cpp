#include "OpenClawBlenderGeometryNodesComponentDetails.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "Components/StaticMeshComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/FileManager.h"
#include "IDetailChildrenBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "StaticMeshAttributes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenClawBlenderGeometryNodesComponentDetails"

namespace
{
struct FBlenderGNScannedCandidate
{
	FString ObjectName;
	FString ModifierName;
	FString NodeGroupName;
	TArray<FBlenderGNInput> Inputs;
};

FString ToAsciiLabel(const FString& Value)
{
	if (Value.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("Input");
	}

	FString Result;
	Result.Reserve(Value.Len());
	bool bPreviousSpace = false;
	for (const TCHAR Char : Value)
	{
		if (Char >= 32 && Char <= 126)
		{
			if (Char == TEXT('?'))
			{
				if (!bPreviousSpace)
				{
					Result.AppendChar(TEXT(' '));
					bPreviousSpace = true;
				}
				continue;
			}
			Result.AppendChar(Char);
			bPreviousSpace = (Char == TEXT(' '));
		}
		else if (!bPreviousSpace)
		{
			Result.AppendChar(TEXT(' '));
			bPreviousSpace = true;
		}
	}

	Result = Result.TrimStartAndEnd();
	return Result.IsEmpty() ? TEXT("Input") : Result;
}

FString SanitizeAssetToken(const FString& Value, const FString& Fallback)
{
	FString Result;
	Result.Reserve(Value.Len());
	for (const TCHAR Char : Value)
	{
		if (FChar::IsAlnum(Char) || Char == TEXT('_'))
		{
			Result.AppendChar(Char);
		}
		else if (Char == TEXT(' ') || Char == TEXT('-') || Char == TEXT('.'))
		{
			Result.AppendChar(TEXT('_'));
		}
	}
	Result = Result.TrimStartAndEnd();
	return Result.IsEmpty() ? Fallback : Result;
}

float ParseFloat(const FString& Value, float Fallback)
{
	float Parsed = 0.0f;
	return LexTryParseString(Parsed, *Value) ? Parsed : Fallback;
}

FVector ParseVector(const FString& Value, const FVector& Fallback)
{
	FString Cleaned = Value;
	Cleaned.ReplaceInline(TEXT("("), TEXT(""));
	Cleaned.ReplaceInline(TEXT(")"), TEXT(""));
	Cleaned.ReplaceInline(TEXT("["), TEXT(""));
	Cleaned.ReplaceInline(TEXT("]"), TEXT(""));

	TArray<FString> Parts;
	Cleaned.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() < 3)
	{
		return Fallback;
	}

	return FVector(
		ParseFloat(Parts[0], Fallback.X),
		ParseFloat(Parts[1], Fallback.Y),
		ParseFloat(Parts[2], Fallback.Z));
}

FBlenderGNInput CreateInputFromScan(const TArray<FString>& Parts)
{
	FBlenderGNInput Input;
	const FString Name = ToAsciiLabel(Parts.IsValidIndex(1) ? Parts[1] : TEXT("Input"));
	const FString SocketType = Parts.IsValidIndex(3) ? Parts[3] : FString();
	const FString DefaultValue = Parts.IsValidIndex(4) ? Parts[4] : FString();

	Input.DisplayName = Name;
	Input.SocketName = Name;
	Input.FallbackIdentifier = Parts.IsValidIndex(2) ? Parts[2] : FString();
	Input.bShowInRuntimePanel = true;
	Input.SliderMin = 0.0f;
	Input.SliderMax = 10.0f;

	if (SocketType.Contains(TEXT("Vector"), ESearchCase::IgnoreCase))
	{
		Input.Type = EBlenderGNParameterType::Vector3;
		Input.VectorValue = ParseVector(DefaultValue, FVector::OneVector);
	}
	else if (SocketType.Contains(TEXT("Int"), ESearchCase::IgnoreCase))
	{
		Input.Type = EBlenderGNParameterType::Int;
		Input.IntValue = FMath::RoundToInt(ParseFloat(DefaultValue, 1.0f));
		Input.SliderMax = FMath::Max(10.0f, FMath::Abs(static_cast<float>(Input.IntValue)) * 2.0f);
	}
	else if (SocketType.Contains(TEXT("Bool"), ESearchCase::IgnoreCase))
	{
		Input.Type = EBlenderGNParameterType::Bool;
		Input.BoolValue = DefaultValue.Equals(TEXT("True"), ESearchCase::IgnoreCase) || DefaultValue.Equals(TEXT("1"));
	}
	else
	{
		Input.Type = EBlenderGNParameterType::Float;
		Input.FloatValue = ParseFloat(DefaultValue, 0.0f);
		Input.SliderMax = FMath::Max(1.0f, FMath::Abs(Input.FloatValue) * 2.0f);
	}

	return Input;
}

TArray<FBlenderGNScannedCandidate> ParseScanOutput(const FString& Output)
{
	TArray<FBlenderGNScannedCandidate> Candidates;
	FBlenderGNScannedCandidate* Current = nullptr;

	TArray<FString> Lines;
	Output.ParseIntoArrayLines(Lines, true);
	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (Line.StartsWith(TEXT("CANDIDATE|")))
		{
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT("|"), false);
			FBlenderGNScannedCandidate Candidate;
			Candidate.ObjectName = Parts.IsValidIndex(1) ? Parts[1] : FString();
			Candidate.ModifierName = Parts.IsValidIndex(2) ? Parts[2] : FString();
			Candidate.NodeGroupName = Parts.IsValidIndex(3) ? Parts[3] : FString();
			Current = &Candidates.Add_GetRef(MoveTemp(Candidate));
		}
		else if (Line.StartsWith(TEXT("INPUT|")) && Current)
		{
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT("|"), false);
			Current->Inputs.Add(CreateInputFromScan(Parts));
		}
	}

	return Candidates;
}

FString BuildScanScript()
{
	return TEXT(R"(import bpy
def socket_value(mod, item):
    ident = getattr(item, 'identifier', '')
    if ident and ident in mod:
        value = mod[ident]
    else:
        value = getattr(item, 'default_value', '')
    try:
        if not isinstance(value, str) and hasattr(value, '__len__'):
            return ','.join(str(float(v)) for v in value)
    except Exception:
        pass
    return str(value)
print('OCGN_SCAN_BEGIN')
for obj in bpy.data.objects:
    for mod in obj.modifiers:
        if mod.type != 'NODES' or not getattr(mod, 'node_group', None):
            continue
        ng = mod.node_group
        print('CANDIDATE|' + obj.name + '|' + mod.name + '|' + ng.name)
        for item in ng.interface.items_tree:
            if getattr(item, 'item_type', None) != 'SOCKET' or getattr(item, 'in_out', None) != 'INPUT':
                continue
            if getattr(item, 'socket_type', '') == 'NodeSocketGeometry':
                continue
            print('INPUT|' + item.name + '|' + getattr(item, 'identifier', '') + '|' + getattr(item, 'socket_type', '') + '|' + socket_value(mod, item))
print('OCGN_SCAN_END')
)");
}

bool RunBlenderScan(UBlenderGeometryNodesComponent* Component, TArray<FBlenderGNScannedCandidate>& OutCandidates, FString& OutError)
{
	if (!Component)
	{
		OutError = TEXT("No Blender Geometry Nodes component selected.");
		return false;
	}
	if (Component->BlenderExecutable.IsEmpty() || !FPaths::FileExists(Component->BlenderExecutable))
	{
		OutError = TEXT("Blender executable is missing or invalid.");
		return false;
	}
	if (Component->BlendFile.FilePath.IsEmpty() || !FPaths::FileExists(Component->BlendFile.FilePath))
	{
		OutError = TEXT("Blend file is missing or invalid.");
		return false;
	}

	const FString WorkDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("OpenClawBlenderGeometryNodes"));
	IFileManager::Get().MakeDirectory(*WorkDir, true);
	const FString ScriptPath = FPaths::Combine(WorkDir, TEXT("ocgn_scan.py"));
	if (!FFileHelper::SaveStringToFile(BuildScanScript(), *ScriptPath))
	{
		OutError = TEXT("Failed to write temporary Geometry Nodes scan script.");
		return false;
	}

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	const FString Arguments = FString::Printf(TEXT("-b \"%s\" --python \"%s\""), *Component->BlendFile.FilePath, *ScriptPath);
	FProcHandle Process = FPlatformProcess::CreateProc(*Component->BlenderExecutable, *Arguments, true, true, true, nullptr, 0, *FPaths::GetPath(Component->BlendFile.FilePath), WritePipe);
	if (!Process.IsValid())
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		OutError = TEXT("Failed to start Blender for scan.");
		return false;
	}

	FString Output;
	const double StartTime = FPlatformTime::Seconds();
	const double TimeoutSeconds = FMath::Max(1, Component->BlenderTimeoutSeconds);
	bool bTimedOut = false;
	while (FPlatformProcess::IsProcRunning(Process))
	{
		Output += FPlatformProcess::ReadPipe(ReadPipe);
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			FPlatformProcess::TerminateProc(Process, true);
			bTimedOut = true;
			break;
		}
		FPlatformProcess::Sleep(0.02f);
	}
	Output += FPlatformProcess::ReadPipe(ReadPipe);

	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(Process, &ReturnCode);
	FPlatformProcess::CloseProc(Process);
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (bTimedOut)
	{
		OutError = FString::Printf(TEXT("Timed out while scanning Geometry Nodes after %ds."), Component->BlenderTimeoutSeconds);
		return false;
	}
	if (ReturnCode != 0)
	{
		OutError = FString::Printf(TEXT("Blender scan failed. ExitCode=%d\n%s"), ReturnCode, *Output);
		return false;
	}

	OutCandidates = ParseScanOutput(Output);
	if (OutCandidates.Num() == 0)
	{
		OutError = TEXT("No Geometry Nodes modifiers were found in this blend file.");
		return false;
	}
	return true;
}

const FBlenderGNScannedCandidate& PickCandidate(const UBlenderGeometryNodesComponent* Component, const TArray<FBlenderGNScannedCandidate>& Candidates)
{
	for (const FBlenderGNScannedCandidate& Candidate : Candidates)
	{
		const bool bObjectMatches = Component->ObjectName.IsEmpty() || Candidate.ObjectName.Equals(Component->ObjectName, ESearchCase::CaseSensitive);
		const bool bModifierMatches = Component->ModifierName.IsEmpty() || Candidate.ModifierName.Equals(Component->ModifierName, ESearchCase::CaseSensitive);
		if (bObjectMatches && bModifierMatches)
		{
			return Candidate;
		}
	}
	return Candidates[0];
}

bool BuildMeshDescriptionFromGNData(const FBlenderGNMeshData& MeshData, FMeshDescription& MeshDescription, TArray<FStaticMaterial>& OutMaterials)
{
	if (MeshData.Vertices.Num() == 0 || MeshData.Triangles.Num() < 3)
	{
		return false;
	}

	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	VertexInstanceUVs.SetNumChannels(1);

	TArray<FVertexID> VertexIDs;
	VertexIDs.Reserve(MeshData.Vertices.Num());
	MeshDescription.ReserveNewVertices(MeshData.Vertices.Num());
	MeshDescription.ReserveNewVertexInstances(MeshData.Triangles.Num());
	MeshDescription.ReserveNewPolygons(MeshData.Triangles.Num() / 3);
	for (const FVector& Vertex : MeshData.Vertices)
	{
		const FVertexID VertexID = MeshDescription.CreateVertex();
		VertexPositions[VertexID] = FVector3f(Vertex);
		VertexIDs.Add(VertexID);
	}

	const int32 SectionCount = FMath::Max(1, MeshData.Sections.Num());
	TArray<FPolygonGroupID> PolygonGroups;
	PolygonGroups.Reserve(SectionCount);
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FString MaterialName = MeshData.Materials.IsValidIndex(SectionIndex) && !MeshData.Materials[SectionIndex].Name.IsEmpty()
			? SanitizeAssetToken(MeshData.Materials[SectionIndex].Name, FString::Printf(TEXT("Material_%d"), SectionIndex))
			: FString::Printf(TEXT("Material_%d"), SectionIndex);
		const FName SlotName(*MaterialName);
		const FPolygonGroupID PolygonGroupID = MeshDescription.CreatePolygonGroup();
		PolygonGroupMaterialSlotNames[PolygonGroupID] = SlotName;
		PolygonGroups.Add(PolygonGroupID);
		OutMaterials.Add(FStaticMaterial(nullptr, SlotName, SlotName));
	}

	auto CreateTriangle = [&](FPolygonGroupID PolygonGroupID, int32 A, int32 B, int32 C)
	{
		if (!VertexIDs.IsValidIndex(A) || !VertexIDs.IsValidIndex(B) || !VertexIDs.IsValidIndex(C))
		{
			return;
		}

		FVertexInstanceID VertexInstanceIDs[3];
		const int32 Indices[3] = { A, B, C };
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 VertexIndex = Indices[Corner];
			const FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexIDs[VertexIndex]);
			const FVector Normal = MeshData.Normals.IsValidIndex(VertexIndex) ? MeshData.Normals[VertexIndex] : FVector::UpVector;
			const FVector2D UV = MeshData.UVs.IsValidIndex(VertexIndex) ? MeshData.UVs[VertexIndex] : FVector2D::ZeroVector;
			VertexInstanceNormals[VertexInstanceID] = FVector3f(Normal);
			VertexInstanceUVs.Set(VertexInstanceID, 0, FVector2f(UV));
			VertexInstanceColors[VertexInstanceID] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
			VertexInstanceTangents[VertexInstanceID] = FVector3f(1.0f, 0.0f, 0.0f);
			VertexInstanceBinormalSigns[VertexInstanceID] = 1.0f;
			VertexInstanceIDs[Corner] = VertexInstanceID;
		}
		MeshDescription.CreateTriangle(PolygonGroupID, VertexInstanceIDs);
	};

	if (MeshData.Sections.Num() > 0)
	{
		for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
		{
			const FPolygonGroupID GroupID = PolygonGroups.IsValidIndex(SectionIndex) ? PolygonGroups[SectionIndex] : PolygonGroups[0];
			const TArray<int32>& SectionTriangles = MeshData.Sections[SectionIndex].Triangles;
			for (int32 Index = 0; Index + 2 < SectionTriangles.Num(); Index += 3)
			{
				CreateTriangle(GroupID, SectionTriangles[Index], SectionTriangles[Index + 1], SectionTriangles[Index + 2]);
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index + 2 < MeshData.Triangles.Num(); Index += 3)
		{
			CreateTriangle(PolygonGroups[0], MeshData.Triangles[Index], MeshData.Triangles[Index + 1], MeshData.Triangles[Index + 2]);
		}
	}

	return MeshDescription.Triangles().Num() > 0;
}

UMaterialInterface* FindComponentMaterial(const UBlenderGeometryNodesComponent* Component, int32 SectionIndex)
{
	if (!Component)
	{
		return nullptr;
	}
	if (Component->MaterialOverrides.IsValidIndex(SectionIndex) && Component->MaterialOverrides[SectionIndex])
	{
		return Component->MaterialOverrides[SectionIndex];
	}
	return Component->FallbackMaterial;
}
}

TSharedRef<IDetailCustomization> FOpenClawBlenderGeometryNodesComponentDetails::MakeInstance()
{
	return MakeShared<FOpenClawBlenderGeometryNodesComponentDetails>();
}

void FOpenClawBlenderGeometryNodesComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomizedComponents.Reset();
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		if (UBlenderGeometryNodesComponent* Component = Cast<UBlenderGeometryNodesComponent>(Object.Get()))
		{
			CustomizedComponents.Add(Component);
		}
	}

	IDetailCategoryBuilder& Actions = DetailBuilder.EditCategory("OpenClaw Actions", LOCTEXT("ActionsCategory", "OpenClaw Actions"), ECategoryPriority::Important);
	Actions.AddCustomRow(LOCTEXT("ActionButtonsFilter", "Scan Clean Refresh Bake"))
	.WholeRowContent()
	[
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(2.0f))
		+ SUniformGridPanel::Slot(0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ScanBlend", "Scan Blend"))
			.ToolTipText(LOCTEXT("ScanBlendTooltip", "Launch Blender and scan Geometry Nodes modifiers and exposed inputs."))
			.IsEnabled(this, &FOpenClawBlenderGeometryNodesComponentDetails::HasSingleComponent)
			.OnClicked(this, &FOpenClawBlenderGeometryNodesComponentDetails::ScanBlend)
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("CleanLabels", "Clean Labels"))
			.ToolTipText(LOCTEXT("CleanLabelsTooltip", "Normalize scanned input labels to editor-safe ASCII labels."))
			.IsEnabled(this, &FOpenClawBlenderGeometryNodesComponentDetails::HasSingleComponent)
			.OnClicked(this, &FOpenClawBlenderGeometryNodesComponentDetails::CleanLabels)
		]
		+ SUniformGridPanel::Slot(2, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("RefreshPreview", "Refresh Preview"))
			.ToolTipText(LOCTEXT("RefreshPreviewTooltip", "Request a fresh Blender evaluation for the procedural preview mesh."))
			.IsEnabled(this, &FOpenClawBlenderGeometryNodesComponentDetails::HasSingleComponent)
			.OnClicked(this, &FOpenClawBlenderGeometryNodesComponentDetails::RefreshPreview)
		]
		+ SUniformGridPanel::Slot(3, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("BakeStaticMesh", "Bake Static Mesh"))
			.ToolTipText(LOCTEXT("BakeStaticMeshTooltip", "Bake the latest procedural mesh output into a Static Mesh asset and scene actor."))
			.IsEnabled(this, &FOpenClawBlenderGeometryNodesComponentDetails::HasSingleComponent)
			.OnClicked(this, &FOpenClawBlenderGeometryNodesComponentDetails::BakeStaticMesh)
		]
	];

	IDetailCategoryBuilder& Status = DetailBuilder.EditCategory("Status");
	Status.AddCustomRow(LOCTEXT("StatusSummaryFilter", "OpenClaw Status"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text_Lambda([this]()
		{
			const UBlenderGeometryNodesComponent* Component = GetSingleComponent();
			if (!Component)
			{
				return LOCTEXT("MultipleComponents", "Select a single Blender Geometry Nodes component to use editor actions.");
			}
			return FText::Format(
				LOCTEXT("StatusSummary", "{0} | {1} vertices | {2} triangles | {3}s"),
				FText::FromString(Component->LastStatus),
				FText::AsNumber(Component->LastVertexCount),
				FText::AsNumber(Component->LastTriangleCount),
				FText::AsNumber(Component->LastEvaluationSeconds));
		})
	];
}

bool FOpenClawBlenderGeometryNodesComponentDetails::HasSingleComponent() const
{
	return GetSingleComponent() != nullptr;
}

UBlenderGeometryNodesComponent* FOpenClawBlenderGeometryNodesComponentDetails::GetSingleComponent() const
{
	return CustomizedComponents.Num() == 1 ? CustomizedComponents[0].Get() : nullptr;
}

FReply FOpenClawBlenderGeometryNodesComponentDetails::ScanBlend()
{
	UBlenderGeometryNodesComponent* Component = GetSingleComponent();
	TArray<FBlenderGNScannedCandidate> Candidates;
	FString Error;
	if (!RunBlenderScan(Component, Candidates, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error), LOCTEXT("ScanFailedTitle", "Geometry Nodes Scan"));
		return FReply::Handled();
	}

	const FBlenderGNScannedCandidate& Selected = PickCandidate(Component, Candidates);
	const FScopedTransaction Transaction(LOCTEXT("ScanBlendTransaction", "Scan Blender Geometry Nodes Inputs"));
	Component->Modify();
	Component->ObjectName = Selected.ObjectName;
	Component->ModifierName = Selected.ModifierName;
	Component->Inputs = Selected.Inputs;
	Component->LastStatus = FString::Printf(TEXT("Scanned %d inputs from %s / %s."), Selected.Inputs.Num(), *Selected.ObjectName, *Selected.ModifierName);
	Component->NotifyParametersChanged();

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(LOCTEXT("ScanSucceeded", "Scanned {0} input(s) from {1} / {2}.\nFound {3} Geometry Nodes candidate(s)."),
			FText::AsNumber(Selected.Inputs.Num()),
			FText::FromString(Selected.ObjectName),
			FText::FromString(Selected.ModifierName),
			FText::AsNumber(Candidates.Num())),
		LOCTEXT("ScanSucceededTitle", "Geometry Nodes Scan"));
	return FReply::Handled();
}

FReply FOpenClawBlenderGeometryNodesComponentDetails::CleanLabels()
{
	UBlenderGeometryNodesComponent* Component = GetSingleComponent();
	if (!Component)
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("CleanLabelsTransaction", "Clean Blender Geometry Nodes Labels"));
	Component->Modify();
	for (FBlenderGNInput& Input : Component->Inputs)
	{
		Input.DisplayName = ToAsciiLabel(Input.DisplayName);
		const FString CleanSocket = ToAsciiLabel(Input.SocketName);
		Input.SocketName = CleanSocket.Equals(TEXT("Input")) && !Input.DisplayName.Equals(TEXT("Input")) ? Input.DisplayName : CleanSocket;
		if (!Input.SocketName.IsEmpty())
		{
			Input.DisplayName = Input.SocketName;
		}
	}
	Component->LastStatus = TEXT("Cleaned Geometry Nodes input labels.");
	Component->NotifyParametersChanged();
	return FReply::Handled();
}

FReply FOpenClawBlenderGeometryNodesComponentDetails::RefreshPreview()
{
	if (UBlenderGeometryNodesComponent* Component = GetSingleComponent())
	{
		Component->RefreshNow();
	}
	return FReply::Handled();
}

FReply FOpenClawBlenderGeometryNodesComponentDetails::BakeStaticMesh()
{
	UBlenderGeometryNodesComponent* Component = GetSingleComponent();
	if (!Component)
	{
		return FReply::Handled();
	}

	const FBlenderGNMeshData& MeshData = Component->GetLastMeshData();
	if (MeshData.Vertices.Num() == 0 || MeshData.GetTriangleCount() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BakeNoMesh", "No procedural mesh data is available. Refresh Preview before baking."), LOCTEXT("BakeTitle", "Bake Static Mesh"));
		return FReply::Handled();
	}

	FMeshDescription MeshDescription;
	TArray<FStaticMaterial> StaticMaterials;
	if (!BuildMeshDescriptionFromGNData(MeshData, MeshDescription, StaticMaterials))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BakeMeshDescriptionFailed", "Failed to convert procedural mesh data into a MeshDescription."), LOCTEXT("BakeTitle", "Bake Static Mesh"));
		return FReply::Handled();
	}

	for (int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
	{
		StaticMaterials[MaterialIndex].MaterialInterface = FindComponentMaterial(Component, MaterialIndex);
	}

	const FString BlendToken = SanitizeAssetToken(FPaths::GetBaseFilename(Component->BlendFile.FilePath), TEXT("BlenderGN"));
	const FString TimeToken = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FolderPath = FString::Printf(TEXT("/Game/Generated/BlenderGN/%s/%s"), *BlendToken, *TimeToken);
	const FString AssetName = SanitizeAssetToken(FString::Printf(TEXT("SM_%s_%s"), *BlendToken, *MeshData.ObjectName), TEXT("SM_BlenderGN"));
	const FString PackageName = FolderPath / AssetName;
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("BakeInvalidPackage", "Invalid bake package path: {0}"), FText::FromString(PackageName)), LOCTEXT("BakeTitle", "Bake Static Mesh"));
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("BakeStaticMeshTransaction", "Bake Blender Geometry Nodes Static Mesh"));
	UPackage* Package = CreatePackage(*PackageName);
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	StaticMesh->SetStaticMaterials(StaticMaterials);
	StaticMesh->AddSourceModel();

	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bBuildSimpleCollision = true;
	BuildParams.bMarkPackageDirty = true;
	BuildParams.bCommitMeshDescription = true;
	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDescription);
	if (!StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BakeBuildFailed", "Static mesh build failed. The asset was not registered."), LOCTEXT("BakeTitle", "Bake Static Mesh"));
		return FReply::Handled();
	}

	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(StaticMesh);

	AStaticMeshActor* BakedActor = nullptr;
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform SpawnTransform = Component->GetOwner() ? Component->GetOwner()->GetActorTransform() : FTransform::Identity;
			BakedActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, Params);
			if (BakedActor)
			{
				BakedActor->Modify();
				BakedActor->SetActorLabel(AssetName);
				BakedActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
				for (int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
				{
					if (StaticMaterials[MaterialIndex].MaterialInterface)
					{
						BakedActor->GetStaticMeshComponent()->SetMaterial(MaterialIndex, StaticMaterials[MaterialIndex].MaterialInterface);
					}
				}
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(BakedActor, true, true);
			}
		}
	}

	Component->Modify();
	Component->LastStatus = FString::Printf(TEXT("Baked Static Mesh: %s"), *PackageName);
	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(LOCTEXT("BakeSucceeded", "Baked Static Mesh:\n{0}\n\nScene actor: {1}"),
			FText::FromString(PackageName),
			FText::FromString(BakedActor ? BakedActor->GetActorLabel() : TEXT("not created"))),
		LOCTEXT("BakeTitle", "Bake Static Mesh"));

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

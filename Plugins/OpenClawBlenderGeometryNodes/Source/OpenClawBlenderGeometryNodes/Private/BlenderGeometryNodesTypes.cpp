#include "BlenderGeometryNodesTypes.h"

namespace
{
FString CleanGNLabel(const FString& Value)
{
	FString Result;
	bool bPreviousSpace = false;
	for (const TCHAR Char : Value)
	{
		if (Char >= 32 && Char <= 126 && Char != TEXT('?'))
		{
			Result.AppendChar(Char);
			bPreviousSpace = Char == TEXT(' ');
		}
		else if (!bPreviousSpace)
		{
			Result.AppendChar(TEXT(' '));
			bPreviousSpace = true;
		}
	}
	Result.TrimStartAndEndInline();
	return Result.IsEmpty() ? TEXT("Input") : Result;
}
}

FString FBlenderGNInput::GetBestName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return SocketName.IsEmpty() ? FallbackIdentifier : SocketName;
}

bool FBlenderGNInput::MatchesName(const FString& Query) const
{
	if (Query.IsEmpty())
	{
		return false;
	}

	const FString CleanQuery = CleanGNLabel(Query);
	const FString Names[] = { SocketName, DisplayName, FallbackIdentifier };
	for (const FString& Name : Names)
	{
		if (Name.Equals(Query, ESearchCase::CaseSensitive) ||
			CleanGNLabel(Name).Equals(CleanQuery, ESearchCase::IgnoreCase) ||
			CleanGNLabel(Name).Contains(CleanQuery, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void FBlenderGNMeshData::BuildSections()
{
	Sections.Reset();
	const int32 TriangleCount = Triangles.Num() / 3;
	int32 MaxMaterialIndex = 0;
	for (int32 MaterialIndex : MaterialIndices)
	{
		MaxMaterialIndex = FMath::Max(MaxMaterialIndex, MaterialIndex);
	}
	Sections.SetNum(FMath::Max(1, MaxMaterialIndex + 1));

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const int32 SectionIndex = MaterialIndices.IsValidIndex(TriangleIndex)
			? FMath::Clamp(MaterialIndices[TriangleIndex], 0, Sections.Num() - 1)
			: 0;
		const int32 Source = TriangleIndex * 3;
		Sections[SectionIndex].Triangles.Add(Triangles[Source]);
		Sections[SectionIndex].Triangles.Add(Triangles[Source + 1]);
		Sections[SectionIndex].Triangles.Add(Triangles[Source + 2]);
	}
}

void FBlenderGNMeshData::Reset()
{
	ObjectName.Reset();
	Vertices.Reset();
	Normals.Reset();
	bImportedNormals = false;
	UVs.Reset();
	Triangles.Reset();
	MaterialIndices.Reset();
	Materials.Reset();
	Sections.Reset();
}

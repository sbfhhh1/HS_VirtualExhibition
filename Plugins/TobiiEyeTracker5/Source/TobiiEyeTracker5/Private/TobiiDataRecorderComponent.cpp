#include "TobiiDataRecorderComponent.h"

#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "TobiiEyeTrackerSubsystem.h"

UTobiiDataRecorderComponent::UTobiiDataRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTobiiDataRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRecording();
	Super::EndPlay(EndPlayReason);
}

bool UTobiiDataRecorderComponent::StartRecording()
{
	if (bRecording)
	{
		return true;
	}
	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), OutputFolder);
	IFileManager::Get().MakeDirectory(*Folder, true);
	OutputPath = FPaths::Combine(Folder, FString::Printf(TEXT("Tobii_%s.xml"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
	Writer.Reset(IFileManager::Get().CreateFileWriter(*OutputPath));
	if (!Writer)
	{
		return false;
	}
	bRecording = true;
	WriteLine(TEXT("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
	WriteLine(TEXT("<TobiiData source=\"Tobii Game Integration\" researchRawData=\"false\">"));
	RecordEvent(TEXT("RecordingStarted"), OutputPath);
	return true;
}

void UTobiiDataRecorderComponent::StopRecording()
{
	if (!bRecording)
	{
		return;
	}
	RecordEvent(TEXT("RecordingStopped"));
	WriteLine(TEXT("</TobiiData>"));
	Writer->Close();
	Writer.Reset();
	bRecording = false;
}

void UTobiiDataRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRecording || !GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}
	const UTobiiEyeTrackerSubsystem* Tobii = GetWorld()->GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>();
	if (!Tobii)
	{
		return;
	}
	const FTobiiGazeSample Gaze = Tobii->GetLatestGaze();
	const FTobiiHeadPoseSample Head = Tobii->GetLatestHeadPose();
	WriteLine(FString::Printf(
		TEXT("  <Sample time=\"%.6f\" gazeValid=\"%s\" gazeX=\"%.6f\" gazeY=\"%.6f\" headValid=\"%s\" headX=\"%.3f\" headY=\"%.3f\" headZ=\"%.3f\" pitch=\"%.3f\" yaw=\"%.3f\" roll=\"%.3f\" presence=\"%d\" target=\"%s\" dwell=\"%.3f\" />"),
		GetWorld()->GetTimeSeconds(), Gaze.bValid ? TEXT("true") : TEXT("false"), Gaze.DisplayPoint.X, Gaze.DisplayPoint.Y,
		Head.bValid ? TEXT("true") : TEXT("false"), Head.Position.X, Head.Position.Y, Head.Position.Z,
		Head.Rotation.Pitch, Head.Rotation.Yaw, Head.Rotation.Roll, static_cast<int32>(Tobii->GetUserPresence()),
		*EscapeXml(CurrentTarget), DwellProgress));
}

void UTobiiDataRecorderComponent::RecordEvent(const FString& EventName, const FString& Value)
{
	if (bRecording)
	{
		WriteLine(FString::Printf(TEXT("  <Event time=\"%.6f\" name=\"%s\" value=\"%s\" />"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, *EscapeXml(EventName), *EscapeXml(Value)));
	}
}

void UTobiiDataRecorderComponent::WriteLine(const FString& Line)
{
	if (!Writer)
	{
		return;
	}
	FTCHARToUTF8 Utf8(*(Line + LINE_TERMINATOR));
	Writer->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
}

FString UTobiiDataRecorderComponent::EscapeXml(const FString& Value)
{
	FString Result = Value;
	Result.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Result.ReplaceInline(TEXT("\""), TEXT("&quot;"));
	Result.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Result.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	return Result;
}

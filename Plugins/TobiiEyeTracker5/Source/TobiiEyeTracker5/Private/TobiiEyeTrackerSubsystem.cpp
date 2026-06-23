#include "TobiiEyeTrackerSubsystem.h"

#include "Async/Async.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Windows/WindowsHWrapper.h"

namespace TobiiNative
{
	enum class ESubscription : int32 { UserPresence = 1 << 1, StandardGaze = 1 << 2, HeadTracking = 1 << 4 };
	enum class EUnitType : int32 { Normalized = 1 };
	struct FGazePoint { int64 Timestamp; float X; float Y; };
	struct FHeadRotation { float Yaw; float Pitch; float Roll; };
	struct FHeadPosition { float X; float Y; float Z; };
	struct FHeadPose { int64 Timestamp; FHeadRotation Rotation; FHeadPosition Position; };

	using FStart = bool(*)(bool);
	using FSetWindow = void(*)(void*);
	using FStop = void(*)();
	using FSubscribe = void(*)(ESubscription);
	using FUnsubscribe = void(*)(ESubscription);
	using FUpdate = bool(*)();
	using FGetGaze = void(*)(FGazePoint*&, int32&, EUnitType);
	using FGetHeadPose = void(*)(FHeadPose*&, int32&);
	using FBoolQuery = bool(*)();
	using FFloatQuery = float(*)();
	using FGetPresence = int32(*)();

	FStart Start = nullptr;
	FSetWindow SetWindow = nullptr;
	FStop Stop = nullptr;
	FSubscribe Subscribe = nullptr;
	FUnsubscribe Unsubscribe = nullptr;
	FUpdate Update = nullptr;
	FGetGaze GetGaze = nullptr;
	FGetHeadPose GetHeadPose = nullptr;
	FBoolQuery IsInitialised = nullptr;
	FBoolQuery IsReady = nullptr;
	FBoolQuery IsConnected = nullptr;
	FFloatQuery TimeSinceLastGaze = nullptr;
	FFloatQuery TimeSinceLastHead = nullptr;
	FGetPresence GetPresence = nullptr;

	void Reset()
	{
		Start = nullptr; SetWindow = nullptr; Stop = nullptr; Subscribe = nullptr; Unsubscribe = nullptr;
		Update = nullptr; GetGaze = nullptr; GetHeadPose = nullptr; IsInitialised = nullptr; IsReady = nullptr;
		IsConnected = nullptr; TimeSinceLastGaze = nullptr; TimeSinceLastHead = nullptr; GetPresence = nullptr;
	}
}

void UTobiiEyeTrackerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: subsystem initialized; starting native integration asynchronously."));
	StartTracking();
}

void UTobiiEyeTrackerSubsystem::Deinitialize()
{
	StopTracking();
	if (!bStartInProgress)
	{
		UnloadIntegration();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TobiiEyeTracker5: native Start is still pending during shutdown; DLL will remain loaded until process exit."));
	}
	Super::Deinitialize();
}

TStatId UTobiiEyeTrackerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTobiiEyeTrackerSubsystem, STATGROUP_Tickables);
}

void UTobiiEyeTrackerSubsystem::Tick(float DeltaTime)
{
	if (bSimulationEnabled)
	{
		UpdateSimulation(DeltaTime);
		return;
	}
	if (bStartInProgress && StartFuture.IsReady())
	{
		CompleteStartTracking();
	}
	if (!bStarted && !bStartAttempted)
	{
		StartTracking();
	}
	if (bAvailable)
	{
		UpdateDevice(DeltaTime);
	}
}

bool UTobiiEyeTrackerSubsystem::LoadIntegration()
{
#if PLATFORM_WINDOWS
	if (DllHandle)
	{
		return true;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TobiiEyeTracker5"));
	const FString PluginBase = Plugin.IsValid() ? Plugin->GetBaseDir() : FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TobiiEyeTracker5"));
	const TArray<FString> Candidates = {
		FPaths::Combine(PluginBase, TEXT("Binaries/Win64/Tobii.GameIntegration.dll")),
		FPaths::Combine(PluginBase, TEXT("ThirdParty/Win64/Tobii.GameIntegration.dll")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/Tobii.GameIntegration.dll"))
	};
	FString DllPath;
	for (const FString& Candidate : Candidates)
	{
		if (FPaths::FileExists(Candidate))
		{
			DllPath = Candidate;
			DllHandle = FPlatformProcess::GetDllHandle(*DllPath);
			if (DllHandle)
			{
				UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: loaded native integration from %s"), *DllPath);
				break;
			}
		}
	}
	if (!DllHandle)
	{
		ReportError(FString::Printf(TEXT("Tobii Game Integration DLL not found under plugin path: %s"), *PluginBase));
		return false;
	}

	auto Export = [this](const TCHAR* Name) { return FPlatformProcess::GetDllExport(DllHandle, Name); };
	TobiiNative::Start = reinterpret_cast<TobiiNative::FStart>(Export(TEXT("Start")));
	TobiiNative::SetWindow = reinterpret_cast<TobiiNative::FSetWindow>(Export(TEXT("SetWindow")));
	TobiiNative::Stop = reinterpret_cast<TobiiNative::FStop>(Export(TEXT("Stop")));
	TobiiNative::Subscribe = reinterpret_cast<TobiiNative::FSubscribe>(Export(TEXT("SubscribeToStream")));
	TobiiNative::Unsubscribe = reinterpret_cast<TobiiNative::FUnsubscribe>(Export(TEXT("UnsubscribeFromStream")));
	TobiiNative::Update = reinterpret_cast<TobiiNative::FUpdate>(Export(TEXT("Update")));
	TobiiNative::GetGaze = reinterpret_cast<TobiiNative::FGetGaze>(Export(TEXT("GetNewGazePoints")));
	TobiiNative::GetHeadPose = reinterpret_cast<TobiiNative::FGetHeadPose>(Export(TEXT("GetNewHeadPoses")));
	TobiiNative::IsInitialised = reinterpret_cast<TobiiNative::FBoolQuery>(Export(TEXT("IsInitialised")));
	TobiiNative::IsReady = reinterpret_cast<TobiiNative::FBoolQuery>(Export(TEXT("IsReady")));
	TobiiNative::IsConnected = reinterpret_cast<TobiiNative::FBoolQuery>(Export(TEXT("IsConnected")));
	TobiiNative::TimeSinceLastGaze = reinterpret_cast<TobiiNative::FFloatQuery>(Export(TEXT("TimeSinceLastGazePacket")));
	TobiiNative::TimeSinceLastHead = reinterpret_cast<TobiiNative::FFloatQuery>(Export(TEXT("TimeSinceLastHeadPacket")));
	TobiiNative::GetPresence = reinterpret_cast<TobiiNative::FGetPresence>(Export(TEXT("GetUserPresence")));

	if (!TobiiNative::Start || !TobiiNative::SetWindow || !TobiiNative::Stop || !TobiiNative::Subscribe ||
		!TobiiNative::Unsubscribe || !TobiiNative::Update || !TobiiNative::GetGaze || !TobiiNative::GetHeadPose ||
		!TobiiNative::IsInitialised || !TobiiNative::IsConnected || !TobiiNative::TimeSinceLastGaze || !TobiiNative::GetPresence)
	{
		ReportError(TEXT("Tobii Game Integration DLL is missing required exports."));
		UnloadIntegration();
		return false;
	}
	return true;
#else
	ReportError(TEXT("Tobii Eye Tracker 5 plugin supports Windows only."));
	return false;
#endif
}

void UTobiiEyeTrackerSubsystem::UnloadIntegration()
{
	if (DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
		DllHandle = nullptr;
	}
	TobiiNative::Reset();
}

bool UTobiiEyeTrackerSubsystem::StartTracking()
{
	if (bStarted)
	{
		return bAvailable;
	}
	if (bStartInProgress || bStartAttempted)
	{
		return false;
	}

	bStartAttempted = true;
	bStopRequested = false;
	if (!LoadIntegration())
	{
		return false;
	}

	bStartInProgress = true;
	UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: native Start(false) dispatched."));
	StartFuture = Async(EAsyncExecution::Thread, []()
	{
		return TobiiNative::Start(false);
	});
	return false;
}

void UTobiiEyeTrackerSubsystem::CompleteStartTracking()
{
	bStartInProgress = false;
	bStarted = StartFuture.Get();
	UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: native Start(false) completed with result=%s."), bStarted ? TEXT("true") : TEXT("false"));

	if (bStopRequested)
	{
		if (bStarted && TobiiNative::Stop)
		{
			TobiiNative::Stop();
		}
		bStarted = false;
		return;
	}

	bAvailable = bStarted && TobiiNative::IsInitialised();
	if (!bAvailable)
	{
		ReportError(TEXT("Tobii Game Integration started but did not initialize."));
		return;
	}

	TobiiNative::Subscribe(TobiiNative::ESubscription::StandardGaze);
	TobiiNative::Subscribe(TobiiNative::ESubscription::HeadTracking);
	TobiiNative::Subscribe(TobiiNative::ESubscription::UserPresence);
	BindGameWindow();
	UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: native streams subscribed."));
}

void UTobiiEyeTrackerSubsystem::StopTracking()
{
	bStopRequested = true;
	if (bStarted && TobiiNative::Unsubscribe && TobiiNative::Stop)
	{
		TobiiNative::Unsubscribe(TobiiNative::ESubscription::StandardGaze);
		TobiiNative::Unsubscribe(TobiiNative::ESubscription::HeadTracking);
		TobiiNative::Unsubscribe(TobiiNative::ESubscription::UserPresence);
		TobiiNative::Stop();
	}
	bStarted = false;
	bAvailable = false;
	bConnected = false;
	if (!bStartInProgress)
	{
		bStartAttempted = false;
	}
}

void UTobiiEyeTrackerSubsystem::BindGameWindow()
{
#if PLATFORM_WINDOWS
	HWND Window = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ActiveWindow.IsValid() && ActiveWindow->GetNativeWindow().IsValid())
		{
			Window = static_cast<HWND>(ActiveWindow->GetNativeWindow()->GetOSWindowHandle());
		}
	}
	if (!Window)
	{
		Window = GetActiveWindow();
	}
	if (!Window)
	{
		Window = GetForegroundWindow();
	}
	if (Window && TobiiNative::SetWindow && Window != LastBoundWindow)
	{
		TobiiNative::SetWindow(Window);
		LastBoundWindow = Window;
		UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: bound native integration to window 0x%p."), Window);
	}
#endif
}

void UTobiiEyeTrackerSubsystem::UpdateDevice(float DeltaTime)
{
	BindGameWindow();
	TobiiNative::Update();

	const bool bNewConnected = TobiiNative::IsConnected();
	if (bNewConnected != bConnected)
	{
		bConnected = bNewConnected;
		UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: connection changed: %s."), bConnected ? TEXT("connected") : TEXT("disconnected"));
		OnConnectionChanged.Broadcast(bConnected);
	}

	TobiiNative::FGazePoint* Points = nullptr;
	int32 PointCount = 0;
	TobiiNative::GetGaze(Points, PointCount, TobiiNative::EUnitType::Normalized);
	if (Points && PointCount > 0)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		int32 ValidCount = 0;
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			const FVector2D Point(Points[Index].X, Points[Index].Y);
			if (FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y))
			{
				Sum += FVector2D(FMath::Clamp(Point.X, 0.0, 1.0), FMath::Clamp(Point.Y, 0.0, 1.0));
				++ValidCount;
			}
		}
		if (ValidCount > 0)
		{
			LatestGaze.TimestampMicroseconds = Points[PointCount - 1].Timestamp;
			LatestGaze.RawDisplayPoint = Sum / ValidCount;
			const double Alpha = 1.0 - FMath::Exp(-18.0 * FMath::Max(DeltaTime, 0.0001f));
			LatestGaze.DisplayPoint = bHasSmoothedPoint
				? FMath::Lerp(LatestGaze.DisplayPoint, LatestGaze.RawDisplayPoint, Alpha)
				: LatestGaze.RawDisplayPoint;
			bHasSmoothedPoint = true;
			LatestGaze.bValid = true;
			GazeSampleCount += PointCount;
			if (!bLoggedFirstGaze)
			{
				bLoggedFirstGaze = true;
				UE_LOG(LogTemp, Log, TEXT("TobiiEyeTracker5: received first valid gaze sample at normalized point (%.3f, %.3f)."),
					LatestGaze.DisplayPoint.X, LatestGaze.DisplayPoint.Y);
			}
			OnGazeUpdated.Broadcast(LatestGaze);
		}
	}
	LastGazeAgeSeconds = TobiiNative::TimeSinceLastGaze();
	if (LastGazeAgeSeconds > 0.35f)
	{
		LatestGaze.bValid = false;
	}

	TobiiNative::FHeadPose* Poses = nullptr;
	int32 PoseCount = 0;
	TobiiNative::GetHeadPose(Poses, PoseCount);
	if (Poses && PoseCount > 0)
	{
		const TobiiNative::FHeadPose& Pose = Poses[PoseCount - 1];
		LatestHeadPose.TimestampMicroseconds = Pose.Timestamp;
		LatestHeadPose.Position = FVector(Pose.Position.X, Pose.Position.Y, Pose.Position.Z);
		LatestHeadPose.Rotation = FRotator(Pose.Rotation.Pitch, Pose.Rotation.Yaw, Pose.Rotation.Roll);
		LatestHeadPose.bValid = true;
	}

	const ETobiiUserPresence NewPresence = static_cast<ETobiiUserPresence>(FMath::Clamp(TobiiNative::GetPresence(), 0, 2));
	if (NewPresence != Presence)
	{
		Presence = NewPresence;
		OnPresenceChanged.Broadcast(Presence);
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastDiagnosticLogSeconds >= 5.0)
	{
		LastDiagnosticLogSeconds = Now;
		const float HeadAgeSeconds = TobiiNative::TimeSinceLastHead ? TobiiNative::TimeSinceLastHead() : -1.0f;
		UE_LOG(LogTemp, Log,
			TEXT("TobiiEyeTracker5: diag ready=%s connected=%s gazeCount=%d gazeAge=%.3f headValid=%s headAge=%.3f presence=%d."),
			TobiiNative::IsReady && TobiiNative::IsReady() ? TEXT("true") : TEXT("false"),
			bConnected ? TEXT("true") : TEXT("false"),
			GazeSampleCount,
			LastGazeAgeSeconds,
			LatestHeadPose.bValid ? TEXT("true") : TEXT("false"),
			HeadAgeSeconds,
			static_cast<int32>(Presence));
	}
}

void UTobiiEyeTrackerSubsystem::UpdateSimulation(float DeltaTime)
{
	const bool bNewConnected = true;
	if (bConnected != bNewConnected)
	{
		bConnected = bNewConnected;
		bAvailable = true;
		OnConnectionChanged.Broadcast(true);
	}
	const double Time = FPlatformTime::Seconds();
	LatestGaze.TimestampMicroseconds = static_cast<int64>(Time * 1000000.0);
	LatestGaze.RawDisplayPoint = FVector2D(0.5 + FMath::Sin(Time * 0.55) * 0.42, 0.55 + FMath::Sin(Time * 0.9) * 0.12);
	LatestGaze.DisplayPoint = FMath::Lerp(LatestGaze.DisplayPoint, LatestGaze.RawDisplayPoint, 1.0 - FMath::Exp(-12.0 * DeltaTime));
	LatestGaze.bValid = true;
	LatestHeadPose.TimestampMicroseconds = LatestGaze.TimestampMicroseconds;
	LatestHeadPose.Position = FVector(FMath::Sin(Time) * 20.0, 0.0, 650.0);
	LatestHeadPose.Rotation = FRotator(FMath::Sin(Time * 0.7) * 5.0, FMath::Sin(Time * 0.5) * 12.0, 0.0);
	LatestHeadPose.bValid = true;
	Presence = ETobiiUserPresence::Present;
	++GazeSampleCount;
	LastGazeAgeSeconds = 0.0f;
	OnGazeUpdated.Broadcast(LatestGaze);
}

bool UTobiiEyeTrackerSubsystem::LaunchSystemCalibration()
{
#if PLATFORM_WINDOWS
	const FString ConfigurationPath = TEXT("C:/Program Files/Tobii/Tobii EyeX/Tobii.Configuration.exe");
	if (!FPaths::FileExists(ConfigurationPath))
	{
		ReportError(FString::Printf(TEXT("Tobii system calibration tool not found: %s"), *ConfigurationPath));
		return false;
	}
	FPlatformProcess::CreateProc(*ConfigurationPath, TEXT(""), true, false, false, nullptr, 0, nullptr, nullptr);
	return true;
#else
	return false;
#endif
}

void UTobiiEyeTrackerSubsystem::ReportError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("TobiiEyeTracker5: %s"), *Message);
	OnError.Broadcast(Message);
}

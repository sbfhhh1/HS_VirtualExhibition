#include "BlenderGNRuntimePanelWidget.h"

#include "BlenderGNCageCornerMarkersComponent.h"
#include "BlenderGNGradientBoxMotionComponent.h"
#include "BlenderGNIslandBreathMotionComponent.h"
#include "BlenderGNParasiteMotionComponent.h"
#include "BlenderGNWallNoiseMotionComponent.h"
#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Actor.h"
#include "Widgets/SWidget.h"

TSharedRef<SWidget> UBlenderGNRuntimePanelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultGradientBoxPanel();
	}

	return Super::RebuildWidget();
}

void UBlenderGNRuntimePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!Target && GetOwningPlayerPawn())
	{
		ResolveReferences(GetOwningPlayerPawn());
	}
	ApplyRuntimeSettings();
	SynchronizeGradientBoxControls();
	UpdateStatusText();
}

void UBlenderGNRuntimePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PumpQueuedRebuild();
	UpdateStatusText();
}

void UBlenderGNRuntimePanelWidget::ResolveReferences(AActor* SearchActor)
{
	if (!SearchActor)
	{
		return;
	}

	if (!Target)
	{
		Target = SearchActor->FindComponentByClass<UBlenderGeometryNodesComponent>();
	}
	if (!IslandMotion)
	{
		IslandMotion = SearchActor->FindComponentByClass<UBlenderGNIslandBreathMotionComponent>();
	}
	if (!ParasiteMotion)
	{
		ParasiteMotion = SearchActor->FindComponentByClass<UBlenderGNParasiteMotionComponent>();
	}
	if (!WallNoiseMotion)
	{
		WallNoiseMotion = SearchActor->FindComponentByClass<UBlenderGNWallNoiseMotionComponent>();
	}
	if (!GradientBoxMotion)
	{
		GradientBoxMotion = SearchActor->FindComponentByClass<UBlenderGNGradientBoxMotionComponent>();
	}
	if (!CageCornerMarkers)
	{
		CageCornerMarkers = SearchActor->FindComponentByClass<UBlenderGNCageCornerMarkersComponent>();
	}
}

void UBlenderGNRuntimePanelWidget::ApplyRuntimeSettings()
{
	if (Target)
	{
		Target->RefreshMode = (bDriveBlenderAnimation && !GradientBoxMotion) ? EBlenderGNRefreshMode::FixedInterval : EBlenderGNRefreshMode::Manual;
		Target->FixedRefreshInterval = 0.2f;
		Target->BlenderFrameRate = BlenderFrameRate;
	}
	SetLiveMotion(bLiveMotion);
}

void UBlenderGNRuntimePanelWidget::SetLiveMotion(bool bEnabled)
{
	bLiveMotion = bEnabled;
	if (IslandMotion)
	{
		IslandMotion->SetLiveMotion(bEnabled);
	}
	if (ParasiteMotion)
	{
		ParasiteMotion->SetLiveMotion(bEnabled);
	}
	if (WallNoiseMotion)
	{
		WallNoiseMotion->SetLiveMotion(bEnabled);
	}
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetLiveMotion(bEnabled);
	}
}

void UBlenderGNRuntimePanelWidget::SetLiveBlenderRebuild(bool bEnabled)
{
	bLiveBlenderRebuild = bEnabled;
	if (!bLiveBlenderRebuild)
	{
		bRebuildQueued = false;
		bForceQueuedRebuild = false;
	}
}

void UBlenderGNRuntimePanelWidget::QueueBlenderRebuild(bool bForce, float DelayOverride)
{
	if (!Target || (!bForce && !bLiveBlenderRebuild))
	{
		return;
	}

	bRebuildQueued = true;
	bForceQueuedRebuild |= bForce;
	const float Delay = DelayOverride >= 0.0f ? DelayOverride : RebuildDebounceSeconds;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RebuildAtSeconds = Target->bIsRunning ? Now : Now + Delay;
}

void UBlenderGNRuntimePanelWidget::RefreshNow()
{
	bRebuildQueued = false;
	bForceQueuedRebuild = false;
	if (Target)
	{
		Target->RefreshNow();
	}
}

void UBlenderGNRuntimePanelWidget::SetFloatInput(const FString& SocketName, float Value, bool bPreviewParasite)
{
	if (!Target)
	{
		return;
	}

	Target->SetFloatInput(SocketName, Value, false);
	if (bPreviewParasite && ParasiteMotion)
	{
		ParasiteMotion->SetGeometryPreview(SocketName, Value);
	}
	if (CageCornerMarkers && SocketName.Equals(TEXT("Corner Sphere Radius"), ESearchCase::IgnoreCase))
	{
		CageCornerMarkers->SetMarkerRadius(Value * Target->BlenderToUnrealScale);
	}
	QueueBlenderRebuild(true);
}

void UBlenderGNRuntimePanelWidget::SetIntInput(const FString& SocketName, int32 Value)
{
	if (Target)
	{
		Target->SetIntInput(SocketName, Value, false);
		QueueBlenderRebuild(true);
	}
}

void UBlenderGNRuntimePanelWidget::SetBoolInput(const FString& SocketName, bool bValue)
{
	if (Target)
	{
		Target->SetBoolInput(SocketName, bValue, false);
		QueueBlenderRebuild(true);
	}
}

void UBlenderGNRuntimePanelWidget::SetVectorInput(const FString& SocketName, FVector Value)
{
	if (Target)
	{
		Target->SetVectorInput(SocketName, Value, false);
		QueueBlenderRebuild(true);
	}
}

void UBlenderGNRuntimePanelWidget::SetWallNoiseStrength(float Value)
{
	if (WallNoiseMotion)
	{
		WallNoiseMotion->Strength = Value;
	}
	SetFloatInput(TEXT("Noise Strength"), Value, false);
}

void UBlenderGNRuntimePanelWidget::SetWallNoiseSpeed(float Value)
{
	if (WallNoiseMotion)
	{
		WallNoiseMotion->Speed = Value;
	}
	SetFloatInput(TEXT("Wave Speed"), Value, false);
}

void UBlenderGNRuntimePanelWidget::SetWallNoiseScale(float Value)
{
	if (WallNoiseMotion)
	{
		WallNoiseMotion->Scale = Value;
	}
	SetFloatInput(TEXT("Noise Scale"), Value, false);
}

void UBlenderGNRuntimePanelWidget::UpdateStatusText()
{
	RebuildStatusText = FString::Printf(TEXT("Blender Rebuild: %s"), bRebuildQueued ? TEXT("Queued") : (bLiveBlenderRebuild ? TEXT("Live") : TEXT("Manual")));
	if (!Target)
	{
		RuntimeStatusText = TEXT("No Geometry Nodes target");
		return;
	}

	RuntimeStatusText = GradientBoxMotion
		? FString::Printf(
			TEXT("%s\n%.2fs | %d verts | %d tris | %d islands"),
			*Target->LastStatus,
			Target->LastEvaluationSeconds,
			Target->LastVertexCount,
			Target->LastTriangleCount,
			GradientBoxMotion->GetCachedIslandCount())
		: FString::Printf(
			TEXT("%s\n%.2fs | %d verts | %d tris"),
			*Target->LastStatus,
			Target->LastEvaluationSeconds,
			Target->LastVertexCount,
			Target->LastTriangleCount);
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(RuntimeStatusText));
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxAmplitude(float Value)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetAmplitude(Value);
		UpdateGradientBoxValueText();
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxSpeed(float Value)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetSpeed(Value);
		UpdateGradientBoxValueText();
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxSmoothness(float Value)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetSmoothness(Value);
		UpdateGradientBoxValueText();
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxMaxFPS(float Value)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->UpdateTickRate(Value);
		UpdateGradientBoxValueText();
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxReversePink(bool bReverse)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetReversePinkAnimation(bReverse);
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxUseFastMesh(bool bEnabled)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetUseFastDynamicMesh(bEnabled);
	}
}

void UBlenderGNRuntimePanelWidget::SetGradientBoxRecalcNormals(bool bEnabled)
{
	if (GradientBoxMotion)
	{
		GradientBoxMotion->SetRecalculateAnimatedNormals(bEnabled);
	}
}

void UBlenderGNRuntimePanelWidget::PumpQueuedRebuild()
{
	if (!bRebuildQueued || !Target)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now < RebuildAtSeconds || Target->bIsRunning)
	{
		return;
	}

	bRebuildQueued = false;
	const bool bForce = bForceQueuedRebuild;
	bForceQueuedRebuild = false;
	Target->RequestRefresh(bForce);
}

void UBlenderGNRuntimePanelWidget::BuildDefaultGradientBoxPanel()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GradientBoxControlRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GradientBoxControlPanel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.88f));
	Panel->SetPadding(FMargin(12.0f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		PanelSlot->SetPosition(FVector2D(22.0f, 22.0f));
		PanelSlot->SetSize(FVector2D(390.0f, 700.0f));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GradientBoxControlContent"));
	Panel->SetContent(Content);

	auto MakeText = [this](const TCHAR* Name, const FString& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		return TextBlock;
	};

	UTextBlock* Title = MakeText(TEXT("GradientBoxControlTitle"), TEXT("GradientBox Runtime Control"), 18, FLinearColor(0.9f, 0.92f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	auto AddSliderRow = [&](const TCHAR* RowName, const FString& Label, float MinValue, float MaxValue, float InitialValue, TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutValueText)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		if (UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
		}

		UTextBlock* LabelText = MakeText(*FString::Printf(TEXT("%sLabel"), RowName), Label, 13, FLinearColor(0.72f, 0.76f, 0.84f, 1.0f));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *FString::Printf(TEXT("%sSlider"), RowName));
		OutSlider->SetMinValue(MinValue);
		OutSlider->SetMaxValue(MaxValue);
		OutSlider->SetValue(InitialValue);
		if (UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(OutSlider))
		{
			SliderSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		}

		OutValueText = MakeText(*FString::Printf(TEXT("%sValue"), RowName), TEXT("0.00"), 12, FLinearColor(0.92f, 0.94f, 1.0f, 1.0f));
		OutValueText->SetMinDesiredWidth(44.0f);
		if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(OutValueText))
		{
			ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	UTextBlock* MotionHeader = MakeText(TEXT("MotionHeader"), TEXT("Motion"), 14, FLinearColor(0.96f, 0.82f, 0.92f, 1.0f));
	if (UVerticalBoxSlot* MotionHeaderSlot = Content->AddChildToVerticalBox(MotionHeader))
	{
		MotionHeaderSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 2.0f));
	}

	AddSliderRow(TEXT("Amplitude"), TEXT("Amplitude"), 0.0f, 1.0f, 1.0f, AmplitudeSlider, AmplitudeValueText);
	AddSliderRow(TEXT("Speed"), TEXT("Motion Speed"), 0.0f, 4.0f, 1.0f, SpeedSlider, SpeedValueText);
	AddSliderRow(TEXT("Smoothness"), TEXT("Smoothness"), 0.0f, 1.0f, 0.55f, SmoothnessSlider, SmoothnessValueText);
	AddSliderRow(TEXT("MaxFPS"), TEXT("Max FPS"), 1.0f, 120.0f, 60.0f, MaxFPSSlider, MaxFPSValueText);

	UTextBlock* GNHeader = MakeText(TEXT("GNHeader"), TEXT("Geometry Nodes"), 14, FLinearColor(0.78f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* GNHeaderSlot = Content->AddChildToVerticalBox(GNHeader))
	{
		GNHeaderSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	}

	AddSliderRow(TEXT("GNNum"), TEXT("num"), 1.0f, 20.0f, 10.0f, NumSlider, NumValueText);
	AddSliderRow(TEXT("GNSize"), TEXT("size"), 0.1f, 3.0f, 1.1236745f, SizeSlider, SizeValueText);
	AddSliderRow(TEXT("GNGap"), TEXT("gap"), 0.0f, 0.6f, 0.15f, GapSlider, GapValueText);
	AddSliderRow(TEXT("GNFrame"), TEXT("frame"), 0.5f, 5.0f, 1.0f, FrameSlider, FrameValueText);
	AddSliderRow(TEXT("GNSpeed"), TEXT("GN speed"), 1.0f, 10.0f, 5.0f, GNSpeedSlider, GNSpeedValueText);
	AddSliderRow(TEXT("GNPower"), TEXT("power"), 1.0f, 5.0f, 2.0f, PowerSlider, PowerValueText);
	AddSliderRow(TEXT("GNShiftAngle"), TEXT("shift ang"), 0.0f, 360.0f, 180.0f, ShiftAngleSlider, ShiftAngleValueText);

	auto AddCheckBoxRow = [&](const TCHAR* RowName, const FString& Label, TObjectPtr<UCheckBox>& OutCheckBox)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		if (UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 3.0f));
		}

		OutCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *FString::Printf(TEXT("%sCheckBox"), RowName));
		if (UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(OutCheckBox))
		{
			CheckSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CheckSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* LabelText = MakeText(*FString::Printf(TEXT("%sLabel"), RowName), Label, 13, FLinearColor(0.82f, 0.86f, 0.94f, 1.0f));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	UTextBlock* RuntimeHeader = MakeText(TEXT("RuntimeHeader"), TEXT("Runtime"), 14, FLinearColor(0.82f, 1.0f, 0.86f, 1.0f));
	if (UVerticalBoxSlot* RuntimeHeaderSlot = Content->AddChildToVerticalBox(RuntimeHeader))
	{
		RuntimeHeaderSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	}

	AddCheckBoxRow(TEXT("LiveMotion"), TEXT("Live motion"), LiveMotionCheckBox);
	AddCheckBoxRow(TEXT("LiveBlenderRebuild"), TEXT("Live Blender rebuild"), LiveBlenderRebuildCheckBox);
	AddCheckBoxRow(TEXT("DriveBlenderAnimation"), TEXT("Drive Blender animation"), DriveBlenderAnimationCheckBox);
	AddCheckBoxRow(TEXT("ReversePink"), TEXT("Reverse pink phase"), ReversePinkCheckBox);
	AddCheckBoxRow(TEXT("FastMesh"), TEXT("Use fast dynamic mesh"), FastMeshCheckBox);
	AddCheckBoxRow(TEXT("RecalculateNormals"), TEXT("Recalculate animated normals"), RecalculateNormalsCheckBox);

	StatusTextBlock = MakeText(TEXT("GradientBoxStatus"), TEXT("Idle"), 11, FLinearColor(0.58f, 0.64f, 0.72f, 1.0f));
	if (UVerticalBoxSlot* StatusSlot = Content->AddChildToVerticalBox(StatusTextBlock))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	}

	if (AmplitudeSlider)
	{
		AmplitudeSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleAmplitudeChanged);
	}
	if (SpeedSlider)
	{
		SpeedSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleSpeedChanged);
	}
	if (SmoothnessSlider)
	{
		SmoothnessSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleSmoothnessChanged);
	}
	if (MaxFPSSlider)
	{
		MaxFPSSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleMaxFPSChanged);
	}
	if (NumSlider)
	{
		NumSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleNumChanged);
	}
	if (SizeSlider)
	{
		SizeSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleSizeChanged);
	}
	if (GapSlider)
	{
		GapSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleGapChanged);
	}
	if (FrameSlider)
	{
		FrameSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleFrameChanged);
	}
	if (GNSpeedSlider)
	{
		GNSpeedSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleGNSpeedChanged);
	}
	if (PowerSlider)
	{
		PowerSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandlePowerChanged);
	}
	if (ShiftAngleSlider)
	{
		ShiftAngleSlider->OnValueChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleShiftAngleChanged);
	}
	if (LiveMotionCheckBox)
	{
		LiveMotionCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleLiveMotionChanged);
	}
	if (LiveBlenderRebuildCheckBox)
	{
		LiveBlenderRebuildCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleLiveBlenderRebuildChanged);
	}
	if (DriveBlenderAnimationCheckBox)
	{
		DriveBlenderAnimationCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleDriveBlenderAnimationChanged);
	}
	if (ReversePinkCheckBox)
	{
		ReversePinkCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleReversePinkChanged);
	}
	if (FastMeshCheckBox)
	{
		FastMeshCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleUseFastMeshChanged);
	}
	if (RecalculateNormalsCheckBox)
	{
		RecalculateNormalsCheckBox->OnCheckStateChanged.AddDynamic(this, &UBlenderGNRuntimePanelWidget::HandleRecalculateNormalsChanged);
	}
}

void UBlenderGNRuntimePanelWidget::SynchronizeGradientBoxControls()
{
	if (!GradientBoxMotion)
	{
		return;
	}

	bSynchronizingControls = true;
	if (AmplitudeSlider)
	{
		AmplitudeSlider->SetValue(GradientBoxMotion->Amplitude);
	}
	if (SpeedSlider)
	{
		SpeedSlider->SetValue(GradientBoxMotion->Speed);
	}
	if (SmoothnessSlider)
	{
		SmoothnessSlider->SetValue(GradientBoxMotion->Smoothness);
	}
	if (MaxFPSSlider)
	{
		MaxFPSSlider->SetValue(GradientBoxMotion->MaxAnimationFrameRate);
	}
	if (LiveMotionCheckBox)
	{
		LiveMotionCheckBox->SetIsChecked(bLiveMotion);
	}
	if (LiveBlenderRebuildCheckBox)
	{
		LiveBlenderRebuildCheckBox->SetIsChecked(bLiveBlenderRebuild);
	}
	if (DriveBlenderAnimationCheckBox)
	{
		DriveBlenderAnimationCheckBox->SetIsChecked(bDriveBlenderAnimation);
	}
	if (ReversePinkCheckBox)
	{
		ReversePinkCheckBox->SetIsChecked(GradientBoxMotion->bReversePinkAnimation);
	}
	if (FastMeshCheckBox)
	{
		FastMeshCheckBox->SetIsChecked(GradientBoxMotion->bUseFastDynamicMesh);
	}
	if (RecalculateNormalsCheckBox)
	{
		RecalculateNormalsCheckBox->SetIsChecked(GradientBoxMotion->bRecalculateAnimatedNormals);
	}
	if (Target)
	{
		if (NumSlider)
		{
			NumSlider->SetValue(static_cast<float>(Target->GetIntInput(TEXT("Socket_2"), Target->GetIntInput(TEXT("num"), 10))));
		}
		if (SizeSlider)
		{
			SizeSlider->SetValue(Target->GetFloatInput(TEXT("Socket_3"), Target->GetFloatInput(TEXT("size"), 1.1236745f)));
		}
		if (GapSlider)
		{
			GapSlider->SetValue(Target->GetFloatInput(TEXT("Socket_8"), Target->GetFloatInput(TEXT("gap"), 0.15f)));
		}
		if (FrameSlider)
		{
			FrameSlider->SetValue(Target->GetFloatInput(TEXT("Socket_4"), Target->GetFloatInput(TEXT("frame"), 1.0f)));
		}
		if (GNSpeedSlider)
		{
			GNSpeedSlider->SetValue(Target->GetFloatInput(TEXT("Socket_5"), Target->GetFloatInput(TEXT("speed"), 5.0f)));
		}
		if (PowerSlider)
		{
			PowerSlider->SetValue(Target->GetFloatInput(TEXT("Socket_6"), Target->GetFloatInput(TEXT("power"), 2.0f)));
		}
		if (ShiftAngleSlider)
		{
			ShiftAngleSlider->SetValue(Target->GetFloatInput(TEXT("Socket_7"), Target->GetFloatInput(TEXT("shift ang"), 180.0f)));
		}
	}
	bSynchronizingControls = false;
	UpdateGradientBoxValueText();
	UpdateGeometryNodeValueText();
}

void UBlenderGNRuntimePanelWidget::UpdateGradientBoxValueText()
{
	if (!GradientBoxMotion)
	{
		return;
	}

	if (AmplitudeValueText)
	{
		AmplitudeValueText->SetText(FText::AsNumber(GradientBoxMotion->Amplitude));
	}
	if (SpeedValueText)
	{
		SpeedValueText->SetText(FText::AsNumber(GradientBoxMotion->Speed));
	}
	if (SmoothnessValueText)
	{
		SmoothnessValueText->SetText(FText::AsNumber(GradientBoxMotion->Smoothness));
	}
	if (MaxFPSValueText)
	{
		MaxFPSValueText->SetText(FText::AsNumber(FMath::RoundToInt(GradientBoxMotion->MaxAnimationFrameRate)));
	}
}

void UBlenderGNRuntimePanelWidget::UpdateGeometryNodeValueText()
{
	if (!Target)
	{
		return;
	}

	if (NumValueText)
	{
		NumValueText->SetText(FText::AsNumber(Target->GetIntInput(TEXT("Socket_2"), Target->GetIntInput(TEXT("num"), 10))));
	}
	if (SizeValueText)
	{
		SizeValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_3"), Target->GetFloatInput(TEXT("size"), 1.1236745f))));
	}
	if (GapValueText)
	{
		GapValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_8"), Target->GetFloatInput(TEXT("gap"), 0.15f))));
	}
	if (FrameValueText)
	{
		FrameValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_4"), Target->GetFloatInput(TEXT("frame"), 1.0f))));
	}
	if (GNSpeedValueText)
	{
		GNSpeedValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_5"), Target->GetFloatInput(TEXT("speed"), 5.0f))));
	}
	if (PowerValueText)
	{
		PowerValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_6"), Target->GetFloatInput(TEXT("power"), 2.0f))));
	}
	if (ShiftAngleValueText)
	{
		ShiftAngleValueText->SetText(FText::AsNumber(Target->GetFloatInput(TEXT("Socket_7"), Target->GetFloatInput(TEXT("shift ang"), 180.0f))));
	}
}

void UBlenderGNRuntimePanelWidget::HandleAmplitudeChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxAmplitude(Value);
	}
}

void UBlenderGNRuntimePanelWidget::HandleSpeedChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxSpeed(Value);
	}
}

void UBlenderGNRuntimePanelWidget::HandleSmoothnessChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxSmoothness(Value);
	}
}

void UBlenderGNRuntimePanelWidget::HandleMaxFPSChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxMaxFPS(Value);
	}
}

void UBlenderGNRuntimePanelWidget::HandleNumChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetIntInput(TEXT("Socket_2"), FMath::RoundToInt(Value));
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleSizeChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_3"), Value, false);
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleGapChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_8"), Value, false);
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleFrameChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_4"), Value, false);
		if (GradientBoxMotion)
		{
			GradientBoxMotion->SetAmplitude(GradientBoxMotion->Amplitude);
		}
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleGNSpeedChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_5"), Value, false);
		if (GradientBoxMotion)
		{
			GradientBoxMotion->SetSpeed(GradientBoxMotion->Speed);
		}
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandlePowerChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_6"), Value, false);
		if (GradientBoxMotion)
		{
			GradientBoxMotion->SetSmoothness(GradientBoxMotion->Smoothness);
		}
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleShiftAngleChanged(float Value)
{
	if (!bSynchronizingControls)
	{
		SetFloatInput(TEXT("Socket_7"), Value, false);
		if (GradientBoxMotion)
		{
			GradientBoxMotion->SetReversePinkAnimation(GradientBoxMotion->bReversePinkAnimation);
		}
		UpdateGeometryNodeValueText();
	}
}

void UBlenderGNRuntimePanelWidget::HandleLiveMotionChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		SetLiveMotion(bIsChecked);
	}
}

void UBlenderGNRuntimePanelWidget::HandleLiveBlenderRebuildChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		SetLiveBlenderRebuild(bIsChecked);
	}
}

void UBlenderGNRuntimePanelWidget::HandleDriveBlenderAnimationChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		bDriveBlenderAnimation = bIsChecked;
		ApplyRuntimeSettings();
	}
}

void UBlenderGNRuntimePanelWidget::HandleReversePinkChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxReversePink(bIsChecked);
	}
}

void UBlenderGNRuntimePanelWidget::HandleUseFastMeshChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxUseFastMesh(bIsChecked);
	}
}

void UBlenderGNRuntimePanelWidget::HandleRecalculateNormalsChanged(bool bIsChecked)
{
	if (!bSynchronizingControls)
	{
		SetGradientBoxRecalcNormals(bIsChecked);
	}
}

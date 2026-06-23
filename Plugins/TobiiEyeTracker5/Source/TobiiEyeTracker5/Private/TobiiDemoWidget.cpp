#include "TobiiDemoWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

TSharedRef<SWidget> UTobiiDemoWidget::RebuildWidget()
{
	Root = NewObject<UCanvasPanel>(this);
	WidgetTree->RootWidget = Root;

	auto AddText = [this](const TCHAR* Name, const FVector2D& Position, const FVector2D& Size, int32 FontSize)
	{
		UTextBlock* Text = NewObject<UTextBlock>(this, Name);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.0f)));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Text);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Text;
	};

	StatusText = AddText(TEXT("StatusText"), FVector2D(24, 24), FVector2D(660, 390), 18);
	EventText = AddText(TEXT("EventText"), FVector2D(24, 430), FVector2D(720, 300), 16);
	GuideText = AddText(TEXT("GuideText"), FVector2D(760, 24), FVector2D(500, 250), 18);
	CursorDot = AddText(TEXT("CursorDot"), FVector2D(0, 0), FVector2D(36, 36), 32);
	CursorDot->SetText(FText::FromString(TEXT("+")));
	CursorDot->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	DwellBar = NewObject<UProgressBar>(this, TEXT("DwellBar"));
	DwellBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f));
	UCanvasPanelSlot* DwellSlot = Root->AddChildToCanvas(DwellBar);
	DwellSlot->SetPosition(FVector2D(24, 390));
	DwellSlot->SetSize(FVector2D(420, 12));
	return Super::RebuildWidget();
}

void UTobiiDemoWidget::SetStatus(const FString& Status)
{
	if (StatusText) StatusText->SetText(FText::FromString(Status));
}

void UTobiiDemoWidget::SetEventLog(const FString& Events)
{
	if (EventText) EventText->SetText(FText::FromString(Events));
}

void UTobiiDemoWidget::SetGuide(const FString& Guide, bool bVisible)
{
	if (GuideText)
	{
		GuideText->SetText(FText::FromString(Guide));
		GuideText->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTobiiDemoWidget::SetGazeCursor(const FVector2D& NormalizedPoint, float DwellProgress, bool bVisible)
{
	if (DwellBar) DwellBar->SetPercent(DwellProgress);
	if (!CursorDot) return;
	CursorDot->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		if (UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(CursorDot->Slot))
		{
			FVector2D ViewportSize(1920, 1080);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ViewportSize);
			// Tobii normalized Y is bottom-to-top (0=bottom, 1=top), UE UI Y is top-to-bottom.
			CursorSlot->SetPosition(FVector2D(NormalizedPoint.X * ViewportSize.X - 18.0, (1.0 - NormalizedPoint.Y) * ViewportSize.Y - 18.0));
		}
	}
}

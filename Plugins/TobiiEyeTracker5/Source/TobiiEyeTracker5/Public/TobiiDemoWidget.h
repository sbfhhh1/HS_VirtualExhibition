#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TobiiDemoWidget.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;

UCLASS()
class TOBIIEYETRACKER5_API UTobiiDemoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatus(const FString& Status);
	void SetEventLog(const FString& Events);
	void SetGuide(const FString& Guide, bool bVisible);
	void SetGazeCursor(const FVector2D& NormalizedPoint, float DwellProgress, bool bVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EventText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GuideText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> DwellBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CursorDot;
};

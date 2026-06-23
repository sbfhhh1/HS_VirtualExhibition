#pragma once

#include "IDetailCustomization.h"

class UBlenderGeometryNodesComponent;

class FOpenClawBlenderGeometryNodesComponentDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TArray<TWeakObjectPtr<UBlenderGeometryNodesComponent>> CustomizedComponents;

	FReply ScanBlend();
	FReply CleanLabels();
	FReply RefreshPreview();
	FReply BakeStaticMesh();

	bool HasSingleComponent() const;
	UBlenderGeometryNodesComponent* GetSingleComponent() const;
};

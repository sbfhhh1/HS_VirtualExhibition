#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "OpenClawCreateGradientBoxSceneCommandlet.generated.h"

UCLASS()
class UOpenClawCreateGradientBoxSceneCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UOpenClawCreateGradientBoxSceneCommandlet();

	virtual int32 Main(const FString& Params) override;
};

UCLASS()
class UOpenClawVerifyGradientBoxSceneCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UOpenClawVerifyGradientBoxSceneCommandlet();

	virtual int32 Main(const FString& Params) override;
};

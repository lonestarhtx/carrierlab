#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabStage1Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabStage1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabStage1Commandlet();

	virtual int32 Main(const FString& Params) override;
};

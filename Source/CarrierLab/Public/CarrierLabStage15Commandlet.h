#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabStage15Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabStage15Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabStage15Commandlet();

	virtual int32 Main(const FString& Params) override;
};

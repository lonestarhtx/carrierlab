#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIISlice1Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIISlice1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice1Commandlet();

	virtual int32 Main(const FString& Params) override;
};

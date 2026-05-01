#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIISlice0Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIISlice0Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice0Commandlet();

	virtual int32 Main(const FString& Params) override;
};

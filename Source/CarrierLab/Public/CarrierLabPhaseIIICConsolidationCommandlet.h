// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIICConsolidationCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIICConsolidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIICConsolidationCommandlet();
	virtual int32 Main(const FString& Params) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIBConsolidationCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIBConsolidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIBConsolidationCommandlet();

	virtual int32 Main(const FString& Params) override;
};

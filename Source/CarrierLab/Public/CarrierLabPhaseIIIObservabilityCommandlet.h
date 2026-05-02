// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIObservabilityCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIObservabilityCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIObservabilityCommandlet();

	virtual int32 Main(const FString& Params) override;
};

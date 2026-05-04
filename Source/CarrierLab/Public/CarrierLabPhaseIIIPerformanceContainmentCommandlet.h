// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIPerformanceContainmentCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIPerformanceContainmentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIPerformanceContainmentCommandlet();

	virtual int32 Main(const FString& Params) override;
};

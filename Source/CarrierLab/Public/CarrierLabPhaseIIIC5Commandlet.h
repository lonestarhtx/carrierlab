// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIC5Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIC5Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIC5Commandlet();
	virtual int32 Main(const FString& Params) override;
};

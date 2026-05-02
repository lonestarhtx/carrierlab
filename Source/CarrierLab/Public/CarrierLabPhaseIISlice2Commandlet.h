// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIISlice2Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIISlice2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice2Commandlet();

	virtual int32 Main(const FString& Params) override;
};

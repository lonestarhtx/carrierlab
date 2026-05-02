// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIISlice4Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIISlice4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice4Commandlet();
	virtual int32 Main(const FString& Params) override;
};

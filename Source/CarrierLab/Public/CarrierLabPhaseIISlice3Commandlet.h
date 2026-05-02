// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIISlice3Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIISlice3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice3Commandlet();
	virtual int32 Main(const FString& Params) override;
};

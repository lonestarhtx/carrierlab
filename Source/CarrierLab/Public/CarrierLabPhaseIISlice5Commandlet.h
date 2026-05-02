// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIISlice5Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIISlice5Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIISlice5Commandlet();

	virtual int32 Main(const FString& Params) override;
};

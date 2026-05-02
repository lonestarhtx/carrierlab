// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIA2Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIA2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIA2Commandlet();

	virtual int32 Main(const FString& Params) override;
};

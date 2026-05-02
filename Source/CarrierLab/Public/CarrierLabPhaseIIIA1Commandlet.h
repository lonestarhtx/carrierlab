// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIA1Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIA1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIA1Commandlet();

	virtual int32 Main(const FString& Params) override;
};

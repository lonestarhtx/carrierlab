// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIA3Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIA3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIA3Commandlet();

	virtual int32 Main(const FString& Params) override;
};

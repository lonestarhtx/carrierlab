// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB1Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIB1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB1Commandlet();

	virtual int32 Main(const FString& Params) override;
};

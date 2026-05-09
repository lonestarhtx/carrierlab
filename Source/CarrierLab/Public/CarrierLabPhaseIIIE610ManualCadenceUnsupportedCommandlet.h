// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet();
	virtual int32 Main(const FString& Params) override;
};

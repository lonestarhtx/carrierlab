// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIICostDriverIdentificationCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIICostDriverIdentificationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIICostDriverIdentificationCommandlet();

	virtual int32 Main(const FString& Params) override;
};

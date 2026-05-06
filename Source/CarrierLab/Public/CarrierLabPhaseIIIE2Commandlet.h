// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE2Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE2Commandlet();
	virtual int32 Main(const FString& Params) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID6Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID6Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID6Commandlet();

	virtual int32 Main(const FString& Params) override;
};

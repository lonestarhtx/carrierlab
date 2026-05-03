// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID7Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID7Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID7Commandlet();
	virtual int32 Main(const FString& Params) override;
};

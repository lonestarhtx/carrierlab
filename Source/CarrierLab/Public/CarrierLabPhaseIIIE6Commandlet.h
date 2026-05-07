// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE6Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE6Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE6Commandlet();
	virtual int32 Main(const FString& Params) override;
};

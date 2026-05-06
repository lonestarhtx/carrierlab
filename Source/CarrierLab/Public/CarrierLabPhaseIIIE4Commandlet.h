// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE4Commandlet();
	virtual int32 Main(const FString& Params) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE5Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE5Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE5Commandlet();
	virtual int32 Main(const FString& Params) override;
};

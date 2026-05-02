// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIC4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIC4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIC4Commandlet();
	virtual int32 Main(const FString& Params) override;
};

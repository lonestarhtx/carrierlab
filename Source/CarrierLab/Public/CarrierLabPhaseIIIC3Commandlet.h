// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIC3Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIC3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIC3Commandlet();
	virtual int32 Main(const FString& Params) override;
};

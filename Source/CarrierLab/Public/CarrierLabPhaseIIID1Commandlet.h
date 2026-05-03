// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID1Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID1Commandlet();
	virtual int32 Main(const FString& Params) override;
};

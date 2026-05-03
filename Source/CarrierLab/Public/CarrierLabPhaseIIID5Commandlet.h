// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID5Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID5Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID5Commandlet();

	virtual int32 Main(const FString& Params) override;
};

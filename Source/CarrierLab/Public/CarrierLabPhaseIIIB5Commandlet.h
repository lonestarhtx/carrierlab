// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB5Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIB5Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB5Commandlet();
	virtual int32 Main(const FString& Params) override;
};

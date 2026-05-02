// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB6Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIB6Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB6Commandlet();
	virtual int32 Main(const FString& Params) override;
};

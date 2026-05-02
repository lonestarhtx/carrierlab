// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB7Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIB7Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB7Commandlet();
	virtual int32 Main(const FString& Params) override;
};

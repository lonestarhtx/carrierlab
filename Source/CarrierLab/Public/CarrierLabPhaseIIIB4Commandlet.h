// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIB4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB4Commandlet();
	virtual int32 Main(const FString& Params) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID8Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID8Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID8Commandlet();
	virtual int32 Main(const FString& Params) override;
};

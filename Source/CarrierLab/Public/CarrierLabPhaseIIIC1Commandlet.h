// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIC1Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIC1Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIC1Commandlet();
	virtual int32 Main(const FString& Params) override;
};

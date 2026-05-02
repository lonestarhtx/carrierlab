// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIC2Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIC2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIC2Commandlet();
	virtual int32 Main(const FString& Params) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet();
	virtual int32 Main(const FString& Params) override;
};

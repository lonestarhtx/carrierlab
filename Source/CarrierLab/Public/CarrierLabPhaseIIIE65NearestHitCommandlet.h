// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE65NearestHitCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE65NearestHitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE65NearestHitCommandlet();
	virtual int32 Main(const FString& Params) override;
};

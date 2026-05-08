// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPreIIIE6MultiHitCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPreIIIE6MultiHitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPreIIIE6MultiHitCommandlet();
	virtual int32 Main(const FString& Params) override;
};

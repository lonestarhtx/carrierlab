// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabV2Stage3Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabV2Stage3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabV2Stage3Commandlet();

	virtual int32 Main(const FString& Params) override;
};

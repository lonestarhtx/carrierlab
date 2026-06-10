// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabV2Stage2Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabV2Stage2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabV2Stage2Commandlet();

	virtual int32 Main(const FString& Params) override;
};

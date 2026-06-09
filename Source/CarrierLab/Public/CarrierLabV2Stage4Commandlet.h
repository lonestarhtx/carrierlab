// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabV2Stage4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabV2Stage4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabV2Stage4Commandlet();

	virtual int32 Main(const FString& Params) override;
};

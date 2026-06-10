// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabV2Milestone4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabV2Milestone4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabV2Milestone4Commandlet();

	virtual int32 Main(const FString& Params) override;
};

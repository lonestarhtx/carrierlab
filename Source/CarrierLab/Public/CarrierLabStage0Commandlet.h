// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabStage0Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabStage0Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabStage0Commandlet();

	virtual int32 Main(const FString& Params) override;
};

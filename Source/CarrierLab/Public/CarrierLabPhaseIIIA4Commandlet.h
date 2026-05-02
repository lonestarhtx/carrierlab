// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIA4Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIA4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIA4Commandlet();

	virtual int32 Main(const FString& Params) override;
};

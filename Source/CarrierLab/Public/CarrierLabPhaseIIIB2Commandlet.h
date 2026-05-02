// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB2Commandlet.generated.h"

UCLASS()
class UCarrierLabPhaseIIIB2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB2Commandlet();

	virtual int32 Main(const FString& Params) override;
};

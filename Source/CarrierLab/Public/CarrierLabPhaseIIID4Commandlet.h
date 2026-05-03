// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID4Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID4Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID4Commandlet();

	virtual int32 Main(const FString& Params) override;
};

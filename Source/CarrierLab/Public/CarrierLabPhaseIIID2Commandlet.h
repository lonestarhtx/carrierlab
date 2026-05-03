// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID2Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID2Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID2Commandlet();
	virtual int32 Main(const FString& Params) override;
};

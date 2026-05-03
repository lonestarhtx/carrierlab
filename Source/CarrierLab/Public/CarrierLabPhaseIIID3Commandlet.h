// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIID3Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIID3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIID3Commandlet();

	virtual int32 Main(const FString& Params) override;
};

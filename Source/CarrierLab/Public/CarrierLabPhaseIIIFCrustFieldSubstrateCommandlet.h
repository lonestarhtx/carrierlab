// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIFCrustFieldSubstrateCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIFCrustFieldSubstrateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIFCrustFieldSubstrateCommandlet();
	virtual int32 Main(const FString& Params) override;
};

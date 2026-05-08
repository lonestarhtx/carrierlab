// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet();

	virtual int32 Main(const FString& Params) override;
};

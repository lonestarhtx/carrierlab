// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet();
	virtual int32 Main(const FString& Params) override;
};

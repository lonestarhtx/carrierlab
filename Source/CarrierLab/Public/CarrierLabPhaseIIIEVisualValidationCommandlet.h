// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "CarrierLabPhaseIIIEVisualValidationCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIEVisualValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIEVisualValidationCommandlet();

	virtual int32 Main(const FString& Params) override;
};

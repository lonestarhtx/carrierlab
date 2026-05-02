// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIB3Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIB3Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIB3Commandlet();

	virtual int32 Main(const FString& Params) override;
};

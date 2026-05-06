// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPreIIIE8Commandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPreIIIE8Commandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPreIIIE8Commandlet();
	virtual int32 Main(const FString& Params) override;
};

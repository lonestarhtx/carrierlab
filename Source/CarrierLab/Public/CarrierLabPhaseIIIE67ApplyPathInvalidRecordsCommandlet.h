// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet.generated.h"

UCLASS()
class CARRIERLAB_API UCarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet();
	virtual int32 Main(const FString& Params) override;
};

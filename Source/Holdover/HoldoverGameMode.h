// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HoldoverGameMode.generated.h"

/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class AHoldoverGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHoldoverGameMode();

protected:
	virtual void BeginPlay() override;

	// --- shift spec (plan's first shift: 2 aircraft, 8 min, quota 2) ---
	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	float ShiftDuration = 480.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	int32 ShiftQuota = 2;
};




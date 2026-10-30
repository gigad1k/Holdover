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

	/** MP-3: evict truck occupants before controller cleanup destroys connection pawns. */
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	float ShiftDuration = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	int32 ShiftQuota = 1;
};




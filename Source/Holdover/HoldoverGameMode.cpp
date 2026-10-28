// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameMode.h"
#include "HoldoverGameState.h"
#include "HoldoverHUD.h"

AHoldoverGameMode::AHoldoverGameMode()
{
	HUDClass = AHoldoverHUD::StaticClass();
	GameStateClass = AHoldoverGameState::StaticClass();
}

void AHoldoverGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (AHoldoverGameState* GS = GetGameState<AHoldoverGameState>())
	{
		GS->StartShift(ShiftDuration, ShiftQuota);
	}
}

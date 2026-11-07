// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameMode.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverGameState.h"
#include "HoldoverHUD.h"
#include "HoldoverPlayerState.h"

AHoldoverGameMode::AHoldoverGameMode()
{
	HUDClass = AHoldoverHUD::StaticClass();
	GameStateClass = AHoldoverGameState::StaticClass();
	PlayerStateClass = AHoldoverPlayerState::StaticClass();
}

void AHoldoverGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (AHoldoverGameState* GS = GetGameState<AHoldoverGameState>())
	{
		GS->StartShift(ShiftDuration, ShiftQuota);
	}

	// DES-1: initialise all level-placed aircraft for the shift.
	for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
	{
		It->ResetForNewShift();
	}
}

// M8: truck-occupant eviction on disconnect lives in exactly one place -
// AHoldoverPlayerController::PawnLeavingGame (the engine hook that also owns
// protecting the level-placed truck from the default pawn teardown).

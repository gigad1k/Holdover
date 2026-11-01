// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameMode.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "HoldoverAircraft.h"
#include "HoldoverDeicingTruck.h"
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

void AHoldoverGameMode::Logout(AController* Exiting)
{
	// MP-3: a level-placed truck must never die as a connection's pawn. Evict the
	// occupant and free the truck before controller cleanup runs.
	if (Exiting)
	{
		if (AHoldoverDeicingTruck* Truck = Cast<AHoldoverDeicingTruck>(Exiting->GetPawn()))
		{
			Truck->EvictOccupant();
			Exiting->UnPossess();
		}
	}
	Super::Logout(Exiting);
}

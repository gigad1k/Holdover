// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameMode.h"
#include "HoldoverGameState.h"
#include "HoldoverHUD.h"

AHoldoverGameMode::AHoldoverGameMode()
{
	HUDClass = AHoldoverHUD::StaticClass();
	GameStateClass = AHoldoverGameState::StaticClass();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameMode.h"
#include "HoldoverHUD.h"

AHoldoverGameMode::AHoldoverGameMode()
{
	HUDClass = AHoldoverHUD::StaticClass();
}

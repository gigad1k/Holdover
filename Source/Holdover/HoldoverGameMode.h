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
};




// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HoldoverCharacter.h"
#include "HoldoverTestFixtures.generated.h"

/**
 * Concrete stand-in for the abstract game character: the game spawns Blueprint
 * subclasses of AHoldoverCharacter, but world tests need a spawnable class to
 * drive the real speed/ice/occupancy behaviour (PROC-1).
 */
UCLASS()
class AHoldoverTestCharacter : public AHoldoverCharacter
{
	GENERATED_BODY()
};

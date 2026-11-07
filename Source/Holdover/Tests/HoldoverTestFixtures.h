// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HoldoverCharacter.h"
#include "HoldoverPlayerController.h"
#include "HoldoverTestFixtures.generated.h"

/**
 * Concrete stand-ins for the abstract game classes: the game spawns Blueprint
 * subclasses, but world tests need spawnable classes to drive the real
 * speed/ice/occupancy behaviour (PROC-1).
 */
UCLASS()
class AHoldoverTestCharacter : public AHoldoverCharacter
{
	GENERATED_BODY()
};

UCLASS()
class AHoldoverTestController : public AHoldoverPlayerController
{
	GENERATED_BODY()

public:
	// Test worlds have no LocalPlayer, so the engine would never report this
	// controller as local. World tests need the locally-controlled seat (the
	// listen host / owning client path) to be reproducible.
	virtual bool IsLocalController() const override { return true; }
};

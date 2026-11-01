// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HoldoverShiftData.h"

/**
 * DES-4: built-in escalating shift profiles (night 1 easy -> night 5 blizzard).
 * These are the greybox defaults; designers can create UHoldoverShiftData
 * assets to override. Each profile tightens weather, deadlines and pay.
 */
namespace HoldoverShiftProfiles
{
	/** Creates 5 escalating UHoldoverShiftData objects (caller owns them). */
	TArray<UHoldoverShiftData*> CreateDefaultProfiles(UObject* Outer);

	/** Profile display name for the dispatcher / HUD. */
	FString GetProfileName(int32 ShiftNumber);
}

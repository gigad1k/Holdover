// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverCharacter.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverSpeedScaleTest,
	"Holdover.Movement.SpeedScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverSpeedScaleTest::RunTest(const FString& Parameters)
{
	// MP-2: sprint and ice are multiplicative scales — no raw MaxWalkSpeed writes
	// to compound into a permanent speed change (the old bug: enter ice -> sprint ->
	// leave ice = permanent 2x walk speed).
	TestEqual(TEXT("No modifiers is 1x"),
		AHoldoverCharacter::ComputeSpeedScale(false, 1.75f, 1.f), 1.f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Sprint is SprintScale"),
		AHoldoverCharacter::ComputeSpeedScale(true, 1.75f, 1.f), 1.75f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Ice is IceScale"),
		AHoldoverCharacter::ComputeSpeedScale(false, 1.75f, 0.5f), 0.5f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Sprint + ice multiply, never accumulate"),
		AHoldoverCharacter::ComputeSpeedScale(true, 1.75f, 0.5f), 0.875f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Leaving ice restores sprint-only scale"),
		AHoldoverCharacter::ComputeSpeedScale(true, 1.75f, 1.f), 1.75f, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverDeicingTruck.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverBoomControlsTest,
	"Holdover.Truck.BoomClampAndApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverBoomControlsTest::RunTest(const FString& Parameters)
{
	// Pure clamp math.
	TestEqual(TEXT("Clamp within range"), AHoldoverDeicingTruck::ClampExtension(500.f, 150.f, 900.f), 500.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Clamp below min"), AHoldoverDeicingTruck::ClampExtension(10.f, 150.f, 900.f), 150.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Clamp above max"), AHoldoverDeicingTruck::ClampExtension(5000.f, 150.f, 900.f), 900.f, KINDA_SMALL_NUMBER);

	// SetBoom applies and clamps.
	AHoldoverDeicingTruck* Truck = NewObject<AHoldoverDeicingTruck>(GetTransientPackage());
	Truck->SetBoom(45.f, 10000.f);
	TestEqual(TEXT("Yaw applied"), Truck->BoomYaw, 45.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Extension clamped to max"), Truck->BoomExtension, Truck->MaxExtension, KINDA_SMALL_NUMBER);

	Truck->SetBoom(-30.f, 0.f);
	TestEqual(TEXT("Extension clamped to min"), Truck->BoomExtension, Truck->MinExtension, KINDA_SMALL_NUMBER);

	// Fresh truck is not occupied.
	TestFalse(TEXT("Not occupied initially"), Truck->IsOccupied());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

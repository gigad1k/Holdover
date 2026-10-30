// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverSprayComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverSprayDtClampTest,
	"Holdover.Spray.DtClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverSprayDtClampTest::RunTest(const FString& Parameters)
{
	// MP-8: ServerSpray must never trust client-supplied DeltaSeconds.
	TestEqual(TEXT("Zero dt rejected"),
		UHoldoverSprayComponent::ClampSprayDt(0.f, 0.1f), 0.f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Negative dt rejected"),
		UHoldoverSprayComponent::ClampSprayDt(-5.f, 0.1f), 0.f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Normal dt passes unchanged"),
		UHoldoverSprayComponent::ClampSprayDt(0.1f, 0.1f), 0.1f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Huge dt clamped to 2x interval"),
		UHoldoverSprayComponent::ClampSprayDt(1000.f, 0.1f), 0.2f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Doubled dt still passes"),
		UHoldoverSprayComponent::ClampSprayDt(0.2f, 0.1f), 0.2f, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

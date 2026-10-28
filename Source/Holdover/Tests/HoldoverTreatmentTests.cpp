// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverSurfaceSection.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverTreatmentIndependenceTest,
	"Holdover.Treatment.SectionIndependenceAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverTreatmentIndependenceTest::RunTest(const FString& Parameters)
{
	AHoldoverSurfaceSection* SectionA = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	AHoldoverSurfaceSection* SectionB = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());

	// Spraying section A must not change section B.
	const float Applied = SectionA->ApplyCleaning(50.f);
	TestEqual(TEXT("Applied amount"), Applied, 50.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Section A cleanliness"), SectionA->CleanlinessPercent, 50.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Section B untouched"), SectionB->CleanlinessPercent, 0.f, KINDA_SMALL_NUMBER);

	// Clamping at 100.
	SectionA->ApplyCleaning(75.f);
	TestEqual(TEXT("Section A clamped"), SectionA->CleanlinessPercent, 100.f, KINDA_SMALL_NUMBER);

	// Protection is independent of cleanliness.
	SectionB->ApplyProtection(30.f);
	TestEqual(TEXT("Section B protection"), SectionB->ProtectionPercent, 30.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Section B cleanliness still 0"), SectionB->CleanlinessPercent, 0.f, KINDA_SMALL_NUMBER);

	// Clean threshold.
	SectionB->ApplyCleaning(SectionB->CleanThreshold);
	TestTrue(TEXT("Section B clean at threshold"), SectionB->IsClean());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

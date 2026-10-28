// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverAircraft.h"
#include "HoldoverSprayComponent.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverFluidRulesTest,
	"Holdover.Treatment.FluidRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverFluidRulesTest::RunTest(const FString& Parameters)
{
	AHoldoverSurfaceSection* Section = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	UHoldoverSprayComponent* Spray = NewObject<UHoldoverSprayComponent>(GetTransientPackage());

	// Wrong fluid: Type IV on a dirty section is rejected and consumes nothing.
	Spray->ActiveFluidType = EHoldoverFluidType::TypeIV;
	Spray->TypeIVFluid = 100.f;
	TestFalse(TEXT("Type IV rejected on dirty section"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Protection unchanged"), Section->ProtectionPercent, 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Type IV tank untouched"), Spray->TypeIVFluid, 100.f, KINDA_SMALL_NUMBER);

	// Type I cleans and consumes only the Type I tank.
	Spray->ActiveFluidType = EHoldoverFluidType::TypeI;
	Spray->TypeIFluid = 100.f;
	TestTrue(TEXT("Type I applied"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Cleanliness raised"), Section->CleanlinessPercent, Spray->CleanRatePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Type I consumed"), Spray->TypeIFluid, 100.f - Spray->TypeIUsePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Type IV untouched"), Spray->TypeIVFluid, 100.f, KINDA_SMALL_NUMBER);

	// Empty Type I tank stops treatment.
	Spray->TypeIFluid = 0.f;
	TestFalse(TEXT("Empty Type I tank rejected"), Spray->ApplySpray(Section, 1.f));

	// Type IV protects a clean section and consumes only the Type IV tank.
	Section->CleanlinessPercent = 100.f;
	Spray->ActiveFluidType = EHoldoverFluidType::TypeIV;
	Spray->TypeIVFluid = 100.f;
	Spray->TypeIFluid = 100.f;
	TestTrue(TEXT("Type IV applied on clean section"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Protection raised"), Section->ProtectionPercent, Spray->ProtectRatePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Type IV consumed"), Spray->TypeIVFluid, 100.f - Spray->TypeIVUsePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Type I untouched"), Spray->TypeIFluid, 100.f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverStateMachineTest,
	"Holdover.Treatment.HoldoverStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverStateMachineTest::RunTest(const FString& Parameters)
{
	AHoldoverAircraft* Aircraft = NewObject<AHoldoverAircraft>(GetTransientPackage());
	AHoldoverSurfaceSection* A = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	AHoldoverSurfaceSection* B = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	Aircraft->RegisterSection(A);
	Aircraft->RegisterSection(B);

	// Incomplete coverage: holdover must not start.
	Aircraft->SimulateTreatment(1.f, 0.f);
	TestEqual(TEXT("State Incomplete"), (int32)Aircraft->HoldoverState, (int32)EHoldoverState::Incomplete);

	// Fully treated: holdover starts at full duration.
	A->CleanlinessPercent = 100.f;
	A->ProtectionPercent = 100.f;
	B->CleanlinessPercent = 100.f;
	B->ProtectionPercent = 100.f;
	Aircraft->SimulateTreatment(0.1f, 0.f);
	TestEqual(TEXT("State HoldoverActive"), (int32)Aircraft->HoldoverState, (int32)EHoldoverState::HoldoverActive);
	TestEqual(TEXT("Holdover full"), Aircraft->HoldoverRemaining, Aircraft->HoldoverDuration, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Cleared for takeoff"), Aircraft->IsClearedForTakeoff());

	// Weather burns the clock faster and degrades protection.
	Aircraft->SimulateTreatment(1.f, 1.f);
	const float ExpectedBurn = 1.f * (1.f + 1.f * Aircraft->WeatherBurnMultiplier);
	TestEqual(TEXT("Weather burns clock"), Aircraft->HoldoverRemaining,
		Aircraft->HoldoverDuration - ExpectedBurn, 0.01f);
	TestTrue(TEXT("Protection degraded"), A->ProtectionPercent < 100.f);

	// Worsening weather forces retreatment (protection drops below threshold).
	Aircraft->SimulateTreatment(1000.f, 1.f);
	TestTrue(TEXT("Treatment lost under weather"),
		Aircraft->HoldoverState == EHoldoverState::Incomplete ||
		Aircraft->HoldoverState == EHoldoverState::HoldoverExpired);

	// Valid retreatment after expiry restarts the clock.
	Aircraft->HoldoverState = EHoldoverState::HoldoverExpired;
	Aircraft->HoldoverRemaining = 0.f;
	A->CleanlinessPercent = 100.f;
	A->ProtectionPercent = 100.f;
	B->CleanlinessPercent = 100.f;
	B->ProtectionPercent = 100.f;
	Aircraft->SimulateTreatment(0.1f, 0.f);
	TestEqual(TEXT("Retreatment restarts holdover"), (int32)Aircraft->HoldoverState, (int32)EHoldoverState::HoldoverActive);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

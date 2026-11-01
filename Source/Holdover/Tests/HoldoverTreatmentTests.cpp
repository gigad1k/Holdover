// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverAircraft.h"
#include "HoldoverPlayerState.h"
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

	// DES-2 one-tank rule: Type IV on a dirty section is rejected and consumes nothing.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeIV;
	Spray->FluidLevel = 100.f;
	TestFalse(TEXT("Type IV rejected on dirty section"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Protection unchanged"), Section->ProtectionPercent, 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tank untouched on rejection"), Spray->FluidLevel, 100.f, KINDA_SMALL_NUMBER);

	// Type I cleans and consumes the single tank.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("Type I applied"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Cleanliness raised"), Section->CleanlinessPercent, Spray->CleanRatePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tank consumed"), Spray->FluidLevel, 100.f - Spray->TypeIUsePerSecond, KINDA_SMALL_NUMBER);

	// Empty tank stops treatment.
	Spray->FluidLevel = 0.f;
	TestFalse(TEXT("Empty tank rejected"), Spray->ApplySpray(Section, 1.f));

	// Type IV protects a clean section and consumes the single tank.
	Section->CleanlinessPercent = 100.f;
	Spray->CarriedFluidType = EHoldoverFluidType::TypeIV;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("Type IV applied on clean section"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Protection raised"), Section->ProtectionPercent, Spray->ProtectRatePerSecond, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tank consumed by Type IV"), Spray->FluidLevel, 100.f - Spray->TypeIVUsePerSecond, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverSectionOwnershipTest,
	"Holdover.Treatment.SectionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverSectionOwnershipTest::RunTest(const FString& Parameters)
{
	// SIM-5: explicit ownership beats proximity.
	AHoldoverAircraft* PlaneA = NewObject<AHoldoverAircraft>(GetTransientPackage());
	AHoldoverAircraft* PlaneB = NewObject<AHoldoverAircraft>(GetTransientPackage());
	AHoldoverSurfaceSection* Section = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());

	// Explicit owner wins.
	Section->OwningAircraft = PlaneB;
	PlaneA->RegisterSection(Section);
	// RegisterSection must NOT steal a section that already has an explicit owner.
	TestTrue(TEXT("Explicit owner preserved"), Section->OwningAircraft.Get() == PlaneB);
	TestFalse(TEXT("Plane A did not claim it"), PlaneA->GetSections().Contains(Section));

	// An unowned section can be claimed.
	AHoldoverSurfaceSection* Unowned = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	PlaneA->RegisterSection(Unowned);
	TestTrue(TEXT("RegisterSection claims unowned"), Unowned->OwningAircraft.Get() == PlaneA);
	TestTrue(TEXT("Plane A owns it"), PlaneA->GetSections().Contains(Unowned));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverPlayerStateAttributionTest,
	"Holdover.Treatment.PlayerStateAttribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverPlayerStateAttributionTest::RunTest(const FString& Parameters)
{
	AHoldoverPlayerState* PS = NewObject<AHoldoverPlayerState>(GetTransientPackage());

	// Earnings and penalties accumulate independently.
	PS->AddEarnings(100);
	PS->AddEarnings(50);
	PS->AddPenalty(25);
	TestEqual(TEXT("Earnings accumulated"), PS->Earnings, 150);
	TestEqual(TEXT("Penalties accumulated"), PS->Penalties, 25);

	// Departure counting.
	PS->RecordDeparture();
	PS->RecordDeparture();
	TestEqual(TEXT("Departures counted"), PS->DeparturesHandled, 2);

	// One-tank fluid choice.
	PS->SetCarriedFluid(EHoldoverFluidType::TypeIV);
	TestEqual(TEXT("Fluid choice stored"), (int32)PS->CarriedFluidType, (int32)EHoldoverFluidType::TypeIV);

	// Reset clears per-shift stats.
	PS->ResetForNewShift();
	TestEqual(TEXT("Earnings reset"), PS->Earnings, 0);
	TestEqual(TEXT("Penalties reset"), PS->Penalties, 0);
	TestEqual(TEXT("Departures reset"), PS->DeparturesHandled, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverOneTankRefillTest,
	"Holdover.Treatment.OneTankRefill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverOneTankRefillTest::RunTest(const FString& Parameters)
{
	UHoldoverSprayComponent* Spray = NewObject<UHoldoverSprayComponent>(GetTransientPackage());

	// DES-2 one-tank rule: carried fluid is Type I by default, single tank.
	TestEqual(TEXT("Default fluid Type I"), (int32)Spray->CarriedFluidType, (int32)EHoldoverFluidType::TypeI);
	TestEqual(TEXT("Default tank full"), Spray->FluidLevel, 100.f, KINDA_SMALL_NUMBER);

	// The tank drains with use.
	Spray->FluidLevel = 50.f;
	TestEqual(TEXT("Tank level tracked"), Spray->FluidLevel, 50.f, KINDA_SMALL_NUMBER);

	// Refill progress is 0 when not refilling.
	TestFalse(TEXT("Not refilling initially"), Spray->IsRefilling());
	TestEqual(TEXT("Refill progress zero"), Spray->GetRefillProgress(), 0.f, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

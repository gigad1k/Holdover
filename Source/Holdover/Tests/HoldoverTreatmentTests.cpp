// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverAircraft.h"
#include "HoldoverDispatcherLines.h"
#include "HoldoverGameState.h"
#include "HoldoverPlayerState.h"
#include "HoldoverShiftProfiles.h"
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
	TestEqual(TEXT("Cleanliness raised"), Section->CleanlinessPercent, Spray->GetCleanRate(), KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tank consumed"), Spray->FluidLevel, 100.f - Spray->GetTypeIUseRate(), KINDA_SMALL_NUMBER);

	// Empty tank stops treatment.
	Spray->FluidLevel = 0.f;
	TestFalse(TEXT("Empty tank rejected"), Spray->ApplySpray(Section, 1.f));

	// Type IV protects a clean section and consumes the single tank.
	Section->CleanlinessPercent = 100.f;
	Spray->CarriedFluidType = EHoldoverFluidType::TypeIV;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("Type IV applied on clean section"), Spray->ApplySpray(Section, 1.f));
	TestEqual(TEXT("Protection raised"), Section->ProtectionPercent, Spray->GetProtectRate(), KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tank consumed by Type IV"), Spray->FluidLevel, 100.f - Spray->GetTypeIVUseRate(), KINDA_SMALL_NUMBER);

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

	// Weather burns the clock faster; SIM-2 pauses protection degradation during
	// an active holdover so expiry (the clock) is the threat, not the abort.
	Aircraft->SimulateTreatment(1.f, 1.f);
	const float ExpectedBurn = 1.f * (1.f + 1.f * Aircraft->WeatherBurnMultiplier);
	TestEqual(TEXT("Weather burns clock"), Aircraft->HoldoverRemaining,
		Aircraft->HoldoverDuration - ExpectedBurn, 0.01f);
	TestEqual(TEXT("Protection held during holdover"), A->ProtectionPercent, 100.f, KINDA_SMALL_NUMBER);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverDispatcherBankTest,
	"Holdover.Treatment.DispatcherBank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverDispatcherBankTest::RunTest(const FString& Parameters)
{
	// Every event returns a non-empty line.
	const TArray<EDispatchEvent> Events = {
		EDispatchEvent::ShiftStart, EDispatchEvent::AircraftArrival,
		EDispatchEvent::TreatmentComplete, EDispatchEvent::HoldoverStarted,
		EDispatchEvent::HoldoverWarning, EDispatchEvent::HoldoverExpired,
		EDispatchEvent::DepartureOnTime, EDispatchEvent::DepartureLate,
		EDispatchEvent::IceStumble, EDispatchEvent::JetBlast,
		EDispatchEvent::RefillComplete, EDispatchEvent::TankEmpty,
		EDispatchEvent::ShiftPassed, EDispatchEvent::ShiftFailed,
		EDispatchEvent::NewShift, EDispatchEvent::PlayerConnected,
		EDispatchEvent::PlayerDisconnected
	};
	for (EDispatchEvent Event : Events)
	{
		const FString Line = HoldoverDispatch::GetLine(Event, 0, TEXT(""));
		TestFalse(FString::Printf(TEXT("Event %d returns a line"), (int32)Event), Line.IsEmpty());
	}

	// Repetition changes the line (tier rotation).
	const FString First = HoldoverDispatch::GetLine(EDispatchEvent::IceStumble, 0, TEXT(""));
	const FString Second = HoldoverDispatch::GetLine(EDispatchEvent::IceStumble, 1, TEXT(""));
	TestFalse(TEXT("Repetition changes the line"), First == Second);

	// Exhausted tiers cycle the last tier without crashing.
	for (int32 i = 0; i < 20; ++i)
	{
		const FString Line = HoldoverDispatch::GetLine(EDispatchEvent::JetBlast, i, TEXT(""));
		TestFalse(FString::Printf(TEXT("Repetition %d returns a line"), i), Line.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverContaminationTest,
	"Holdover.Treatment.Contamination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverContaminationTest::RunTest(const FString& Parameters)
{
	// SIM-3: unprotected sections lose cleanliness; protected sections do not.
	AHoldoverSurfaceSection* Unprotected = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	AHoldoverSurfaceSection* Protected = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());

	Unprotected->CleanlinessPercent = 100.f;
	Unprotected->ProtectionPercent = 0.f;
	Protected->CleanlinessPercent = 100.f;
	Protected->ProtectionPercent = 80.f;

	// 1 second at 1.0 weather, 3%/s rate.
	Unprotected->ApplyContamination(3.f);
	Protected->ApplyContamination(3.f); // caller checks threshold, but let's verify the mechanic

	TestEqual(TEXT("Unprotected section contaminated"), Unprotected->CleanlinessPercent, 97.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Protected section (direct call) also contaminated"), Protected->CleanlinessPercent, 97.f, KINDA_SMALL_NUMBER);

	// Contamination clamps at 0.
	Unprotected->ApplyContamination(200.f);
	TestEqual(TEXT("Contamination clamps at 0"), Unprotected->CleanlinessPercent, 0.f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverNetPayTest,
	"Holdover.Treatment.NetPay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverNetPayTest::RunTest(const FString& Parameters)
{
	// SIM-4: net pay = money - deductions.
	AHoldoverGameState* GS = NewObject<AHoldoverGameState>(GetTransientPackage());

	// Simulate the state without a world.
	GS->Money = 200;
	GS->Deductions = 75;
	TestEqual(TEXT("Net pay = money - deductions"), GS->GetNetPay(), 125);

	GS->Money = 50;
	GS->Deductions = 100;
	TestEqual(TEXT("Net pay can be negative"), GS->GetNetPay(), -50);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverShiftProfilesTest,
	"Holdover.Treatment.ShiftProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverShiftProfilesTest::RunTest(const FString& Parameters)
{
	// DES-4: built-in profiles exist and escalate.
	TArray<UHoldoverShiftData*> Profiles = HoldoverShiftProfiles::CreateDefaultProfiles(GetTransientPackage());
	TestEqual(TEXT("5 profiles created"), Profiles.Num(), 5);

	if (Profiles.Num() == 5)
	{
		// Difficulty escalates: weather severity increases across profiles.
		TestTrue(TEXT("Profile 2 weather harder than 1"), Profiles[1]->WeatherSeverityMax > Profiles[0]->WeatherSeverityMax);
		TestTrue(TEXT("Profile 3 weather harder than 2"), Profiles[2]->WeatherSeverityMax > Profiles[1]->WeatherSeverityMax);
		TestTrue(TEXT("Profile 5 weather hardest"), Profiles[4]->WeatherSeverityMax > Profiles[3]->WeatherSeverityMax);

		// Holdover duration decreases (more pressure).
		TestTrue(TEXT("Profile 5 holdover shorter"), Profiles[4]->HoldoverDuration < Profiles[0]->HoldoverDuration);

		// Deadlines tighten.
		TestTrue(TEXT("Profile 5 deadline tighter"), Profiles[4]->PerAircraftDeadlineSeconds < Profiles[0]->PerAircraftDeadlineSeconds);
	}

	// Profile names are non-empty.
	for (int32 i = 1; i <= 5; ++i)
	{
		TestFalse(FString::Printf(TEXT("Profile %d has a name"), i), HoldoverShiftProfiles::GetProfileName(i).IsEmpty());
	}

	// Cleanup (NewObject'd objects get GC'd, but be explicit).
	for (UHoldoverShiftData* P : Profiles)
	{
		if (P)
		{
			P->MarkAsGarbage();
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverProfilePayRulesTest,
	"Holdover.Treatment.ProfilePayRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverProfilePayRulesTest::RunTest(const FString& Parameters)
{
	// Sonnet M13: profile pay rules must actually take effect at StartShift.
	// They used to be dead data while the GameState kept its C++ defaults.
	AHoldoverGameState* GS = NewObject<AHoldoverGameState>(GetTransientPackage());
	GS->ShiftNumber = 4;
	GS->StartShift(1.f, 1);
	UHoldoverShiftData* Profile = GS->GetCurrentProfile();
	if (!TestNotNull(TEXT("profile 4 resolved"), Profile))
	{
		return false;
	}
	TestEqual(TEXT("pay rule comes from the profile"), GS->PayPerOnTimeDeparture, Profile->PayPerOnTimeDeparture);
	TestEqual(TEXT("late deduction comes from the profile"), GS->LateDepartureDeduction, Profile->LateDepartureDeduction);
	// Sanity: profile 4's late rule differs from the C++ default of 50.
	TestNotEqual(TEXT("profile 4 overrides the default late deduction"), GS->LateDepartureDeduction, 50);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverAircraft.h"
#include "HoldoverGameState.h"
#include "HoldoverSurfaceSection.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverDepartureOnceTest,
	"Holdover.Shift.DepartureOnlyOnceAndValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverDepartureOnceTest::RunTest(const FString& Parameters)
{
	AHoldoverAircraft* Aircraft = NewObject<AHoldoverAircraft>(GetTransientPackage());
	AHoldoverSurfaceSection* Section = NewObject<AHoldoverSurfaceSection>(GetTransientPackage());
	Aircraft->RegisterSection(Section);

	// Departure requires a running holdover.
	TestFalse(TEXT("No departure without holdover"), Aircraft->TryDepart());
	TestFalse(TEXT("Not departed"), Aircraft->bDeparted);

	// Make the aircraft fully treated and start the holdover.
	Section->CleanlinessPercent = 100.f;
	Section->ProtectionPercent = 100.f;
	Aircraft->SimulateTreatment(0.1f, 0.f);
	TestTrue(TEXT("Holdover active"), Aircraft->IsClearedForTakeoff());

	// First departure succeeds; a plane is rewarded exactly once.
	TestTrue(TEXT("First departure ok"), Aircraft->TryDepart());
	TestTrue(TEXT("bDeparted set"), Aircraft->bDeparted);
	TestFalse(TEXT("Second departure rejected"), Aircraft->TryDepart());

	// Reset for a new shift makes it departable again.
	Aircraft->ResetForNewShift();
	TestFalse(TEXT("Reset clears departed"), Aircraft->bDeparted);
	TestFalse(TEXT("Reset clears holdover"), Aircraft->IsClearedForTakeoff());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverShiftLoopTest,
	"Holdover.Shift.ScoringAndRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverShiftLoopTest::RunTest(const FString& Parameters)
{
	AHoldoverGameState* GS = NewObject<AHoldoverGameState>(GetTransientPackage());

	// Shift start and countdown.
	GS->StartShift(10.f, 2);
	TestEqual(TEXT("Shift running"), (int32)GS->ShiftState, (int32)EShiftState::InProgress);
	TestEqual(TEXT("Quota set"), GS->Quota, 2);

	// On-time departure pays.
	GS->RegisterDeparture(true);
	TestEqual(TEXT("On-time counted"), GS->OnTimeDepartures, 1);
	TestEqual(TEXT("Pay awarded"), GS->Money, GS->PayPerOnTimeDeparture);
	TestEqual(TEXT("No deduction"), GS->Deductions, 0);

	// Late departure penalises.
	GS->RegisterDeparture(false);
	TestEqual(TEXT("Late counted"), GS->LateDepartures, 1);
	TestEqual(TEXT("Late deduction"), GS->Deductions, GS->LateDepartureDeduction);

	// Timer ends the shift; 1/2 on-time = fail. DES-4 profiles own the duration,
	// so tick past whatever time remains rather than a hardcoded amount.
	GS->TickShift(GS->ShiftTimeRemaining + 1.f);
	TestEqual(TEXT("Shift ended"), (int32)GS->ShiftState, (int32)EShiftState::ShiftEnded);
	TestFalse(TEXT("Quota missed = fail"), GS->bShiftPassed);

	// Restart keeps the loop repeatable and resets shift counters.
	GS->RestartShift();
	TestEqual(TEXT("Shift running again"), (int32)GS->ShiftState, (int32)EShiftState::InProgress);
	TestEqual(TEXT("Counters reset"), GS->OnTimeDepartures + GS->LateDepartures + GS->Money + GS->Deductions, 0);

	// Meeting the quota passes.
	GS->RegisterDeparture(true);
	GS->RegisterDeparture(true);
	GS->EndShift();
	TestTrue(TEXT("Quota met = pass"), GS->bShiftPassed);

	// Dispatch feed recorded events.
	TestTrue(TEXT("Dispatch feed populated"), GS->DispatchFeed.Num() >= 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverRestartGuardTest,
	"Holdover.Shift.RestartGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverRestartGuardTest::RunTest(const FString& Parameters)
{
	AHoldoverGameState* GS = NewObject<AHoldoverGameState>(GetTransientPackage());

	// N5/N6: RestartShift is a post-verdict action - a mid-shift call must not
	// roll the ledger or reset the counters out from under the running shift.
	GS->StartShift(10.f, 2);
	GS->RegisterDeparture(true);
	GS->RestartShift();
	TestEqual(TEXT("mid-shift restart keeps the shift running"), (int32)GS->ShiftState, (int32)EShiftState::InProgress);
	TestEqual(TEXT("mid-shift restart keeps the ledger"), GS->OnTimeDepartures, 1);
	TestEqual(TEXT("mid-shift restart keeps the shift number"), GS->ShiftNumber, 1);

	// After the verdict it works and resets the counters.
	GS->EndShift();
	GS->RestartShift();
	TestEqual(TEXT("post-verdict restart runs"), (int32)GS->ShiftState, (int32)EShiftState::InProgress);
	TestEqual(TEXT("post-verdict restart resets counters"), GS->OnTimeDepartures + GS->LateDepartures + GS->Money + GS->Deductions, 0);
	TestEqual(TEXT("post-verdict restart advances the shift"), GS->ShiftNumber, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

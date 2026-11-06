// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameState.h"
#include "EngineUtils.h"
#include "Holdover.h"
#include "HoldoverAircraft.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverPlayerState.h"
#include "HoldoverShiftData.h"
#include "HoldoverShiftProfiles.h"
#include "Net/UnrealNetwork.h"

AHoldoverGameState::AHoldoverGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

// --- DES-5: shift tuning accessors (current profile with local fallbacks) ---

float AHoldoverGameState::GetHoldoverDuration() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->HoldoverDuration : 120.f;
}

float AHoldoverGameState::GetWeatherBurnMultiplier() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->WeatherBurnMultiplier : 2.f;
}

float AHoldoverGameState::GetProtectionDegradationPerSecond() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->ProtectionDegradationPerSecond : 2.f;
}

float AHoldoverGameState::GetHoldoverDegradationScale() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->HoldoverDegradationScale : 0.f;
}

float AHoldoverGameState::GetReContaminationRatePerSecond() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->ReContaminationRatePerSecond : 3.f;
}

float AHoldoverGameState::GetReContaminationProtectionThreshold() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->ReContaminationProtectionThreshold : 50.f;
}

int32 AHoldoverGameState::GetHoldoverAbortPenalty() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->HoldoverAbortPenalty : 15;
}

float AHoldoverGameState::GetPerAircraftDeadlineSeconds() const
{
	const UHoldoverShiftData* P = GetCurrentProfile();
	return P ? P->PerAircraftDeadlineSeconds : 300.f;
}

// --- DES-4: shift profiles ---

void AHoldoverGameState::EnsureShiftProfiles()
{
	if (ShiftProfiles.Num() == 0)
	{
		ShiftProfiles = HoldoverShiftProfiles::CreateDefaultProfiles(this);
	}
}

const UHoldoverShiftData* AHoldoverGameState::GetCurrentProfile() const
{
	if (ShiftProfiles.Num() > 0)
	{
		const int32 Index = FMath::Clamp(ShiftNumber - 1, 0, ShiftProfiles.Num() - 1);
		return ShiftProfiles[Index];
	}
	return ShiftData;
}

FString AHoldoverGameState::GetProfileName() const
{
	return HoldoverShiftProfiles::GetProfileName(ShiftNumber);
}

void AHoldoverGameState::SetWeatherSeverity(float NewSeverity)
{
	if (HasAuthority())
	{
		WeatherSeverity = FMath::Clamp(NewSeverity, 0.f, 1.f);
	}
}

void AHoldoverGameState::StartShift(float DurationSeconds, int32 InQuota)
{
	// N6: shift state is server-authoritative - a client call must not desync.
	if (!HasAuthority())
	{
		return;
	}
	// DES-4: use the current profile's spec when available.
	EnsureShiftProfiles();
	if (const UHoldoverShiftData* Profile = GetCurrentProfile())
	{
		LastShiftDuration = Profile->ShiftDuration;
		Quota = Profile->ShiftQuota;
		// Sonnet M13: the profile's pay rules were dead data - apply them so a
		// profile that says "late costs 75" actually charges 75.
		PayPerOnTimeDeparture = Profile->PayPerOnTimeDeparture;
		LateDepartureDeduction = Profile->LateDepartureDeduction;
	}
	else
	{
		LastShiftDuration = FMath::Max(0.f, DurationSeconds);
		Quota = FMath::Max(1, InQuota);
	}
	ShiftState = EShiftState::InProgress;
	ShiftTimeRemaining = LastShiftDuration;
	OnTimeDepartures = 0;
	LateDepartures = 0;
	Money = 0;
	Deductions = 0;
	bShiftPassed = false;

	// SIM-1: randomise weather for this shift (squalls and lulls, not a monotonic ramp).
	const UHoldoverShiftData* Profile = GetCurrentProfile();
	if (Profile)
	{
		WeatherBaseSeverity = FMath::FRandRange(Profile->WeatherSeverityMin, Profile->WeatherSeverityMax);
		WeatherPhase = FMath::FRandRange(0.f, 2.f * PI);
	}
	else
	{
		WeatherBaseSeverity = FMath::FRandRange(0.1f, 0.5f);
		WeatherPhase = FMath::FRandRange(0.f, 2.f * PI);
	}
	WeatherTime = 0.f;
	WeatherSeverity = WeatherBaseSeverity;

	// DES-2: reset per-player attribution for the new shift.
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const AController* C = It->Get())
			{
				if (AHoldoverPlayerState* PS = C->GetPlayerState<AHoldoverPlayerState>())
				{
					PS->ResetForNewShift();
				}
			}
		}
	}

	DispatchEvent(EDispatchEvent::ShiftStart);
}

void AHoldoverGameState::TickShift(float DeltaSeconds)
{
	if (ShiftState != EShiftState::InProgress || DeltaSeconds <= 0.f)
	{
		return;
	}

	ShiftTimeRemaining -= DeltaSeconds;
	if (ShiftTimeRemaining <= 0.f)
	{
		ShiftTimeRemaining = 0.f;
		EndShift();
	}
}

void AHoldoverGameState::EndShift()
{
	// N6: shift state is server-authoritative - a client call must not desync.
	if (!HasAuthority())
	{
		return;
	}
	if (ShiftState == EShiftState::ShiftEnded)
	{
		return;
	}
	ShiftState = EShiftState::ShiftEnded;
	// SIM-4: net pay matters — a shift passes only with enough departures AND positive net pay.
	bShiftPassed = (OnTimeDepartures >= Quota) && (GetNetPay() > 0);
	DispatchEvent(bShiftPassed ? EDispatchEvent::ShiftPassed : EDispatchEvent::ShiftFailed);
}

void AHoldoverGameState::RestartShift()
{
	// N5/N6: restarting is a post-verdict, server-only action. A mid-shift call
	// must not roll the ledger or reset the running shift's counters.
	if (!HasAuthority() || ShiftState != EShiftState::ShiftEnded)
	{
		return;
	}
	// DES-4: advance the shift number and update the running ledger.
	++ShiftNumber;
	TotalCompanyPay += Money;
	TotalCompanyDeductions += Deductions;
	CompanyLedger = TotalCompanyPay - TotalCompanyDeductions;

	DispatchEvent(EDispatchEvent::NewShift);
	StartShift(LastShiftDuration, Quota);

	// DES-1: reset all aircraft for the fresh shift.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHoldoverAircraft> It(World); It; ++It)
		{
			It->ResetForNewShift();
		}
	}
}

void AHoldoverGameState::RegisterDeparture(bool bOnTime, AHoldoverPlayerState* ResponsiblePlayer)
{
	// N6: shift state is server-authoritative - a client call must not desync.
	if (!HasAuthority())
	{
		return;
	}
	// Sonnet H4: the ledger freezes at the verdict - post-EndShift departures
	// must not move Money/Deductions under the already-computed PASSED/FAILED.
	if (ShiftState != EShiftState::InProgress)
	{
		return;
	}
	const FString Who = ResponsiblePlayer ? ResponsiblePlayer->GetShortName() : TEXT("");
	if (bOnTime)
	{
		OnTimeDepartures++;
		Money += PayPerOnTimeDeparture;
		DispatchEvent(EDispatchEvent::DepartureOnTime, Who);
	}
	else
	{
		LateDepartures++;
		Deductions += LateDepartureDeduction;
		DispatchEvent(EDispatchEvent::DepartureLate, Who);
	}
}

void AHoldoverGameState::RegisterPenalty(int32 Amount, const FString& Reason, AHoldoverPlayerState* ResponsiblePlayer, EDispatchEvent Event)
{
	// N6: shift state is server-authoritative - a client call must not desync.
	if (!HasAuthority())
	{
		return;
	}
	// Sonnet H4: penalties (holdover aborts, jet blast) stop at the verdict too.
	if (ShiftState != EShiftState::InProgress || Amount <= 0)
	{
		return;
	}
	Deductions += Amount;
	const FString Who = ResponsiblePlayer ? ResponsiblePlayer->GetShortName() : TEXT("");
	UE_LOG(LogHoldover, Display, TEXT("Penalty: %s -$%d (%s)"), *Who, Amount, *Reason);
	DispatchEvent(Event, Who);
}

void AHoldoverGameState::AddDispatch(const FString& Line)
{
	DispatchFeed.Add(FText::FromString(Line));
	if (DispatchFeed.Num() > 8)
	{
		DispatchFeed.RemoveAt(0, DispatchFeed.Num() - 8, EAllowShrinking::No);
	}
	UE_LOG(LogTemp, Display, TEXT("DISPATCH: %s"), *Line);

	if (GetWorld())
	{
		UHoldoverAudioLibrary::Play2D(this, FName("radio_beep"), 0.6f);
	}
}

void AHoldoverGameState::DispatchEvent(EDispatchEvent Event, const FString& PlayerName)
{
	int32& Count = EventRepetitionCounts.FindOrAdd(Event);
	const FString Line = HoldoverDispatch::GetLine(Event, Count, PlayerName);
	++Count;
	AddDispatch(Line);
}

void AHoldoverGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (bAutoRampWeather && ShiftState == EShiftState::InProgress)
		{
			// SIM-1: squalls and lulls — oscillate around the shift's base severity
			// instead of a one-way ramp to max.
			WeatherTime += DeltaSeconds;
			const UHoldoverShiftData* P = GetCurrentProfile();
			const float Period = P ? P->WeatherPeriodSeconds : 90.f;
			const float Amplitude = P ? P->WeatherAmplitude : 0.2f;
			const float Wave = FMath::Sin(2.f * PI * (WeatherTime + WeatherPhase) / FMath::Max(Period, 1.f));
			WeatherSeverity = FMath::Clamp(WeatherBaseSeverity + Amplitude * Wave, 0.f, 1.f);
		}
		TickShift(DeltaSeconds);
	}
}

void AHoldoverGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverGameState, WeatherSeverity);
	DOREPLIFETIME(AHoldoverGameState, ShiftState);
	DOREPLIFETIME(AHoldoverGameState, ShiftTimeRemaining);
	DOREPLIFETIME(AHoldoverGameState, Quota);
	DOREPLIFETIME(AHoldoverGameState, OnTimeDepartures);
	DOREPLIFETIME(AHoldoverGameState, LateDepartures);
	DOREPLIFETIME(AHoldoverGameState, Money);
	DOREPLIFETIME(AHoldoverGameState, Deductions);
	DOREPLIFETIME(AHoldoverGameState, bShiftPassed);
	DOREPLIFETIME(AHoldoverGameState, DispatchFeed);
	// G-5: late-join correctness — duration and pay rules must match the host.
	DOREPLIFETIME(AHoldoverGameState, LastShiftDuration);
	DOREPLIFETIME(AHoldoverGameState, PayPerOnTimeDeparture);
	DOREPLIFETIME(AHoldoverGameState, LateDepartureDeduction);
	// Sonnet H1: these are UPROPERTY(ReplicatedUsing) and were never registered,
	// so clients kept the defaults (SHIFT 1 / ledger $0 forever).
	DOREPLIFETIME(AHoldoverGameState, ShiftNumber);
	DOREPLIFETIME(AHoldoverGameState, TotalCompanyPay);
	DOREPLIFETIME(AHoldoverGameState, TotalCompanyDeductions);
	DOREPLIFETIME(AHoldoverGameState, CompanyLedger);
}

void AHoldoverGameState::OnRep_Shift()
{
	// HUD reads the replicated values directly.
}

bool AHoldoverGameState::ShouldPlayDispatchBeep()
{
	// M14: the initial join replicate carries the whole feed - that is not
	// news. After that, beep only when the feed grew.
	const int32 NewLen = DispatchFeed.Num();
	if (!bSeenDispatchFeed)
	{
		bSeenDispatchFeed = true;
		LastSeenDispatchLen = NewLen;
		return false;
	}
	const bool bGrew = NewLen > LastSeenDispatchLen;
	LastSeenDispatchLen = NewLen;
	return bGrew;
}

void AHoldoverGameState::OnRep_Dispatch()
{
	// Radio beep on clients for each replicated dispatch update (M14: not on
	// the initial join replicate, and only when the feed grew).
	if (GetWorld() && !HasAuthority() && ShouldPlayDispatchBeep())
	{
		UHoldoverAudioLibrary::Play2D(this, FName("radio_beep"), 0.6f);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameState.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverPlayerState.h"
#include "HoldoverShiftData.h"
#include "Net/UnrealNetwork.h"

AHoldoverGameState::AHoldoverGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

// --- DES-5: shift tuning accessors ---

float AHoldoverGameState::GetHoldoverDuration() const
{
	return ShiftData ? ShiftData->HoldoverDuration : 120.f;
}

float AHoldoverGameState::GetWeatherBurnMultiplier() const
{
	return ShiftData ? ShiftData->WeatherBurnMultiplier : 2.f;
}

float AHoldoverGameState::GetProtectionDegradationPerSecond() const
{
	return ShiftData ? ShiftData->ProtectionDegradationPerSecond : 2.f;
}

float AHoldoverGameState::GetHoldoverDegradationScale() const
{
	return ShiftData ? ShiftData->HoldoverDegradationScale : 0.f;
}

float AHoldoverGameState::GetReContaminationRatePerSecond() const
{
	return ShiftData ? ShiftData->ReContaminationRatePerSecond : 3.f;
}

float AHoldoverGameState::GetReContaminationProtectionThreshold() const
{
	return ShiftData ? ShiftData->ReContaminationProtectionThreshold : 50.f;
}

int32 AHoldoverGameState::GetHoldoverAbortPenalty() const
{
	return ShiftData ? ShiftData->HoldoverAbortPenalty : 15;
}

float AHoldoverGameState::GetPerAircraftDeadlineSeconds() const
{
	return ShiftData ? ShiftData->PerAircraftDeadlineSeconds : 300.f;
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
	LastShiftDuration = FMath::Max(0.f, DurationSeconds);
	ShiftState = EShiftState::InProgress;
	ShiftTimeRemaining = LastShiftDuration;
	Quota = FMath::Max(1, InQuota);
	OnTimeDepartures = 0;
	LateDepartures = 0;
	Money = 0;
	Deductions = 0;
	bShiftPassed = false;

	// SIM-1: randomise weather for this shift (squalls and lulls, not a monotonic ramp).
	if (ShiftData)
	{
		WeatherBaseSeverity = FMath::FRandRange(ShiftData->WeatherSeverityMin, ShiftData->WeatherSeverityMax);
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
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AController* C = It->Get())
		{
			if (AHoldoverPlayerState* PS = C->GetPlayerState<AHoldoverPlayerState>())
			{
				PS->ResetForNewShift();
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
	DispatchEvent(EDispatchEvent::NewShift);
	StartShift(LastShiftDuration, Quota);

	// DES-1: reset all aircraft for the fresh shift.
	for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
	{
		It->ResetForNewShift();
	}
}

void AHoldoverGameState::RegisterDeparture(bool bOnTime, AHoldoverPlayerState* ResponsiblePlayer)
{
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
	if (Amount <= 0)
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
			const float Period = ShiftData ? ShiftData->WeatherPeriodSeconds : 90.f;
			const float Amplitude = ShiftData ? ShiftData->WeatherAmplitude : 0.2f;
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
}

void AHoldoverGameState::OnRep_Shift()
{
	// HUD reads the replicated values directly.
}

void AHoldoverGameState::OnRep_Dispatch()
{
	// Radio beep on clients for each replicated dispatch update.
	if (GetWorld() && !HasAuthority())
	{
		UHoldoverAudioLibrary::Play2D(this, FName("radio_beep"), 0.6f);
	}
}

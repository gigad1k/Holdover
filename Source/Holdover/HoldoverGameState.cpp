// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameState.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverPlayerState.h"
#include "Net/UnrealNetwork.h"

AHoldoverGameState::AHoldoverGameState()
{
	PrimaryActorTick.bCanEverTick = true;
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

	AddDispatch(FString::Printf(TEXT("Shift start. Quota: %d on-time departures."), Quota));
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
	bShiftPassed = OnTimeDepartures >= Quota;
	AddDispatch(bShiftPassed
		? FString::Printf(TEXT("SHIFT PASSED. %d/%d on-time. Pay $%d, deductions $%d."), OnTimeDepartures, Quota, Money, Deductions)
		: FString::Printf(TEXT("RESTRUCTURED. %d/%d on-time. Pay $%d, deductions $%d."), OnTimeDepartures, Quota, Money, Deductions));
}

void AHoldoverGameState::RestartShift()
{
	AddDispatch(TEXT("New shift."));
	StartShift(LastShiftDuration, Quota);

	// DES-1: reset all aircraft for the fresh shift.
	for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
	{
		It->ResetForNewShift();
	}
}

void AHoldoverGameState::RegisterDeparture(bool bOnTime, AHoldoverPlayerState* ResponsiblePlayer)
{
	const FString Who = ResponsiblePlayer ? ResponsiblePlayer->GetShortName() + TEXT(" ") : TEXT("");
	if (bOnTime)
	{
		OnTimeDepartures++;
		Money += PayPerOnTimeDeparture;
		AddDispatch(FString::Printf(TEXT("%sdeparture cleared. +$%d."), *Who, PayPerOnTimeDeparture));
	}
	else
	{
		LateDepartures++;
		Deductions += LateDepartureDeduction;
		AddDispatch(FString::Printf(TEXT("%sLATE departure. -$%d."), *Who, LateDepartureDeduction));
	}
}

void AHoldoverGameState::RegisterPenalty(int32 Amount, const FString& Reason, AHoldoverPlayerState* ResponsiblePlayer)
{
	if (Amount <= 0)
	{
		return;
	}
	Deductions += Amount;
	const FString Who = ResponsiblePlayer ? ResponsiblePlayer->GetShortName() + TEXT(" ") : TEXT("");
	AddDispatch(FString::Printf(TEXT("%s%s -$%d"), *Who, *Reason, Amount));
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

void AHoldoverGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (bAutoRampWeather)
		{
			WeatherSeverity = FMath::Clamp(WeatherSeverity + WeatherRampPerSecond * DeltaSeconds, 0.f, 1.f);
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverGameState.h"
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

void AHoldoverGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && bAutoRampWeather)
	{
		WeatherSeverity = FMath::Clamp(WeatherSeverity + WeatherRampPerSecond * DeltaSeconds, 0.f, 1.f);
	}
}

void AHoldoverGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverGameState, WeatherSeverity);
}

void AHoldoverGameState::OnRep_Weather()
{
	// HUD reads WeatherSeverity directly.
}

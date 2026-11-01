// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverPlayerState.h"
#include "Net/UnrealNetwork.h"

AHoldoverPlayerState::AHoldoverPlayerState()
{
}

void AHoldoverPlayerState::AddEarnings(int32 Amount)
{
	if (HasAuthority() && Amount > 0)
	{
		Earnings += Amount;
	}
}

void AHoldoverPlayerState::AddPenalty(int32 Amount)
{
	if (HasAuthority() && Amount > 0)
	{
		Penalties += Amount;
	}
}

void AHoldoverPlayerState::RecordDeparture()
{
	if (HasAuthority())
	{
		DeparturesHandled++;
	}
}

void AHoldoverPlayerState::SetCarriedFluid(EHoldoverFluidType NewType)
{
	if (HasAuthority())
	{
		CarriedFluidType = NewType;
	}
}

void AHoldoverPlayerState::ResetForNewShift()
{
	if (HasAuthority())
	{
		Earnings = 0;
		Penalties = 0;
		DeparturesHandled = 0;
	}
}

FString AHoldoverPlayerState::GetShortName() const
{
	const FString Name = GetPlayerName();
	return Name.IsEmpty() ? TEXT("Crew") : Name;
}

void AHoldoverPlayerState::OnRep_Stats()
{
	// HUD reads the replicated values directly.
}

void AHoldoverPlayerState::OnRep_FluidChoice()
{
	// HUD reads the replicated value directly.
}

void AHoldoverPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverPlayerState, Earnings);
	DOREPLIFETIME(AHoldoverPlayerState, Penalties);
	DOREPLIFETIME(AHoldoverPlayerState, DeparturesHandled);
	DOREPLIFETIME(AHoldoverPlayerState, CarriedFluidType);
}

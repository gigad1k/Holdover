// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverAircraft.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Holdover.h"
#include "HoldoverGameState.h"
#include "HoldoverSurfaceSection.h"
#include "Net/UnrealNetwork.h"

AHoldoverAircraft::AHoldoverAircraft()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	// Scene root so the actor can hold a transform (required for spawn placement).
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FText AHoldoverAircraft::GetInteractionPrompt_Implementation() const
{
	if (bDeparted)
	{
		return FText::FromString(TEXT("Departed"));
	}
	switch (HoldoverState)
	{
	case EHoldoverState::HoldoverActive:
		return FText::FromString(TEXT("Press E - Request Clearance"));
	case EHoldoverState::HoldoverExpired:
		return FText::FromString(TEXT("Holdover expired - retreat before departure"));
	default:
		return FText::FromString(TEXT("Treatment incomplete"));
	}
}

void AHoldoverAircraft::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	const bool bOnTime = GS && GS->ShiftState == EShiftState::InProgress && GS->ShiftTimeRemaining > 0.f;

	if (TryDepart())
	{
		if (GS)
		{
			GS->RegisterDeparture(bOnTime);
		}
		UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s' departed (%s)"), *GetName(), bOnTime ? TEXT("on-time") : TEXT("late"));
	}
}

bool AHoldoverAircraft::TryDepart()
{
	if (bDeparted || !IsClearedForTakeoff())
	{
		return false;
	}
	bDeparted = true;
	HoldoverState = EHoldoverState::Departed;
	return true;
}

void AHoldoverAircraft::ResetForNewShift()
{
	bDeparted = false;
	HoldoverState = EHoldoverState::Incomplete;
	HoldoverRemaining = 0.f;
	SetActorLocation(SpawnLocation);
	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Section->CleanlinessPercent = 0.f;
			Section->ProtectionPercent = 0.f;
		}
	}
}

void AHoldoverAircraft::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();

	// Gather sections that were not registered explicitly (greybox test setups).
	for (TActorIterator<AHoldoverSurfaceSection> It(GetWorld()); It; ++It)
	{
		AHoldoverSurfaceSection* Section = *It;
		if (Section && FVector::DistSquared(Section->GetActorLocation(), GetActorLocation()) <= FMath::Square(SectionGatherRadius))
		{
			RegisterSection(Section);
		}
	}

	UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s' registered %d surface sections"), *GetName(), Sections.Num());
}

void AHoldoverAircraft::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && bDeparted)
	{
		// Greybox departure cue: the aircraft climbs away.
		AddActorWorldOffset(FVector(0.f, 0.f, DepartSpeed * DeltaSeconds));
		return;
	}

	if (!HasAuthority())
	{
		return;
	}

	float Weather = 0.f;
	if (const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
	{
		Weather = GS->WeatherSeverity;
	}
	SimulateTreatment(DeltaSeconds, Weather);
}

void AHoldoverAircraft::RegisterSection(AHoldoverSurfaceSection* Section)
{
	if (Section && !Sections.Contains(Section))
	{
		Sections.Add(Section);
	}
}

float AHoldoverAircraft::GetOverallCleanliness() const
{
	if (Sections.Num() == 0)
	{
		return 0.f;
	}
	float Total = 0.f;
	for (const AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Total += Section->CleanlinessPercent;
		}
	}
	return Total / Sections.Num();
}

float AHoldoverAircraft::GetOverallProtection() const
{
	if (Sections.Num() == 0)
	{
		return 0.f;
	}
	float Total = 0.f;
	for (const AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Total += Section->ProtectionPercent;
		}
	}
	return Total / Sections.Num();
}

bool AHoldoverAircraft::AreAllSectionsClean() const
{
	if (Sections.Num() == 0)
	{
		return false;
	}
	for (const AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section && !Section->IsClean())
		{
			return false;
		}
	}
	return true;
}

bool AHoldoverAircraft::AreAllSectionsCleanAndProtected() const
{
	if (Sections.Num() == 0)
	{
		return false;
	}
	for (const AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section && (!Section->IsClean() || !Section->IsProtected()))
		{
			return false;
		}
	}
	return true;
}

void AHoldoverAircraft::SimulateTreatment(float DeltaSeconds, float WeatherSeverity)
{
	if (DeltaSeconds <= 0.f || Sections.Num() == 0)
	{
		return;
	}
	const float Weather = FMath::Clamp(WeatherSeverity, 0.f, 1.f);

	// 1. Weather degrades protection on every section.
	const float Degradation = ProtectionDegradationPerSecond * Weather * DeltaSeconds;
	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Section->ApplyDegradation(Degradation);
		}
	}

	// 2. Holdover start: only when every section passes clean+protected thresholds.
	bool bJustStarted = false;
	if (HoldoverState == EHoldoverState::Incomplete && AreAllSectionsCleanAndProtected())
	{
		HoldoverState = EHoldoverState::HoldoverActive;
		HoldoverRemaining = HoldoverDuration;
		bJustStarted = true;
		UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s': holdover started (%.0fs)"), *GetName(), HoldoverRemaining);
		if (AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr)
		{
			GS->AddDispatch(TEXT("Aircraft treated. Holdover running - clear it for departure!"));
		}
	}

	// 3. Holdover burn / invalidation (not on the starting tick).
	if (!bJustStarted && HoldoverState == EHoldoverState::HoldoverActive)
	{
		if (!AreAllSectionsCleanAndProtected())
		{
			// Degradation forced retreatment mid-holdover.
			HoldoverState = EHoldoverState::Incomplete;
			HoldoverRemaining = 0.f;
			UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s': holdover aborted (treatment lost)"), *GetName());
		}
		else
		{
			HoldoverRemaining -= DeltaSeconds * (1.f + Weather * WeatherBurnMultiplier);
			if (HoldoverRemaining <= 0.f)
			{
				HoldoverRemaining = 0.f;
				HoldoverState = EHoldoverState::HoldoverExpired;
				// Expired protection is gone: retreatment required.
				for (AHoldoverSurfaceSection* Section : Sections)
				{
					if (Section)
					{
						Section->ApplyDegradation(100.f);
					}
				}
				UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s': holdover EXPIRED — retreatment required"), *GetName());
				if (AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr)
				{
					GS->AddDispatch(TEXT("HOLDOVER EXPIRED! Protection is gone - retreat the aircraft."));
				}
			}
		}
	}
	else if (HoldoverState == EHoldoverState::HoldoverExpired && AreAllSectionsCleanAndProtected())
	{
		// Valid retreatment after expiry: restart the clock.
		HoldoverState = EHoldoverState::HoldoverActive;
		HoldoverRemaining = HoldoverDuration;
		UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s': holdover restarted after retreatment"), *GetName());
	}
}

void AHoldoverAircraft::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverAircraft, HoldoverState);
	DOREPLIFETIME(AHoldoverAircraft, HoldoverRemaining);
	DOREPLIFETIME(AHoldoverAircraft, bDeparted);
}

void AHoldoverAircraft::OnRep_Holdover()
{
	// HUD reads the replicated values directly.
}

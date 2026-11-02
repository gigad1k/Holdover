// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverAircraft.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Holdover.h"
#include "HoldoverGameState.h"
#include "HoldoverPlayerState.h"
#include "HoldoverSurfaceSection.h"
#include "Net/UnrealNetwork.h"

AHoldoverAircraft::AHoldoverAircraft()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// MP-5: clients must see the departure climb-out and shift-reset jumps.
	SetReplicatingMovement(true);

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

	// SIM-4: per-aircraft deadline — late if past the deadline OR the shift is over.
	const bool bOnTime = GS
		&& GS->ShiftState == EShiftState::InProgress
		&& !IsDepartureLate();

	if (TryDepart())
	{
		// DES-2: attribute the departure to the player who requested it.
		AHoldoverPlayerState* PS = nullptr;
		if (const APawn* Pawn = Cast<APawn>(Interactor))
		{
			if (const AController* C = Pawn->GetController())
			{
				PS = C->GetPlayerState<AHoldoverPlayerState>();
			}
		}
		if (PS)
		{
			PS->RecordDeparture();
			if (bOnTime)
			{
				PS->AddEarnings(GS ? GS->PayPerOnTimeDeparture : 100);
			}
			else
			{
				PS->AddPenalty(GS ? GS->LateDepartureDeduction : 50);
			}
		}
		if (GS)
		{
			GS->RegisterDeparture(bOnTime, PS);
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
	SyncArrivalVisuals();
	return true;
}

void AHoldoverAircraft::ResetForNewShift()
{
	bDeparted = false;
	bArrived = ArrivalDelay <= 0.f;
	ArrivalTime = bArrived ? (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) : -1.f;
	HoldoverState = EHoldoverState::Incomplete;
	HoldoverRemaining = 0.f;
	SetActorLocation(SpawnLocation);
	SyncArrivalVisuals();
	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Section->CleanlinessPercent = 0.f;
			Section->ProtectionPercent = 0.f;
		}
	}
}

bool AHoldoverAircraft::IsDepartureLate() const
{
	if (ArrivalTime < 0.f || !GetWorld())
	{
		return false;
	}
	const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>();
	const float Deadline = GS ? GS->GetPerAircraftDeadlineSeconds() : 300.f;
	return GetWorld()->GetTimeSeconds() > ArrivalTime + Deadline;
}

void AHoldoverAircraft::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();

	// H2: only the authority initialises the replicated arrival flag. Clients
	// must not write it (doing so raced the initial replication and left late
	// joiners with a permanently hidden aircraft); they re-derive visuals from
	// the replicated value instead.
	if (HasAuthority())
	{
		bArrived = ArrivalDelay <= 0.f;
	}
	SyncArrivalVisuals();

	// SIM-5: radius-gather only claims sections with no explicit owner.
	for (TActorIterator<AHoldoverSurfaceSection> It(GetWorld()); It; ++It)
	{
		AHoldoverSurfaceSection* Section = *It;
		if (Section
			&& Section->OwningAircraft == nullptr
			&& FVector::DistSquared(Section->GetActorLocation(), GetActorLocation()) <= FMath::Square(SectionGatherRadius))
		{
			RegisterSection(Section);
		}
	}

	UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s' registered %d surface sections (arrival delay %.1fs)"),
		*GetName(), Sections.Num(), ArrivalDelay);
}

void AHoldoverAircraft::SyncArrivalVisuals()
{
	// Aircraft visibility follows arrival; a departed plane stays visible so its
	// climb-out cue can play. Sonnet M17: the sections are separate actors that
	// do not follow the aircraft — hide them together with arrival/departure so
	// nothing is left floating, visible and sprayable behind.
	SetActorHiddenInGame(!bArrived);
	SetActorEnableCollision(bArrived);
	const bool bShowSections = bArrived && !bDeparted;
	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Section->SetActorHiddenInGame(!bShowSections);
			Section->SetActorEnableCollision(bShowSections);
		}
	}
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

	// DES-1: staggered arrival — the plane appears once the shift clock passes its delay.
	if (!bArrived)
	{
		const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>();
		const float Elapsed = GS ? (GS->GetShiftDuration() - GS->ShiftTimeRemaining) : 0.f;
		if (Elapsed >= ArrivalDelay)
		{
			bArrived = true;
			SyncArrivalVisuals();
			ArrivalTime = GetWorld()->GetTimeSeconds();
			UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s' has ARRIVED"), *GetName());
			if (AHoldoverGameState* GSW = GetWorld()->GetGameState<AHoldoverGameState>())
			{
				GSW->DispatchEvent(EDispatchEvent::AircraftArrival);
			}
		}
		return;
	}

	float Weather = 0.f;
	if (const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
	{
		Weather = GS->WeatherSeverity;

		// DES-5: sync tuning from the shift data asset (local defaults as fallback).
		HoldoverDuration = GS->GetHoldoverDuration();
		WeatherBurnMultiplier = GS->GetWeatherBurnMultiplier();
		ProtectionDegradationPerSecond = GS->GetProtectionDegradationPerSecond();
		HoldoverDegradationScale = GS->GetHoldoverDegradationScale();
		ReContaminationRatePerSecond = GS->GetReContaminationRatePerSecond();
		ReContaminationProtectionThreshold = GS->GetReContaminationProtectionThreshold();
	}
	SimulateTreatment(DeltaSeconds, Weather);
}

void AHoldoverAircraft::RegisterSection(AHoldoverSurfaceSection* Section)
{
	if (!Section || Sections.Contains(Section))
	{
		return;
	}
	// SIM-5: never steal a section explicitly owned by another aircraft.
	if (Section->OwningAircraft != nullptr && Section->OwningAircraft != this)
	{
		return;
	}
	Sections.Add(Section);
	Section->OwningAircraft = this;
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

	// SIM-2: degradation is paused (or slowed) during an active holdover so
	// expiry is the clock players actually fight, not the abort.
	const bool bInHoldover = (HoldoverState == EHoldoverState::HoldoverActive);
	const float DegScale = bInHoldover ? HoldoverDegradationScale : 1.f;
	const float Degradation = ProtectionDegradationPerSecond * Weather * DegScale * DeltaSeconds;
	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			Section->ApplyDegradation(Degradation);
		}
	}

	// SIM-3: precipitation re-contaminates unprotected sections.
	if (ReContaminationRatePerSecond > 0.f && Weather > 0.f)
	{
		const float Contamination = ReContaminationRatePerSecond * Weather * DeltaSeconds;
		for (AHoldoverSurfaceSection* Section : Sections)
		{
			if (Section && Section->ProtectionPercent < ReContaminationProtectionThreshold)
			{
				Section->ApplyContamination(Contamination);
			}
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
			GS->DispatchEvent(EDispatchEvent::HoldoverStarted);
		}
	}

	// 3. Holdover burn / invalidation (not on the starting tick).
	if (!bJustStarted && HoldoverState == EHoldoverState::HoldoverActive)
	{
		if (!AreAllSectionsCleanAndProtected())
		{
			// SIM-2: degradation forced retreatment mid-holdover — abort costs something.
			HoldoverState = EHoldoverState::Incomplete;
			HoldoverRemaining = 0.f;
			UE_LOG(LogHoldover, Display, TEXT("Aircraft '%s': holdover aborted (treatment lost)"), *GetName());
			if (AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr)
			{
				GS->RegisterPenalty(GS->GetHoldoverAbortPenalty(), TEXT("Holdover aborted"));
			}
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
					GS->DispatchEvent(EDispatchEvent::HoldoverExpired);
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
	DOREPLIFETIME(AHoldoverAircraft, bArrived);
}

void AHoldoverAircraft::OnRep_Holdover()
{
	// HUD reads the replicated values directly.
}

void AHoldoverAircraft::OnRep_Arrived()
{
	SyncArrivalVisuals();
}

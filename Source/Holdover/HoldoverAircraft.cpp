// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverAircraft.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Holdover.h"
#include "HoldoverSurfaceSection.h"

AHoldoverAircraft::AHoldoverAircraft()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	// Scene root so the actor can hold a transform (required for spawn placement).
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AHoldoverAircraft::BeginPlay()
{
	Super::BeginPlay();

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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverSurfaceSection.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AHoldoverSurfaceSection::AHoldoverSurfaceSection()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	bReplicates = true;
	SetReplicatingMovement(false);
}

AHoldoverAircraft* AHoldoverSurfaceSection::FindOwningAircraft() const
{
	AHoldoverAircraft* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
	{
		const float DistSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

FText AHoldoverSurfaceSection::GetInteractionPrompt_Implementation() const
{
	AHoldoverAircraft* Aircraft = FindOwningAircraft();
	return Aircraft
		? IHoldoverInteractableInterface::Execute_GetInteractionPrompt(Aircraft)
		: FText::GetEmpty();
}

void AHoldoverSurfaceSection::Interact_Implementation(AActor* Interactor)
{
	if (AHoldoverAircraft* Aircraft = FindOwningAircraft())
	{
		IHoldoverInteractableInterface::Execute_Interact(Aircraft, Interactor);
	}
}

float AHoldoverSurfaceSection::ApplyCleaning(float Amount)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = CleanlinessPercent;
	CleanlinessPercent = FMath::Clamp(CleanlinessPercent + Amount, 0.f, 100.f);
	return CleanlinessPercent - Before;
}

float AHoldoverSurfaceSection::ApplyProtection(float Amount)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = ProtectionPercent;
	ProtectionPercent = FMath::Clamp(ProtectionPercent + Amount, 0.f, 100.f);
	return ProtectionPercent - Before;
}

float AHoldoverSurfaceSection::ApplyDegradation(float Amount)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = ProtectionPercent;
	ProtectionPercent = FMath::Clamp(ProtectionPercent - Amount, 0.f, 100.f);
	return Before - ProtectionPercent;
}

void AHoldoverSurfaceSection::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverSurfaceSection, CleanlinessPercent);
	DOREPLIFETIME(AHoldoverSurfaceSection, ProtectionPercent);
}

void AHoldoverSurfaceSection::OnRep_Treatment()
{
	// UI reads the replicated values directly; hook kept for future presentation.
}

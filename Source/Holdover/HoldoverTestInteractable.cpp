// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverTestInteractable.h"
#include "Components/StaticMeshComponent.h"
#include "Holdover.h"
#include "HoldoverCharacter.h"
#include "HoldoverSprayComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHoldoverTestInteractable::AHoldoverTestInteractable()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

FText AHoldoverTestInteractable::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(TEXT("Press E - Test Interact"));
}

void AHoldoverTestInteractable::Interact_Implementation(AActor* Interactor)
{
	UE_LOG(LogHoldover, Display, TEXT("TestInteractable '%s' interacted by '%s'"), *GetName(),
		Interactor ? *Interactor->GetName() : TEXT("null"));

	// Greybox behaviour: refills the interactor's Type I tank (Phase 4 turns this into a real refill station).
	if (AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Interactor))
	{
		if (UHoldoverSprayComponent* Spray = Character->GetSprayComponent())
		{
			Spray->RefillTypeI();
		}
	}

	if (!DynamicMaterial)
	{
		DynamicMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	// Flip between two obvious colors so the effect is visible without any game art.
	static bool bToggle = false;
	bToggle = !bToggle;
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"),
			bToggle ? FLinearColor::Green : FLinearColor(1.f, 0.5f, 0.f));
	}
}

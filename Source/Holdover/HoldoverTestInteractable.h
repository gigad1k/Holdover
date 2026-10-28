// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverInteractableInterface.h"
#include "HoldoverTestInteractable.generated.h"

class UStaticMeshComponent;

/** Greybox test object implementing the shared interaction pattern. */
UCLASS()
class HOLDOVER_API AHoldoverTestInteractable : public AActor, public IHoldoverInteractableInterface
{
	GENERATED_BODY()

public:
	AHoldoverTestInteractable();

	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* Mesh;

	/** Toggled on each interaction so the effect is visible in PIE. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};

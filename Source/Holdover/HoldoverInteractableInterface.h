// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HoldoverInteractableInterface.generated.h"

UINTERFACE(BlueprintType)
class UHoldoverInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class HOLDOVER_API IHoldoverInteractableInterface
{
	GENERATED_BODY()

public:
	/** Short prompt shown while the player looks at this actor, e.g. "Press E - Refill" */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt() const;

	/** Gameplay effect. Runs server-side (see AHoldoverCharacter::ServerInteract). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
};

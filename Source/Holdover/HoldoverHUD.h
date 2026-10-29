// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HoldoverHUD.generated.h"

class UHoldoverHUDWidget;

/**
 * Owns the ramp-ops UMG HUD for this client. All presentation lives in
 * UHoldoverHUDWidget; this class only spawns it and relays input toggles.
 */
UCLASS()
class HOLDOVER_API AHoldoverHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Client-side cosmetic prompt, updated by the locally controlled character/truck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionPrompt;

	/** Optional BP subclass of the ramp-ops widget; defaults to the native class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UHoldoverHUDWidget> HUDWidgetClass;

	/** Tab relay: opens/closes the fullscreen departure board. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleDepartureBoard();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UHoldoverHUDWidget> MainWidget;
};

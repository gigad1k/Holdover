// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HoldoverHUD.generated.h"

class AHoldoverSurfaceSection;
class UHoldoverHUDWidget;

/** Greybox HUD: canvas reticle/prompt + a UMG panel set (see UHoldoverHUDWidget). */
UCLASS()
class HOLDOVER_API AHoldoverHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Client-side cosmetic prompt, updated by the locally controlled character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionPrompt;

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

protected:
	UPROPERTY(Transient)
	UHoldoverHUDWidget* HUDWidget;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverInteractableInterface.h"
#include "HoldoverSurfaceSection.generated.h"

class UStaticMeshComponent;
class AHoldoverAircraft;

/**
 * One independently trackable aircraft surface panel.
 * Treatment state is server-authoritative and replicated.
 */
UCLASS()
class HOLDOVER_API AHoldoverSurfaceSection : public AActor, public IHoldoverInteractableInterface
{
	GENERATED_BODY()

public:
	AHoldoverSurfaceSection();

	// The aircraft has no traceable collision, so sections forward the departure
	// interact to their nearest aircraft (MP-5 end-to-end path).
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	AHoldoverAircraft* FindOwningAircraft() const;

	/** e.g. "Nose", "LeftWing" — labels are the source of truth in greybox. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Treatment")
	FName SectionName;

	/** 0-100. Type I spray raises this. */
	UPROPERTY(ReplicatedUsing = OnRep_Treatment, EditAnywhere, BlueprintReadOnly, Category = "Treatment")
	float CleanlinessPercent = 0.f;

	/** 0-100. Type IV protection (Phase 4). */
	UPROPERTY(ReplicatedUsing = OnRep_Treatment, EditAnywhere, BlueprintReadOnly, Category = "Treatment")
	float ProtectionPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Treatment")
	float CleanThreshold = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Treatment")
	float ProtectionThreshold = 95.f;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	bool IsClean() const { return CleanlinessPercent >= CleanThreshold; }

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	bool IsProtected() const { return ProtectionPercent >= ProtectionThreshold; }

	/** Server-side: raise cleanliness. Returns the amount actually applied. */
	float ApplyCleaning(float Amount);

	/** Server-side: raise protection. Returns the amount actually applied. */
	float ApplyProtection(float Amount);

	/** Server-side: weather-driven protection loss. Returns the amount actually lost. */
	float ApplyDegradation(float Amount);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* Mesh;

	UFUNCTION()
	void OnRep_Treatment();
};

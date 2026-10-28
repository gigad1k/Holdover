// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverAircraft.generated.h"

class AHoldoverSurfaceSection;

/**
 * Test aircraft: gathers nearby surface sections and exposes treatment totals.
 * Sections are independent replicated actors; this aggregates them.
 */
UCLASS()
class HOLDOVER_API AHoldoverAircraft : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverAircraft();

	/** Sections within this radius register with this aircraft on BeginPlay. */
	UPROPERTY(EditAnywhere, Category = "Treatment")
	float SectionGatherRadius = 1500.f;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	const TArray<AHoldoverSurfaceSection*>& GetSections() const { return Sections; }

	/** Average cleanliness across all sections (0-100). */
	UFUNCTION(BlueprintCallable, Category = "Treatment")
	float GetOverallCleanliness() const;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	bool AreAllSectionsClean() const;

	/** Registers a section (called by sections on BeginPlay). */
	void RegisterSection(AHoldoverSurfaceSection* Section);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TArray<AHoldoverSurfaceSection*> Sections;
};

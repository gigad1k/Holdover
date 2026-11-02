// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HoldoverFluidData.generated.h"

/**
 * DES-5: spray / tank tuning as a data asset. Replaces the scattered
 * EditDefaultsOnly constants on UHoldoverSprayComponent. Create one per
 * fluid balance profile.
 */
UCLASS(BlueprintType)
class HOLDOVER_API UHoldoverFluidData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Maximum fluid units in a full tank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float FluidCapacity = 100.f;

	/** Type I cleanliness units per second of spray. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float CleanRatePerSecond = 25.f;

	/** Type IV protection units per second of spray. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float ProtectRatePerSecond = 15.f;

	/** Type I tank units consumed per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float TypeIUsePerSecond = 8.f;

	/** Type IV tank units consumed per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float TypeIVUsePerSecond = 4.f;

	/** Seconds to refill at a station (DES-2 timed refills). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float RefillDuration = 3.f;

	/** Maximum spray trace distance (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float SprayRange = 600.f;

	/** Client->server spray request cadence (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float SprayRequestInterval = 0.1f;

	/** Cleanliness % at or above which a section counts as clean. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float SectionCleanThreshold = 95.f;

	/** Protection % at or above which a section counts as protected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fluid")
	float SectionProtectThreshold = 95.f;
};

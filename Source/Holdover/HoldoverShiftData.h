// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HoldoverShiftData.generated.h"

/**
 * DES-5: shift / weather / holdover / pay tuning as a data asset. Replaces the
 * scattered EditDefaultsOnly constants across GameMode, GameState, Aircraft and
 * HazardVolume. Create one per shift profile (night 1 easy -> night 5 blizzard).
 */
UCLASS(BlueprintType)
class HOLDOVER_API UHoldoverShiftData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- shift spec ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shift")
	float ShiftDuration = 480.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shift")
	int32 ShiftQuota = 2;

	// --- pay ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pay")
	int32 PayPerOnTimeDeparture = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pay")
	int32 LateDepartureDeduction = 50;

	/** SIM-2: penalty charged when an active holdover aborts (protection drops). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pay")
	int32 HoldoverAbortPenalty = 15;

	// --- weather (SIM-1: squalls and lulls, randomised per shift) ---
	/** Base severity range randomised per shift. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float WeatherSeverityMin = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float WeatherSeverityMax = 0.5f;

	/** Amplitude of the squall/lull oscillation around the base. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float WeatherAmplitude = 0.2f;

	/** Seconds per full squall/lull cycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "10"))
	float WeatherPeriodSeconds = 90.f;

	// --- holdover ---
	/** Holdover duration at zero weather (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Holdover")
	float HoldoverDuration = 120.f;

	/** Extra burn multiplier at weather severity 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Holdover")
	float WeatherBurnMultiplier = 2.f;

	/** Protection % lost per second at weather severity 1 (when not in holdover). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Holdover")
	float ProtectionDegradationPerSecond = 2.f;

	/**
	 * SIM-2: degradation scale during an active holdover. 0 = fully paused
	 * (expiry is the clock players fight); 0.5 = half speed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Holdover", meta = (ClampMin = "0", ClampMax = "1"))
	float HoldoverDegradationScale = 0.f;

	// --- contamination (SIM-3) ---
	/** Cleanliness % lost per second at weather severity 1 on unprotected sections. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contamination")
	float ReContaminationRatePerSecond = 3.f;

	/** Sections at or above this protection % are immune to re-contamination. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contamination", meta = (ClampMin = "0", ClampMax = "100"))
	float ReContaminationProtectionThreshold = 50.f;

	// --- per-aircraft deadlines (SIM-4) ---
	/** Seconds after arrival before an aircraft's departure counts as late. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Departure")
	float PerAircraftDeadlineSeconds = 300.f;
};

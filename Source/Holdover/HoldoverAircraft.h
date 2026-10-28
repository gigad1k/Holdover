// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverAircraft.generated.h"

class AHoldoverSurfaceSection;

UENUM(BlueprintType)
enum class EHoldoverState : uint8
{
	Incomplete		UMETA(DisplayName = "Incomplete"),
	HoldoverActive	UMETA(DisplayName = "Holdover Active"),
	HoldoverExpired	UMETA(DisplayName = "Holdover Expired")
};

/**
 * Test aircraft: gathers nearby surface sections and runs the treatment /
 * holdover state machine. All rules are deterministic and server-authoritative;
 * SimulateTreatment(dt, weather) is the single update path (also used by tests).
 */
UCLASS()
class HOLDOVER_API AHoldoverAircraft : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverAircraft();

	// --- tuning (editable defaults; data-asset-ise later) ---
	UPROPERTY(EditAnywhere, Category = "Holdover")
	float SectionGatherRadius = 1500.f;

	/** Holdover duration at zero weather (seconds). */
	UPROPERTY(EditAnywhere, Category = "Holdover")
	float HoldoverDuration = 120.f;

	/** Extra burn multiplier at weather severity 1. */
	UPROPERTY(EditAnywhere, Category = "Holdover")
	float WeatherBurnMultiplier = 2.f;

	/** Protection % lost per second at weather severity 1. */
	UPROPERTY(EditAnywhere, Category = "Holdover")
	float ProtectionDegradationPerSecond = 2.f;

	// --- state (server writes; clients read) ---
	UPROPERTY(ReplicatedUsing = OnRep_Holdover, BlueprintReadOnly, Category = "Holdover")
	EHoldoverState HoldoverState = EHoldoverState::Incomplete;

	UPROPERTY(ReplicatedUsing = OnRep_Holdover, BlueprintReadOnly, Category = "Holdover")
	float HoldoverRemaining = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	const TArray<AHoldoverSurfaceSection*>& GetSections() const { return Sections; }

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	float GetOverallCleanliness() const;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	float GetOverallProtection() const;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	bool AreAllSectionsClean() const;

	UFUNCTION(BlueprintCallable, Category = "Treatment")
	bool AreAllSectionsCleanAndProtected() const;

	/** Departure is only valid while the holdover clock is running. */
	UFUNCTION(BlueprintCallable, Category = "Holdover")
	bool IsClearedForTakeoff() const { return HoldoverState == EHoldoverState::HoldoverActive && HoldoverRemaining > 0.f; }

	/**
	 * Deterministic treatment update: degrades protection with weather, manages
	 * holdover start/burn/expiry. Called by the server tick; tests call directly.
	 */
	void SimulateTreatment(float DeltaSeconds, float WeatherSeverity);

	/** Registers a section (called by sections on BeginPlay). */
	void RegisterSection(AHoldoverSurfaceSection* Section);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Holdover();

	UPROPERTY(Transient)
	TArray<AHoldoverSurfaceSection*> Sections;
};

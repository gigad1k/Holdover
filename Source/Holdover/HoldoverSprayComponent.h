// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverPlayerState.h"
#include "HoldoverSprayComponent.generated.h"

class AHoldoverSurfaceSection;
class USceneComponent;

/**
 * DES-2 one-tank rule: each player carries exactly one fluid type (Type I or
 * Type IV). The choice is made at a refill station; refills are timed, not
 * instant. This forces the clean -> protect relay between players.
 *
 * Server-validated spray: the owning client traces and requests; the server
 * checks range + fluid and applies treatment.
 */
UCLASS(ClassGroup = (Holdover), meta = (BlueprintSpawnableComponent))
class HOLDOVER_API UHoldoverSprayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoldoverSprayComponent();

	// --- tuning (data-asset-ise later per the playbook) ---
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float SprayRange = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float CleanRatePerSecond = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float ProtectRatePerSecond = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float FluidCapacity = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float TypeIUsePerSecond = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float TypeIVUsePerSecond = 4.f;

	/** Client->server request cadence (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float SprayRequestInterval = 0.1f;

	/** DES-2: seconds to refill at a station (not instant). */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float RefillDuration = 3.f;

	/** MP-8: pure dt clamp (world-free, testable) — rejects <=0, caps at 2x interval. */
	static float ClampSprayDt(float RequestedDt, float Interval);

	// --- state (single tank) ---
	UPROPERTY(ReplicatedUsing = OnRep_Fluid, BlueprintReadOnly, Category = "Spray")
	float FluidLevel = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Fluid, BlueprintReadOnly, Category = "Spray")
	EHoldoverFluidType CarriedFluidType = EHoldoverFluidType::TypeI;

	/** Cosmetic client-side target for HUD feedback. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Spray")
	TWeakObjectPtr<AHoldoverSurfaceSection> CurrentTargetSection;

	/** Cosmetic client-side hint when the active fluid is wrong for the target. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Spray")
	FText TargetHint;

	/**
	 * Optional spray origin (e.g. a truck nozzle). When set, traces start here
	 * and aim along this component's forward vector instead of the owner actor.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Spray")
	TWeakObjectPtr<USceneComponent> SprayOrigin;

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void SetSprayOrigin(USceneComponent* Origin) { SprayOrigin = Origin; }

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void SetSpraying(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Spray")
	bool IsSpraying() const { return bSpraying; }

	/**
	 * DES-2: begin a timed refill of the chosen fluid type. Replaces the old
	 * instant RefillTanks. Server-authoritative; called from refill stations.
	 */
	void BeginRefill(EHoldoverFluidType NewType);

	/** True while a timed refill is in progress. */
	bool IsRefilling() const { return bRefilling; }

	/** 0..1 progress of the current refill (for HUD). */
	float GetRefillProgress() const;

	/**
	 * Core treatment rules (server-side; testable without a world).
	 * Type I: raises cleanliness, consumes tank.
	 * Type IV: only on clean sections, raises protection, consumes tank.
	 * Returns true if treatment was applied.
	 */
	bool ApplySpray(AHoldoverSurfaceSection* Target, float DeltaSeconds);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSpray(AHoldoverSurfaceSection* Target, float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void ServerBeginRefill(EHoldoverFluidType NewType);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateTargetFromTrace();

	UFUNCTION()
	void OnRep_Fluid();

	bool bSpraying = false;
	float RequestAccumulator = 0.f;

	// Timed refill state (server-driven, replicated for HUD).
	UPROPERTY(Replicated)
	bool bRefilling = false;

	UPROPERTY(Replicated)
	float RefillTimeRemaining = 0.f;

	UPROPERTY(Transient)
	class UAudioComponent* HissComponent = nullptr;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverFluidData.h"
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

	// --- tuning (DES-5: data asset overrides local defaults when set) ---
	/** DES-5: fluid tuning data asset. When null, the local defaults below are used. */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	TObjectPtr<UHoldoverFluidData> FluidData;

	// Local fallbacks (used when FluidData is null).
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

	/** G-1: server-side proximity to a refill station required to start a refill. */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float RefillStationRange = 1000.f;

	/** Getters that prefer the data asset over local defaults. */
	float GetSprayRange() const { return FluidData ? FluidData->SprayRange : SprayRange; }
	float GetCleanRate() const { return FluidData ? FluidData->CleanRatePerSecond : CleanRatePerSecond; }
	float GetProtectRate() const { return FluidData ? FluidData->ProtectRatePerSecond : ProtectRatePerSecond; }
	float GetFluidCapacity() const { return FluidData ? FluidData->FluidCapacity : FluidCapacity; }
	float GetTypeIUseRate() const { return FluidData ? FluidData->TypeIUsePerSecond : TypeIUsePerSecond; }
	float GetTypeIVUseRate() const { return FluidData ? FluidData->TypeIVUsePerSecond : TypeIVUsePerSecond; }
	float GetRefillDuration() const { return FluidData ? FluidData->RefillDuration : RefillDuration; }

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

	/**
	 * Server-side handler for a refill request (G-1 validation lives here; the
	 * ServerBeginRefill RPC delegates to this). Public so world-based automation
	 * tests can exercise the exact validation path a remote client hits (PROC-1).
	 * Returns true if the refill started.
	 */
	bool ValidateAndBeginRefill(EHoldoverFluidType NewType);

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

	/**
	 * Server-side handler for a spray request (MP-8 validation lives here; the
	 * ServerSpray RPC delegates to this). Public so world-based automation tests
	 * can exercise the exact validation path a remote client hits (PROC-1).
	 * `NowSeconds` < 0 uses world time (tests inject their own).
	 * Returns true if treatment was applied.
	 */
	bool ServerApplySprayRequest(AHoldoverSurfaceSection* Target, float DeltaSeconds, double NowSeconds = -1.0);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Sonnet H3: unreliable — the request is idempotent (dt is clamped server
	// side), so a lost update costs nothing, while a reliable stream at 10 Hz
	// per spraying client backs up the reliable buffer under packet loss.
	UFUNCTION(Server, Unreliable)
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

	// Sonnet M1: server-side token bucket for spray time (not replicated).
	float SprayTimeAllowance = 0.f;
	double LastSprayRateTime = -1.0;

	UPROPERTY(Transient)
	class UAudioComponent* HissComponent = nullptr;
};

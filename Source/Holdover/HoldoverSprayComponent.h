// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverSprayComponent.generated.h"

class AHoldoverSurfaceSection;
class USceneComponent;

UENUM(BlueprintType)
enum class EHoldoverFluidType : uint8
{
	TypeI	UMETA(DisplayName = "Type I (clean)"),
	TypeIV	UMETA(DisplayName = "Type IV (protect)")
};

/**
 * Server-validated spray: the owning client traces and requests; the server
 * checks range + fluid and applies treatment. Type I cleans, Type IV protects
 * (clean sections only). Tanks are separate and replicated.
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

	// --- state ---
	UPROPERTY(ReplicatedUsing = OnRep_Fluid, BlueprintReadOnly, Category = "Spray")
	float TypeIFluid = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Fluid, BlueprintReadOnly, Category = "Spray")
	float TypeIVFluid = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spray")
	EHoldoverFluidType ActiveFluidType = EHoldoverFluidType::TypeI;

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

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void ToggleActiveFluid();

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void RefillTanks();

	/**
	 * Core treatment rules (server-side; testable without a world).
	 * Type I: raises cleanliness, consumes Type I tank.
	 * Type IV: only on clean sections, raises protection, consumes Type IV tank.
	 * Returns true if treatment was applied.
	 */
	bool ApplySpray(AHoldoverSurfaceSection* Target, float DeltaSeconds);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSpray(AHoldoverSurfaceSection* Target, float DeltaSeconds);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateTargetFromTrace();

	UFUNCTION()
	void OnRep_Fluid();

	bool bSpraying = false;
	float RequestAccumulator = 0.f;
};

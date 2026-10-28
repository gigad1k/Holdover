// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverSprayComponent.generated.h"

class AHoldoverSurfaceSection;

UENUM(BlueprintType)
enum class EHoldoverFluidType : uint8
{
	TypeI	UMETA(DisplayName = "Type I (clean)"),
	TypeIV	UMETA(DisplayName = "Type IV (protect)")
};

/**
 * Server-validated spray: the owning client traces and requests; the server
 * checks range + fluid and applies treatment. Fluid is tracked per component.
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
	float FluidCapacity = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float FluidUsePerSecond = 8.f;

	/** Client->server request cadence (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float SprayRequestInterval = 0.1f;

	// --- state ---
	UPROPERTY(ReplicatedUsing = OnRep_Fluid, BlueprintReadOnly, Category = "Spray")
	float TypeIFluid = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spray")
	EHoldoverFluidType ActiveFluidType = EHoldoverFluidType::TypeI;

	/** Cosmetic client-side target for HUD feedback. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Spray")
	TWeakObjectPtr<AHoldoverSurfaceSection> CurrentTargetSection;

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void SetSpraying(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Spray")
	bool IsSpraying() const { return bSpraying; }

	UFUNCTION(BlueprintCallable, Category = "Spray")
	void RefillTypeI() { TypeIFluid = FluidCapacity; }

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

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HoldoverPlayerState.generated.h"

UENUM(BlueprintType)
enum class EHoldoverFluidType : uint8
{
	TypeI	UMETA(DisplayName = "Type I (clean)"),
	TypeIV	UMETA(DisplayName = "Type IV (protect)")
};

/**
 * DES-2: per-player pay/penalty/contribution attribution. All values are
 * server-written and replicated so the results screen and dispatcher feed
 * can name-and-shame. Also carries the one-tank fluid choice.
 */
UCLASS()
class HOLDOVER_API AHoldoverPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AHoldoverPlayerState();

	// --- attribution (server writes, replicated) ---
	UPROPERTY(ReplicatedUsing = OnRep_Stats, BlueprintReadOnly, Category = "Holdover")
	int32 Earnings = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats, BlueprintReadOnly, Category = "Holdover")
	int32 Penalties = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats, BlueprintReadOnly, Category = "Holdover")
	int32 DeparturesHandled = 0;

	/** DES-2 one-tank rule: the single fluid this player carries. */
	UPROPERTY(ReplicatedUsing = OnRep_FluidChoice, BlueprintReadOnly, Category = "Holdover")
	EHoldoverFluidType CarriedFluidType = EHoldoverFluidType::TypeI;

	// --- server-side mutation ---
	void AddEarnings(int32 Amount);
	void AddPenalty(int32 Amount);
	void RecordDeparture();
	void SetCarriedFluid(EHoldoverFluidType NewType);

	/** Clears per-shift attribution for the next shift. */
	void ResetForNewShift();

	/** Display name for dispatch/results ("Dan", "Player 2", ...). */
	FString GetShortName() const;

protected:
	UFUNCTION()
	void OnRep_Stats();

	UFUNCTION()
	void OnRep_FluidChoice();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverHazardVolume.generated.h"

class UBoxComponent;
class ACharacter;

UENUM(BlueprintType)
enum class EHoldoverHazardType : uint8
{
	Ice			UMETA(DisplayName = "Ice (traction)"),
	JetBlast	UMETA(DisplayName = "Jet Blast (push + penalty)")
};

/**
 * Greybox hazard volume. Ice reduces traction while overlapped and trips a
 * brief stumble; Jet Blast pushes the player away and costs a penalty (with a
 * per-player cooldown so the same player isn't fined repeatedly).
 */
UCLASS()
class HOLDOVER_API AHoldoverHazardVolume : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverHazardVolume();

	UPROPERTY(EditAnywhere, Category = "Hazard")
	EHoldoverHazardType HazardType = EHoldoverHazardType::Ice;

	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	float IceSpeedMultiplier = 0.5f;

	/** DES-3: brief physical stumble on ice (small launch + slowdown). */
	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	float IceStumbleStrength = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	float JetBlastLaunchStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	int32 JetBlastPenalty = 25;

	/** DES-3: seconds between jet-blast penalties for the same player. */
	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	float JetBlastCooldown = 5.f;

	/** M6: client-predicted launch only on a true client. On a listen host the
	 *  authority launch already runs in the same frame - predicting too launched
	 *  twice (the second overwrote the first). */
	static bool ShouldPredictLaunch(bool bHasAuthority, bool bLocallyControlled)
	{
		return bLocallyControlled && !bHasAuthority;
	}

	/** M6: deterministic ice-stumble direction (used to be per-call FRandRange,
	 *  so the predicted and authoritative launches never matched). */
	static FVector ComputeStumbleDirection(const FVector& CharacterForward)
	{
		return (CharacterForward * 0.5f + FVector(0.f, 0.f, 0.15f)).GetSafeNormal();
	}

	/** M6: deterministic jet-blast push direction (away from the hazard). */
	static FVector ComputeJetBlastDirection(const FVector& HazardLocation, const FVector& CharacterLocation)
	{
		return (CharacterLocation - HazardLocation).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
	}

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* Box;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	/** The overlap behaviour itself (the UFUNCTIONs above just cast and forward
	 *  here) so world tests can drive it without the physics scene. */
	void HandleBeginOverlap(ACharacter* Character);
	void HandleEndOverlap(ACharacter* Character);

protected:
	/** Per-player last jet-blast penalty time (server-only). */
	TMap<TWeakObjectPtr<AActor>, float> JetBlastCooldownMap;
};

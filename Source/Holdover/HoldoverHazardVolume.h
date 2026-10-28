// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverHazardVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EHoldoverHazardType : uint8
{
	Ice			UMETA(DisplayName = "Ice (traction)"),
	JetBlast	UMETA(DisplayName = "Jet Blast (push + penalty)")
};

/**
 * Greybox hazard volume. Ice reduces traction while overlapped (fair, no lock);
 * Jet Blast pushes the player away and costs a penalty.
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

	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	float JetBlastLaunchStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	int32 JetBlastPenalty = 25;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* Box;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

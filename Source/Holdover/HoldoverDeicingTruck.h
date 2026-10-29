// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HoldoverInteractableInterface.h"
#include "HoldoverDeicingTruck.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class UFloatingPawnMovement;
class UHoldoverSprayComponent;

/**
 * Deicing truck with a rotating, extending boom and basket nozzle.
 * Stationary by default; arcade driving is modular behind bDrivingEnabled.
 * Occupancy is server-authoritative (player-controller possession).
 */
UCLASS()
class HOLDOVER_API AHoldoverDeicingTruck : public APawn, public IHoldoverInteractableInterface
{
	GENERATED_BODY()

public:
	AHoldoverDeicingTruck();

	// --- interaction (enter) ---
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	// --- boom state (server writes; replicated for presentation) ---
	UPROPERTY(ReplicatedUsing = OnRep_Boom, BlueprintReadOnly, Category = "Boom")
	float BoomYaw = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Boom, BlueprintReadOnly, Category = "Boom")
	float BoomExtension = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom")
	float MinExtension = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom")
	float MaxExtension = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom")
	float YawSpeed = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom")
	float ExtensionSpeed = 300.f;

	/** Arcade driving is modular: keep false for the parked-truck demo. */
	UPROPERTY(EditDefaultsOnly, Category = "Driving")
	bool bDrivingEnabled = false;

	/** Clamped, pure boom math (testable). */
	static float ClampExtension(float Extension, float MinValue, float MaxValue);

	UFUNCTION(BlueprintCallable, Category = "Boom")
	void SetBoom(float InYaw, float InExtension);

	// --- occupancy ---
	UFUNCTION(BlueprintCallable, Category = "Occupancy")
	bool TryEnter(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Occupancy")
	void TryExit();

	UFUNCTION(BlueprintCallable, Category = "Occupancy")
	bool IsOccupied() const { return OccupantCharacter != nullptr; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetBoom(float InYaw, float InExtension);

	UFUNCTION(Server, Reliable)
	void ServerExit();

	UFUNCTION()
	void OnRep_Boom();

	void ApplyBoomTransform();
	void YawBoom(float Direction);
	void ExtendBoom(float Direction);

	// --- input handlers (Enhanced Input, (const FInputActionValue&) signatures) ---
	void BoomYawLeft(const struct FInputActionValue& Value);
	void BoomYawRight(const struct FInputActionValue& Value);
	void BoomExtend(const struct FInputActionValue& Value);
	void BoomRetract(const struct FInputActionValue& Value);
	void SprayStarted(const struct FInputActionValue& Value);
	void SprayStopped(const struct FInputActionValue& Value);
	void ExitPressed(const struct FInputActionValue& Value);
	void DriveForward(const struct FInputActionValue& Value);
	void DriveBackward(const struct FInputActionValue& Value);
	void DriveTurnLeft(const struct FInputActionValue& Value);
	void DriveTurnRight(const struct FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* BoomPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* BoomArmMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* Basket;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* BasketMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* Nozzle;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UHoldoverSprayComponent* SprayComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UFloatingPawnMovement* Movement;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OccupantCharacter;
};

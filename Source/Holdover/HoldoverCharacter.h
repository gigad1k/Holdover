// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "HoldoverCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UHoldoverSprayComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AHoldoverCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Server-validated spray (Type I / Type IV treatment) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UHoldoverSprayComponent* SprayComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	AHoldoverCharacter();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	virtual void Tick(float DeltaSeconds) override;

	/** Sprint (bound to Left Shift via transient action) */
	void SprintStart(const FInputActionValue& Value);
	void SprintEnd(const FInputActionValue& Value);

	/** Interact (bound to E via transient action): trace from camera and call the target's interact */
	void InteractPressed(const FInputActionValue& Value);

	/** Spray (bound to Left Mouse via transient action) */
	void SprayStarted(const FInputActionValue& Value);
	void SprayStopped(const FInputActionValue& Value);

	/** UE 5.8 removed BindKey: sprint/interact/spray use runtime-created actions + mapping context. */
	void EnsureExtraInputBindings(UInputComponent* InputComponent);

	UPROPERTY(Transient)
	UInputAction* SprintAction;

	UPROPERTY(Transient)
	UInputAction* InteractAction;

	UPROPERTY(Transient)
	UInputAction* SprayAction;

	UPROPERTY(Transient)
	class UInputMappingContext* HoldoverExtraContext;

	/** Client-side cosmetic prompt refresh via a short camera trace */
	void UpdateInteractionPrompt();

	/** Server-validated interaction. Clients request; the server range-checks and applies. */
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeedMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionDistance = 500.f;


public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns the spray component **/
	UHoldoverSprayComponent* GetSprayComponent() const { return SprayComponent; }

};


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

	virtual void BeginPlay() override;

public:
	// --- replicated speed state (MP-2) ---

	/** Server-side ice slow toggled by hazard volumes (Multiplier = hazard tuning
	 *  value). M5: overlap-counted — begin/end pairs must balance, and the slow
	 *  only clears when the last overlapping ice patch releases. */
	void SetIceSlowed(bool bSlowed, float Multiplier);

	/** MP-6: vehicle occupancy hides the body and disables collision. Replicated so
	 *  every client applies the same capsule state (collision itself does not replicate). */
	void SetVehicleHidden(bool bHiddenInVehicle);

	/** Pure speed-scale formula (world-free, testable). Sprint and ice multiply — they can
	 *  never stack with raw MaxWalkSpeed writes into a permanent speed change. */
	static float ComputeSpeedScale(bool bSprinting, float SprintScale, float IceScale);

	/**
	 * G-4: server-side interact validation — distance + line of sight. The
	 * ServerInteract RPC calls this before touching any interactable. Static and
	 * caller-agnostic so world-based tests can drive it directly (PROC-1).
	 */
	static bool ValidateInteractTarget(const AActor* Caller, const AActor* Target, float Range);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_SpeedScale();

	/** M4: recomputes the local speed when the server's ice multiplier arrives,
	 *  so a sprinting client predicts Base*Ice*Sprint exactly like the server. */
	UFUNCTION()
	void OnRep_IceScale();

	UFUNCTION()
	void OnRep_VehicleHidden();

	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bEnabled);

	void SetSprintLocal(bool bEnabled);
	void RecomputeSpeedScale();
	void ApplySpeedScale();
	void ApplyVehicleHidden();

	/** Authoritative movement multiplier; MaxWalkSpeed = BaseWalkSpeed * SpeedScale (MP-2). */
	UPROPERTY(ReplicatedUsing = OnRep_SpeedScale)
	float SpeedScale = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_VehicleHidden)
	bool bVehicleHidden = false;

	bool bSprinting = false;

	/** Server-set ice multiplier (hazard volumes). Replicated (M4) because the
	 *  client's local sprint prediction multiplies by it. */
	UPROPERTY(ReplicatedUsing = OnRep_IceScale)
	float IceScale = 1.f;

	/** M5: number of currently overlapped ice patches (server-only). */
	int32 IceOverlapCount = 0;

	float BaseWalkSpeed = 0.f;

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

	/** Toggle Type I / Type IV (bound to Q via transient action) */
	void ToggleFluid(const FInputActionValue& Value);

	/** D1-D3: toggle the field manual overlay (bound to F1 via transient action). */
	void ToggleInfoPanel(const FInputActionValue& Value);

	/** UE 5.8 removed BindKey: sprint/interact/spray use runtime-created actions + mapping context. */
	void EnsureExtraInputBindings(UInputComponent* InputComponent);

	/** MP-9: explicit mapping-context removal on possession end (contexts must not
	 *  accumulate across enter/exit cycles). */
	void RemoveHoldoverInputContext();

	/** Sonnet M14: held so the looping wind ambience can be stopped on EndPlay
	 *  (it used to leak a new loop per respawn/PIE restart). */
	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> WindLoopComponent;

	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UEnhancedInputLocalPlayerSubsystem> CachedInputSubsystem;

	UPROPERTY(Transient)
	UInputAction* SprintAction;

	UPROPERTY(Transient)
	UInputAction* InteractAction;

	UPROPERTY(Transient)
	UInputAction* SprayAction;

	UPROPERTY(Transient)
	UInputAction* FluidToggleAction;

	UPROPERTY(Transient)
	UInputAction* InfoAction;

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


// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Holdover.h"
#include "HoldoverHUD.h"
#include "HoldoverInteractableInterface.h"
#include "HoldoverSprayComponent.h"

AHoldoverCharacter::AHoldoverCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// Spray / treatment component (server-validated)
	SprayComponent = CreateDefaultSubobject<UHoldoverSprayComponent>(TEXT("Spray Component"));

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Network: standard replicated character movement for listen-server co-op
	bReplicates = true;
	SetReplicatingMovement(true);
}

void AHoldoverCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AHoldoverCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHoldoverCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHoldoverCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHoldoverCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHoldoverCharacter::LookInput);

		// Sprint + interact (transient actions; UE 5.8 has no BindKey)
		EnsureExtraInputBindings(InputComponent);
	}
	else
	{
		UE_LOG(LogHoldover, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AHoldoverCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AHoldoverCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AHoldoverCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AHoldoverCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AHoldoverCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AHoldoverCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void AHoldoverCharacter::EnsureExtraInputBindings(UInputComponent* InInputComponent)
{
	if (!SprintAction)
	{
		SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint_Transient"));
		InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact_Transient"));
		SprayAction = NewObject<UInputAction>(this, TEXT("IA_Spray_Transient"));
		FluidToggleAction = NewObject<UInputAction>(this, TEXT("IA_FluidToggle_Transient"));
		BoardToggleAction = NewObject<UInputAction>(this, TEXT("IA_BoardToggle_Transient"));
		HoldoverExtraContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Holdover_Transient"));
		HoldoverExtraContext->MapKey(SprintAction, EKeys::LeftShift);
		HoldoverExtraContext->MapKey(InteractAction, EKeys::E);
		HoldoverExtraContext->MapKey(SprayAction, EKeys::LeftMouseButton);
		HoldoverExtraContext->MapKey(FluidToggleAction, EKeys::Q);
		HoldoverExtraContext->MapKey(BoardToggleAction, EKeys::Tab);

		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					Sub->AddMappingContext(HoldoverExtraContext, 100);
				}
			}
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InInputComponent))
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &AHoldoverCharacter::SprintStart);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHoldoverCharacter::SprintEnd);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AHoldoverCharacter::InteractPressed);
		EIC->BindAction(SprayAction, ETriggerEvent::Started, this, &AHoldoverCharacter::SprayStarted);
		EIC->BindAction(SprayAction, ETriggerEvent::Completed, this, &AHoldoverCharacter::SprayStopped);
		EIC->BindAction(FluidToggleAction, ETriggerEvent::Started, this, &AHoldoverCharacter::ToggleFluid);
		EIC->BindAction(BoardToggleAction, ETriggerEvent::Started, this, &AHoldoverCharacter::BoardTogglePressed);
	}
}

void AHoldoverCharacter::ToggleFluid(const FInputActionValue& Value)
{
	if (SprayComponent)
	{
		SprayComponent->ToggleActiveFluid();
	}
}

void AHoldoverCharacter::BoardTogglePressed(const FInputActionValue& Value)
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AHoldoverHUD* HUD = Cast<AHoldoverHUD>(PC->GetHUD()))
		{
			HUD->ToggleDepartureBoard();
		}
	}
}

void AHoldoverCharacter::SprayStarted(const FInputActionValue& Value)
{
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(true);
	}
}

void AHoldoverCharacter::SprayStopped(const FInputActionValue& Value)
{
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(false);
	}
}

void AHoldoverCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		UpdateInteractionPrompt();
	}
}

void AHoldoverCharacter::SprintStart(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Move->GetClass()->GetDefaultObject<UCharacterMovementComponent>()->MaxWalkSpeed * SprintSpeedMultiplier;
	}
}

void AHoldoverCharacter::SprintEnd(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Move->GetClass()->GetDefaultObject<UCharacterMovementComponent>()->MaxWalkSpeed;
	}
}

void AHoldoverCharacter::InteractPressed(const FInputActionValue& Value)
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverInteract), false, this);

	const FVector Start = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
	const FVector End = Start + (GetControlRotation().Vector() * InteractionDistance);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* Target = Hit.GetActor())
		{
			if (Target->GetClass()->ImplementsInterface(UHoldoverInteractableInterface::StaticClass()))
			{
				if (HasAuthority())
				{
					IHoldoverInteractableInterface::Execute_Interact(Target, this);
				}
				else
				{
					ServerInteract(Target);
				}
			}
		}
	}
}

void AHoldoverCharacter::ServerInteract_Implementation(AActor* Target)
{
	if (!IsValid(Target) ||
		!Target->GetClass()->ImplementsInterface(UHoldoverInteractableInterface::StaticClass()))
	{
		return;
	}

	// Server-side range validation (generous slack for latency)
	if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(InteractionDistance * 2.f))
	{
		UE_LOG(LogHoldover, Warning, TEXT("Interact rejected (out of range): %s -> %s"), *GetName(), *Target->GetName());
		return;
	}

	IHoldoverInteractableInterface::Execute_Interact(Target, this);
}

void AHoldoverCharacter::UpdateInteractionPrompt()
{
	FText Prompt;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverInteractPrompt), false, this);

	const FVector Start = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
	const FVector End = Start + (GetControlRotation().Vector() * InteractionDistance);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* Target = Hit.GetActor())
		{
			if (Target->GetClass()->ImplementsInterface(UHoldoverInteractableInterface::StaticClass()))
			{
				Prompt = IHoldoverInteractableInterface::Execute_GetInteractionPrompt(Target);
			}
		}
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AHoldoverHUD* HUD = Cast<AHoldoverHUD>(PC->GetHUD()))
		{
			HUD->InteractionPrompt = Prompt;
		}
	}
}

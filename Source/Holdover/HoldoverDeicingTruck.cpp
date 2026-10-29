// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverDeicingTruck.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Holdover.h"
#include "HoldoverSprayComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "HoldoverHUD.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AHoldoverDeicingTruck::AHoldoverDeicingTruck()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(200.f, 120.f, 80.f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Body;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
	}
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	BodyMesh->SetRelativeScale3D(FVector(2.f, 1.2f, 0.8f));
	BodyMesh->SetCollisionProfileName(TEXT("NoCollision"));

	BoomPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoomPivot"));
	BoomPivot->SetupAttachment(Body);
	BoomPivot->SetRelativeLocation(FVector(-40.f, 0.f, 180.f));

	BoomArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoomArmMesh"));
	BoomArmMesh->SetupAttachment(BoomPivot);
	if (CubeMesh.Succeeded())
	{
		BoomArmMesh->SetStaticMesh(CubeMesh.Object);
	}
	BoomArmMesh->SetCollisionProfileName(TEXT("NoCollision"));

	Basket = CreateDefaultSubobject<USceneComponent>(TEXT("Basket"));
	Basket->SetupAttachment(BoomPivot);

	BasketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasketMesh"));
	BasketMesh->SetupAttachment(Basket);
	if (CubeMesh.Succeeded())
	{
		BasketMesh->SetStaticMesh(CubeMesh.Object);
	}
	BasketMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.5f));
	BasketMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Nozzle points along the boom direction (pivot X, pitched down 25 degrees).
	Nozzle = CreateDefaultSubobject<USceneComponent>(TEXT("Nozzle"));
	Nozzle->SetupAttachment(Basket);

	SprayComponent = CreateDefaultSubobject<UHoldoverSprayComponent>(TEXT("Spray Component"));
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Body);
}

float AHoldoverDeicingTruck::ClampExtension(float Extension, float MinValue, float MaxValue)
{
	return FMath::Clamp(Extension, FMath::Min(MinValue, MaxValue), FMath::Max(MinValue, MaxValue));
}

void AHoldoverDeicingTruck::SetBoom(float InYaw, float InExtension)
{
	BoomYaw = InYaw;
	BoomExtension = ClampExtension(InExtension, MinExtension, MaxExtension);
	ApplyBoomTransform();
}

void AHoldoverDeicingTruck::ApplyBoomTransform()
{
	BoomPivot->SetRelativeRotation(FRotator(-25.f, BoomYaw, 0.f));
	BoomArmMesh->SetRelativeLocation(FVector(BoomExtension * 0.5f, 0.f, 0.f));
	BoomArmMesh->SetRelativeScale3D(FVector(BoomExtension / 100.f, 0.15f, 0.15f));
	Basket->SetRelativeLocation(FVector(BoomExtension, 0.f, 0.f));
}

FText AHoldoverDeicingTruck::GetInteractionPrompt_Implementation() const
{
	return IsOccupied()
		? FText::FromString(TEXT("Truck occupied"))
		: FText::FromString(TEXT("Press E - Enter Deicing Truck"));
}

void AHoldoverDeicingTruck::Interact_Implementation(AActor* Interactor)
{
	TryEnter(Interactor);
}

bool AHoldoverDeicingTruck::TryEnter(AActor* Interactor)
{
	if (!HasAuthority() || IsOccupied())
	{
		return false;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn || !Pawn->GetController())
	{
		return false;
	}

	OccupantCharacter = Pawn;
	Pawn->GetController()->Possess(this);
	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);
	if (SprayComponent)
	{
		SprayComponent->SetSprayOrigin(Nozzle);
	}
	UE_LOG(LogHoldover, Display, TEXT("Truck '%s' entered by '%s'"), *GetName(), *Pawn->GetName());
	return true;
}

void AHoldoverDeicingTruck::TryExit()
{
	if (HasAuthority())
	{
		ServerExit_Implementation();
	}
	else
	{
		ServerExit();
	}
}

void AHoldoverDeicingTruck::ServerExit_Implementation()
{
	APawn* Pawn = Cast<APawn>(OccupantCharacter.Get());
	if (Pawn)
	{
		if (AController* PController = GetController())
		{
			PController->Possess(Pawn);
		}
		Pawn->SetActorHiddenInGame(false);
		Pawn->SetActorEnableCollision(true);
		UE_LOG(LogHoldover, Display, TEXT("Truck '%s' exited back to '%s'"), *GetName(), *Pawn->GetName());
	}
	OccupantCharacter.Reset();
	if (SprayComponent)
	{
		SprayComponent->SetSprayOrigin(nullptr);
		SprayComponent->SetSpraying(false);
	}
}

void AHoldoverDeicingTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AHoldoverDeicingTruck::ServerSetBoom_Implementation(float InYaw, float InExtension)
{
	SetBoom(InYaw, InExtension);
}

void AHoldoverDeicingTruck::OnRep_Boom()
{
	ApplyBoomTransform();
}

void AHoldoverDeicingTruck::YawBoom(float Direction)
{
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	SetBoom(BoomYaw + Direction * YawSpeed * DeltaSeconds, BoomExtension);
	if (!HasAuthority())
	{
		ServerSetBoom(BoomYaw, BoomExtension);
	}
}

void AHoldoverDeicingTruck::ExtendBoom(float Direction)
{
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	SetBoom(BoomYaw, BoomExtension + Direction * ExtensionSpeed * DeltaSeconds);
	if (!HasAuthority())
	{
		ServerSetBoom(BoomYaw, BoomExtension);
	}
}

void AHoldoverDeicingTruck::BoomYawLeft(const FInputActionValue& Value) { YawBoom(-1.f); }
void AHoldoverDeicingTruck::BoomYawRight(const FInputActionValue& Value) { YawBoom(1.f); }
void AHoldoverDeicingTruck::BoomExtend(const FInputActionValue& Value) { ExtendBoom(1.f); }
void AHoldoverDeicingTruck::BoomRetract(const FInputActionValue& Value) { ExtendBoom(-1.f); }

void AHoldoverDeicingTruck::SprayStarted(const FInputActionValue& Value)
{
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(true);
	}
}

void AHoldoverDeicingTruck::SprayStopped(const FInputActionValue& Value)
{
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(false);
	}
}

void AHoldoverDeicingTruck::ExitPressed(const FInputActionValue& Value)
{
	TryExit();
}

void AHoldoverDeicingTruck::BoardTogglePressed(const FInputActionValue& Value)
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AHoldoverHUD* HUD = Cast<AHoldoverHUD>(PC->GetHUD()))
		{
			HUD->ToggleDepartureBoard();
		}
	}
}

void AHoldoverDeicingTruck::DriveForward(const FInputActionValue& Value)
{
	if (bDrivingEnabled)
	{
		AddMovementInput(GetActorForwardVector(), 1.f);
	}
}

void AHoldoverDeicingTruck::DriveBackward(const FInputActionValue& Value)
{
	if (bDrivingEnabled)
	{
		AddMovementInput(GetActorForwardVector(), -1.f);
	}
}

void AHoldoverDeicingTruck::DriveTurnLeft(const FInputActionValue& Value)
{
	if (bDrivingEnabled)
	{
		AddControllerYawInput(-1.f);
	}
}

void AHoldoverDeicingTruck::DriveTurnRight(const FInputActionValue& Value)
{
	if (bDrivingEnabled)
	{
		AddControllerYawInput(1.f);
	}
}

void AHoldoverDeicingTruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	// UE 5.8 has no BindKey: runtime actions + mapping context (same pattern as the character).
	UInputAction* YawLeft = NewObject<UInputAction>(this, TEXT("IA_BoomYawLeft_Transient"));
	UInputAction* YawRight = NewObject<UInputAction>(this, TEXT("IA_BoomYawRight_Transient"));
	UInputAction* Extend = NewObject<UInputAction>(this, TEXT("IA_BoomExtend_Transient"));
	UInputAction* Retract = NewObject<UInputAction>(this, TEXT("IA_BoomRetract_Transient"));
	UInputAction* Spray = NewObject<UInputAction>(this, TEXT("IA_TruckSpray_Transient"));
	UInputAction* Exit = NewObject<UInputAction>(this, TEXT("IA_TruckExit_Transient"));
	UInputAction* Board = NewObject<UInputAction>(this, TEXT("IA_TruckBoardToggle_Transient"));
	UInputAction* Fwd = NewObject<UInputAction>(this, TEXT("IA_DriveFwd_Transient"));
	UInputAction* Back = NewObject<UInputAction>(this, TEXT("IA_DriveBack_Transient"));
	UInputAction* TurnL = NewObject<UInputAction>(this, TEXT("IA_DriveTurnL_Transient"));
	UInputAction* TurnR = NewObject<UInputAction>(this, TEXT("IA_DriveTurnR_Transient"));

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Truck_Transient"));
	IMC->MapKey(YawLeft, EKeys::A);
	IMC->MapKey(YawRight, EKeys::D);
	IMC->MapKey(Extend, EKeys::W);
	IMC->MapKey(Retract, EKeys::S);
	IMC->MapKey(Spray, EKeys::LeftMouseButton);
	IMC->MapKey(Exit, EKeys::E);
	IMC->MapKey(Board, EKeys::Tab);
	IMC->MapKey(Fwd, EKeys::Up);
	IMC->MapKey(Back, EKeys::Down);
	IMC->MapKey(TurnL, EKeys::Left);
	IMC->MapKey(TurnR, EKeys::Right);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->AddMappingContext(IMC, 100);
			}
		}
	}

	EIC->BindAction(YawLeft, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomYawLeft);
	EIC->BindAction(YawRight, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomYawRight);
	EIC->BindAction(Extend, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomExtend);
	EIC->BindAction(Retract, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomRetract);
	EIC->BindAction(Spray, ETriggerEvent::Started, this, &AHoldoverDeicingTruck::SprayStarted);
	EIC->BindAction(Spray, ETriggerEvent::Completed, this, &AHoldoverDeicingTruck::SprayStopped);
	EIC->BindAction(Exit, ETriggerEvent::Started, this, &AHoldoverDeicingTruck::ExitPressed);
	EIC->BindAction(Board, ETriggerEvent::Started, this, &AHoldoverDeicingTruck::BoardTogglePressed);
	EIC->BindAction(Fwd, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveForward);
	EIC->BindAction(Back, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveBackward);
	EIC->BindAction(TurnL, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveTurnLeft);
	EIC->BindAction(TurnR, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveTurnRight);
}

void AHoldoverDeicingTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHoldoverDeicingTruck, BoomYaw);
	DOREPLIFETIME(AHoldoverDeicingTruck, BoomExtension);
}

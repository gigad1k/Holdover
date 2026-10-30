// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverDeicingTruck.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/NetConnection.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Holdover.h"
#include "HoldoverCharacter.h"
#include "HoldoverSprayComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"

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
	// MP-4: the nozzle is the spray origin on EVERY instance (server + owning
	// clients), so remote occupants' targeting traces come from the basket too.
	SprayComponent->SetSprayOrigin(Nozzle);
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Body);
}

float AHoldoverDeicingTruck::ClampExtension(float Extension, float MinValue, float MaxValue)
{
	return FMath::Clamp(Extension, FMath::Min(MinValue, MaxValue), FMath::Max(MinValue, MaxValue));
}

float AHoldoverDeicingTruck::ClampBoomYaw(float Yaw, float MaxAbsYawValue)
{
	return FMath::Clamp(Yaw, -FMath::Abs(MaxAbsYawValue), FMath::Abs(MaxAbsYawValue));
}

void AHoldoverDeicingTruck::SetBoom(float InYaw, float InExtension)
{
	BoomYaw = ClampBoomYaw(InYaw, MaxAbsYaw);
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
	// MP-6: replicated hidden/collision state (raw SetActorEnableCollision does not replicate).
	if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Pawn))
	{
		HC->SetVehicleHidden(true);
	}
	if (SprayComponent)
	{
		SprayComponent->SetSprayOrigin(Nozzle);
	}
	UE_LOG(LogHoldover, Display, TEXT("Truck '%s' entered by '%s'"), *GetName(), *Pawn->GetName());
	return true;
}

void AHoldoverDeicingTruck::TryExit()
{
	UE_LOG(LogHoldover, Display, TEXT("TryExit: %s authority=%d"), *GetName(), HasAuthority() ? 1 : 0);
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
	UE_LOG(LogHoldover, Display, TEXT("ServerExit arrived: %s occupied=%d controller=%s"),
		*GetName(), OccupantCharacter.IsValid() ? 1 : 0, *GetNameSafe(GetController()));
	APawn* Pawn = Cast<APawn>(OccupantCharacter.Get());
	if (Pawn)
	{
		if (AController* PController = GetController())
		{
			PController->Possess(Pawn);
		}
		if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Pawn))
		{
			HC->SetVehicleHidden(false);
		}
		UE_LOG(LogHoldover, Display, TEXT("Truck '%s' exited back to '%s'"), *GetName(), *Pawn->GetName());
	}
	OccupantCharacter.Reset();
	if (SprayComponent)
	{
		SprayComponent->SetSprayOrigin(nullptr);
		SprayComponent->SetSpraying(false);
	}
}

void AHoldoverDeicingTruck::StopSprayLocal()
{
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(false);
		SprayComponent->SetSprayOrigin(nullptr);
	}
}

void AHoldoverDeicingTruck::EvictOccupant()
{
	UE_LOG(LogHoldover, Display, TEXT("EvictOccupant: %s world=%s had=%d"), *GetName(),
		GetWorld() ? *GetWorld()->GetName() : TEXT("null"), OccupantCharacter.IsValid() ? 1 : 0);
	if (APawn* Pawn = Cast<APawn>(OccupantCharacter.Get()))
	{
		if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Pawn))
		{
			HC->SetVehicleHidden(false);
		}
	}
	OccupantCharacter.Reset();
	StopSprayLocal();
}

void AHoldoverDeicingTruck::UnPossessed()
{
	// P7-2/MP-9: kill local spray loop + input context on any possession-lifetime edge.
	// NOTE: the engine also calls UnPossessed inside Possess() during ENTRY (callstack
	// verified 2026-10-30) — so this must NOT evict the occupant here. Departure
	// eviction lives in ServerExit, GameMode::Logout and EndPlay.
	StopSprayLocal();
	RemoveTruckInputContext();
	Super::UnPossessed();
}

void AHoldoverDeicingTruck::OnRep_Controller()
{
	// Fires on the owning client on possession CHANGE — including arrival (entry).
	// P7-2/MP-9: always kill the local spray loop + input context; evict only when
	// possession actually LEFT (controller cleared).
	StopSprayLocal();
	RemoveTruckInputContext();
	if (!GetController())
	{
		EvictOccupant();
	}
	Super::OnRep_Controller();
}

void AHoldoverDeicingTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EvictOccupant();
	RemoveTruckInputContext();
	Super::EndPlay(EndPlayReason);
}

void AHoldoverDeicingTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AHoldoverDeicingTruck::ServerSetBoom_Implementation(float InYaw, float InExtension)
{
	// MP-7: validate movement rate — clamp each request to one second of possible
	// motion from the server's current state (tolerates lag bursts, rejects teleports).
	const float Yaw = BoomYaw + FMath::Clamp(InYaw - BoomYaw, -YawSpeed, YawSpeed);
	const float Ext = BoomExtension + FMath::Clamp(InExtension - BoomExtension, -ExtensionSpeed, ExtensionSpeed);
	SetBoom(Yaw, Ext);
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
	// Owner-side: kill the looping spray hiss before the server handles the exit.
	if (SprayComponent)
	{
		SprayComponent->SetSpraying(false);
	}
	TryExit();
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

	// MP-9: create actions/context and bind ONCE per truck — re-possession must not
	// pile up mapping contexts and duplicate bindings.
	if (!TruckContext)
	{
		UInputAction* YawLeft = NewObject<UInputAction>(this, TEXT("IA_BoomYawLeft_Transient"));
		UInputAction* YawRight = NewObject<UInputAction>(this, TEXT("IA_BoomYawRight_Transient"));
		UInputAction* Extend = NewObject<UInputAction>(this, TEXT("IA_BoomExtend_Transient"));
		UInputAction* Retract = NewObject<UInputAction>(this, TEXT("IA_BoomRetract_Transient"));
		UInputAction* Spray = NewObject<UInputAction>(this, TEXT("IA_TruckSpray_Transient"));
		UInputAction* Exit = NewObject<UInputAction>(this, TEXT("IA_TruckExit_Transient"));
		UInputAction* Fwd = NewObject<UInputAction>(this, TEXT("IA_DriveFwd_Transient"));
		UInputAction* Back = NewObject<UInputAction>(this, TEXT("IA_DriveBack_Transient"));
		UInputAction* TurnL = NewObject<UInputAction>(this, TEXT("IA_DriveTurnL_Transient"));
		UInputAction* TurnR = NewObject<UInputAction>(this, TEXT("IA_DriveTurnR_Transient"));

		TruckContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Truck_Transient"));
		TruckContext->MapKey(YawLeft, EKeys::A);
		TruckContext->MapKey(YawRight, EKeys::D);
		TruckContext->MapKey(Extend, EKeys::W);
		TruckContext->MapKey(Retract, EKeys::S);
		TruckContext->MapKey(Spray, EKeys::LeftMouseButton);
		TruckContext->MapKey(Exit, EKeys::E);
		TruckContext->MapKey(Fwd, EKeys::Up);
		TruckContext->MapKey(Back, EKeys::Down);
		TruckContext->MapKey(TurnL, EKeys::Left);
		TruckContext->MapKey(TurnR, EKeys::Right);

		EIC->BindAction(YawLeft, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomYawLeft);
		EIC->BindAction(YawRight, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomYawRight);
		EIC->BindAction(Extend, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomExtend);
		EIC->BindAction(Retract, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::BoomRetract);
		EIC->BindAction(Spray, ETriggerEvent::Started, this, &AHoldoverDeicingTruck::SprayStarted);
		EIC->BindAction(Spray, ETriggerEvent::Completed, this, &AHoldoverDeicingTruck::SprayStopped);
		EIC->BindAction(Exit, ETriggerEvent::Started, this, &AHoldoverDeicingTruck::ExitPressed);
		EIC->BindAction(Fwd, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveForward);
		EIC->BindAction(Back, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveBackward);
		EIC->BindAction(TurnL, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveTurnLeft);
		EIC->BindAction(TurnR, ETriggerEvent::Triggered, this, &AHoldoverDeicingTruck::DriveTurnRight);
	}

	// MP-9: priority 200 — the truck context outranks the on-foot context (100).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				CachedInputSubsystem = Sub;
				Sub->RemoveMappingContext(TruckContext);
				Sub->AddMappingContext(TruckContext, 200);
			}
		}
	}
}

void AHoldoverDeicingTruck::RemoveTruckInputContext()
{
	if (!TruckContext)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = CachedInputSubsystem.Get())
	{
		Sub->RemoveMappingContext(TruckContext);
	}
	CachedInputSubsystem.Reset();
}

void AHoldoverDeicingTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// MP-7: skip the owner — it predicts the boom locally and must not snap back.
	DOREPLIFETIME_CONDITION(AHoldoverDeicingTruck, BoomYaw, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AHoldoverDeicingTruck, BoomExtension, COND_SkipOwner);
	DOREPLIFETIME(AHoldoverDeicingTruck, OccupantCharacter);
}

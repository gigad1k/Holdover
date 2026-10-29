// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverSprayComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Holdover.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverCharacter.h"
#include "HoldoverSurfaceSection.h"
#include "Net/UnrealNetwork.h"

UHoldoverSprayComponent::UHoldoverSprayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UHoldoverSprayComponent::SetSpraying(bool bEnabled)
{
	bSpraying = bEnabled;
	RequestAccumulator = 0.f;

	// Cosmetic spray hiss on the owning client only. The stop is unconditional so
	// server-side stops are harmless no-ops and owner-side stops always land.
	if (bEnabled)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !HissComponent)
		{
			HissComponent = UHoldoverAudioLibrary::Play2D(GetOwner(), FName("spray_hiss"), 0.9f, true);
			UE_LOG(LogHoldover, Display, TEXT("spray hiss start (%s)"), *GetNameSafe(GetOwner()));
		}
	}
	else if (HissComponent)
	{
		HissComponent->Stop();
		HissComponent = nullptr;
		UE_LOG(LogHoldover, Display, TEXT("spray hiss stop (%s)"), *GetNameSafe(GetOwner()));
	}
}

void UHoldoverSprayComponent::ToggleActiveFluid()
{
	ActiveFluidType = (ActiveFluidType == EHoldoverFluidType::TypeI)
		? EHoldoverFluidType::TypeIV
		: EHoldoverFluidType::TypeI;

	// Owner applies the flip optimistically for HUD responsiveness; the server
	// value (from this RPC) is authoritative and replicates back.
	ServerSetFluid(ActiveFluidType);
}

void UHoldoverSprayComponent::RefillTanks()
{
	TypeIFluid = FluidCapacity;
	TypeIVFluid = FluidCapacity;
}

bool UHoldoverSprayComponent::ApplySpray(AHoldoverSurfaceSection* Target, float DeltaSeconds)
{
	if (!IsValid(Target) || DeltaSeconds <= 0.f)
	{
		return false;
	}

	if (ActiveFluidType == EHoldoverFluidType::TypeI)
	{
		if (TypeIFluid <= 0.f)
		{
			return false;
		}
		Target->ApplyCleaning(CleanRatePerSecond * DeltaSeconds);
		TypeIFluid = FMath::Max(0.f, TypeIFluid - TypeIUsePerSecond * DeltaSeconds);
		return true;
	}

	// Type IV: wrong fluid on a dirty section — rejected by rule.
	if (!Target->IsClean())
	{
		UE_LOG(LogHoldover, Verbose, TEXT("Type IV rejected (section not clean): %s"), *Target->GetName());
		return false;
	}
	if (TypeIVFluid <= 0.f)
	{
		return false;
	}
	Target->ApplyProtection(ProtectRatePerSecond * DeltaSeconds);
	TypeIVFluid = FMath::Max(0.f, TypeIVFluid - TypeIVUsePerSecond * DeltaSeconds);
	return true;
}

void UHoldoverSprayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		// Only the owning client (incl. listen host) drives spray requests.
		return;
	}

	if (!bSpraying)
	{
		CurrentTargetSection.Reset();
		TargetHint = FText::GetEmpty();
		return;
	}

	UpdateTargetFromTrace();

	RequestAccumulator += DeltaTime;
	if (RequestAccumulator >= SprayRequestInterval)
	{
		if (AHoldoverSurfaceSection* Target = CurrentTargetSection.Get())
		{
			// On the listen host this calls the implementation directly; on clients it is an RPC.
			ServerSpray(Target, RequestAccumulator);
		}
		RequestAccumulator = 0.f;
	}
}

void UHoldoverSprayComponent::UpdateTargetFromTrace()
{
	CurrentTargetSection.Reset();
	TargetHint = FText::GetEmpty();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Owner);
	FVector Start = Owner->GetActorLocation();
	FRotator Aim = Owner->GetActorRotation();

	if (USceneComponent* Origin = SprayOrigin.Get())
	{
		// Truck nozzle / custom origin wins.
		Start = Origin->GetComponentLocation();
		Aim = Origin->GetComponentRotation();
	}
	else if (Character && Character->GetFirstPersonCameraComponent())
	{
		Start = Character->GetFirstPersonCameraComponent()->GetComponentLocation();
		Aim = Character->GetControlRotation();
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverSpray), false, Owner);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Aim.Vector() * SprayRange, ECC_Visibility, Params))
	{
		if (AHoldoverSurfaceSection* Section = Cast<AHoldoverSurfaceSection>(Hit.GetActor()))
		{
			CurrentTargetSection = Section;

			// Cosmetic pre-validation hint (the server enforces the real rules).
			if (ActiveFluidType == EHoldoverFluidType::TypeIV && !Section->IsClean())
			{
				TargetHint = FText::FromString(TEXT("Section not clean - use Type I first"));
			}
			else if (ActiveFluidType == EHoldoverFluidType::TypeI && TypeIFluid <= 0.f)
			{
				TargetHint = FText::FromString(TEXT("Type I tank empty - refill (E)"));
			}
			else if (ActiveFluidType == EHoldoverFluidType::TypeIV && TypeIVFluid <= 0.f)
			{
				TargetHint = FText::FromString(TEXT("Type IV tank empty - refill (E)"));
			}
		}
	}
}

void UHoldoverSprayComponent::ServerSpray_Implementation(AHoldoverSurfaceSection* Target, float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner || !IsValid(Target) || DeltaSeconds <= 0.f)
	{
		return;
	}

	// Server-side range validation
	if (FVector::DistSquared(Owner->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(SprayRange * 2.f))
	{
		UE_LOG(LogHoldover, Warning, TEXT("Spray rejected (out of range): %s -> %s"), *Owner->GetName(), *Target->GetName());
		return;
	}

	ApplySpray(Target, DeltaSeconds);
}

void UHoldoverSprayComponent::ServerSetFluid_Implementation(EHoldoverFluidType NewFluid)
{
	ActiveFluidType = NewFluid;
}

void UHoldoverSprayComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoldoverSprayComponent, TypeIFluid);
	DOREPLIFETIME(UHoldoverSprayComponent, TypeIVFluid);
	DOREPLIFETIME(UHoldoverSprayComponent, ActiveFluidType);
}

void UHoldoverSprayComponent::OnRep_Fluid()
{
	// HUD reads the tank levels directly.
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverSprayComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Holdover.h"
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

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Owner);
	FVector Start = Owner->GetActorLocation();
	FRotator Aim = Owner->GetActorRotation();
	if (Character && Character->GetFirstPersonCameraComponent())
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

	if (TypeIFluid <= 0.f)
	{
		return;
	}

	const float CleanAmount = CleanRatePerSecond * DeltaSeconds;
	const float FluidAmount = FluidUsePerSecond * DeltaSeconds;

	Target->ApplyCleaning(CleanAmount);
	TypeIFluid = FMath::Max(0.f, TypeIFluid - FluidAmount);
}

void UHoldoverSprayComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoldoverSprayComponent, TypeIFluid);
}

void UHoldoverSprayComponent::OnRep_Fluid()
{
	// HUD reads TypeIFluid directly.
}

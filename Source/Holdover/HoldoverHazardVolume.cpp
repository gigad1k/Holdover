// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHazardVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Holdover.h"
#include "HoldoverGameState.h"

AHoldoverHazardVolume::AHoldoverHazardVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	Box->OnComponentBeginOverlap.AddDynamic(this, &AHoldoverHazardVolume::OnOverlapBegin);
	Box->OnComponentEndOverlap.AddDynamic(this, &AHoldoverHazardVolume::OnOverlapEnd);
}

void AHoldoverHazardVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !HasAuthority())
	{
		return;
	}

	if (HazardType == EHoldoverHazardType::Ice)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->MaxWalkSpeed *= IceSpeedMultiplier;
		}
		if (AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			GS->AddDispatch(TEXT("Ice! Watch your step out there."));
		}
	}
	else
	{
		const FVector LaunchDir = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
		Character->LaunchCharacter(LaunchDir * JetBlastLaunchStrength, true, true);

		if (AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			GS->RegisterPenalty(JetBlastPenalty, TEXT("Jet blast incident! Keep clear of the exhaust."));
		}
	}
}

void AHoldoverHazardVolume::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !HasAuthority() || HazardType != EHoldoverHazardType::Ice)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->MaxWalkSpeed /= IceSpeedMultiplier;
	}
}

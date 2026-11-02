// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHazardVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Holdover.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverCharacter.h"
#include "HoldoverGameState.h"
#include "HoldoverPlayerState.h"

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
	if (!Character)
	{
		return;
	}

	// Local-only collision cue + client-predicted launch for instant feedback.
	// The server authoritative launch below is the real one; this is cosmetic.
	if (Character->IsLocallyControlled())
	{
		UHoldoverAudioLibrary::Play2D(this, FName("collision_cue"), 0.9f);

		const bool bIce = (HazardType == EHoldoverHazardType::Ice);
		const FVector LaunchDir = bIce
			? (Character->GetActorForwardVector() * 0.5f + FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), 0.15f)).GetSafeNormal()
			: (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
		const float Strength = bIce ? IceStumbleStrength : JetBlastLaunchStrength;
		Character->LaunchCharacter(LaunchDir * Strength, true, true);
	}

	if (!HasAuthority())
	{
		return;
	}

	if (HazardType == EHoldoverHazardType::Ice)
	{
		// MP-2: speed changes go through the character's replicated speed state.
		if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Character))
		{
			HC->SetIceSlowed(true, IceSpeedMultiplier);
		}

		// DES-3: brief physical stumble — a small stumble launch so ice is felt,
		// not just a silent speed number.
		const FVector StumbleDir = Character->GetActorForwardVector() * 0.5f
			+ FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), 0.15f);
		Character->LaunchCharacter(StumbleDir.GetSafeNormal() * IceStumbleStrength, true, true);

		if (AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			GS->DispatchEvent(EDispatchEvent::IceStumble);
		}
	}
	else
	{
		// DES-3: per-player cooldown so the same player isn't fined every frame.
		const float Now = GetWorld()->GetTimeSeconds();
		if (const float* LastTime = JetBlastCooldownMap.Find(Character))
		{
			if (Now - *LastTime < JetBlastCooldown)
			{
				// Still launch (the blast is real) but skip the penalty.
				const FVector LaunchDir = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
				Character->LaunchCharacter(LaunchDir * JetBlastLaunchStrength, true, true);
				return;
			}
		}
		JetBlastCooldownMap.Add(Character, Now);

		const FVector LaunchDir = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
		Character->LaunchCharacter(LaunchDir * JetBlastLaunchStrength, true, true);

		// Attribute the penalty to the player who got blasted.
		AHoldoverPlayerState* PS = nullptr;
		if (const AController* C = Character->GetController())
		{
			PS = C->GetPlayerState<AHoldoverPlayerState>();
		}

		if (AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			GS->RegisterPenalty(JetBlastPenalty, TEXT("Jet blast incident"), PS, EDispatchEvent::JetBlast);
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

	if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Character))
	{
		HC->SetIceSlowed(false, IceSpeedMultiplier);
	}
}

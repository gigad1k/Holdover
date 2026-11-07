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
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		HandleBeginOverlap(Character);
	}
}

void AHoldoverHazardVolume::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		HandleEndOverlap(Character);
	}
}

void AHoldoverHazardVolume::HandleBeginOverlap(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// Local-only collision cue - the sound stays local in every seat (listen
	// host included).
	if (Character->IsLocallyControlled())
	{
		UHoldoverAudioLibrary::Play2D(this, FName("collision_cue"), 0.9f);
	}

	// M6: client-predicted launch only on a true client. On a listen host the
	// authority launch below already runs in this frame; predicting too
	// launched twice (each with its own direction, the second overwriting the
	// first). Directions are deterministic so a true client's prediction
	// matches the server's launch exactly.
	if (ShouldPredictLaunch(HasAuthority(), Character->IsLocallyControlled()))
	{
		const bool bIce = (HazardType == EHoldoverHazardType::Ice);
		const FVector LaunchDir = bIce
			? ComputeStumbleDirection(Character->GetActorForwardVector())
			: ComputeJetBlastDirection(GetActorLocation(), Character->GetActorLocation());
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

		// DES-3: brief physical stumble - a small stumble launch so ice is felt,
		// not just a silent speed number. Deterministic (M6).
		Character->LaunchCharacter(
			ComputeStumbleDirection(Character->GetActorForwardVector()) * IceStumbleStrength, true, true);

		if (AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			GS->DispatchEvent(EDispatchEvent::IceStumble);
		}
	}
	else
	{
		// DES-3: per-player cooldown so the same player isn't fined every frame.
		const float Now = GetWorld()->GetTimeSeconds();
		const FVector BlastDir = ComputeJetBlastDirection(GetActorLocation(), Character->GetActorLocation());
		if (const float* LastTime = JetBlastCooldownMap.Find(Character))
		{
			if (Now - *LastTime < JetBlastCooldown)
			{
				// Still launch (the blast is real) but skip the penalty.
				Character->LaunchCharacter(BlastDir * JetBlastLaunchStrength, true, true);
				return;
			}
		}
		// Sonnet M7: prune stale weak keys so destroyed/respawned characters do
		// not accumulate dead entries across a session.
		for (auto It = JetBlastCooldownMap.CreateIterator(); It; ++It)
		{
			if (!It->Key.IsValid())
			{
				It.RemoveCurrent();
			}
		}
		JetBlastCooldownMap.Add(Character, Now);

		Character->LaunchCharacter(BlastDir * JetBlastLaunchStrength, true, true);

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

void AHoldoverHazardVolume::HandleEndOverlap(ACharacter* Character)
{
	if (!Character || !HasAuthority() || HazardType != EHoldoverHazardType::Ice)
	{
		return;
	}

	if (AHoldoverCharacter* HC = Cast<AHoldoverCharacter>(Character))
	{
		HC->SetIceSlowed(false, IceSpeedMultiplier);
	}
}

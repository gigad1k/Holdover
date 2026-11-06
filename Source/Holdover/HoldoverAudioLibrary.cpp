// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverAudioLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Holdover.h"
#include "HoldoverAudioSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

bool UHoldoverAudioLibrary::ShouldPlayAudio(ENetMode NetMode)
{
	// M14: a dedicated server has no listener - loading and playing there only
	// wasted a LoadObject and logged the MISSING warning.
	return NetMode != NM_DedicatedServer;
}

USoundWave* UHoldoverAudioLibrary::LoadSound(UObject* WorldContextObject, FName Name)
{
	// M14: cache through the subsystem when a GameInstance exists (all real
	// game/PIE contexts). Bare automation worlds fall back to a direct load.
	if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		if (UHoldoverAudioSubsystem* Audio = GameInstance->GetSubsystem<UHoldoverAudioSubsystem>())
		{
			return Audio->GetSound(Name);
		}
	}

	const FString Path = FString::Printf(TEXT("/Game/Holdover/Audio/%s.%s"), *Name.ToString(), *Name.ToString());
	USoundWave* Sound = LoadObject<USoundWave>(nullptr, *Path);
	if (!Sound)
	{
		UE_LOG(LogHoldover, Warning, TEXT("PLACEHOLDER AUDIO MISSING: %s (run Tools/build_placeholder_audio.py)"), *Path);
	}
	return Sound;
}

UAudioComponent* UHoldoverAudioLibrary::Play2D(UObject* WorldContextObject, FName Name, float VolumeMultiplier, bool bLooping)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (World && !ShouldPlayAudio(World->GetNetMode()))
	{
		return nullptr;
	}

	USoundWave* Sound = LoadSound(WorldContextObject, Name);
	if (!Sound || !WorldContextObject)
	{
		return nullptr;
	}
	if (bLooping && !Sound->IsLooping())
	{
		UE_LOG(LogHoldover, Warning, TEXT("Play2D: %s requested looping but the asset is not flagged looping (see Tools/build_placeholder_audio.py)"), *Name.ToString());
	}
	// SpawnSound2D has no loop parameter - looping comes from USoundWave::bLooping.
	return UGameplayStatics::SpawnSound2D(WorldContextObject, Sound, VolumeMultiplier, 1.f, 0.f, nullptr, false, true);
}

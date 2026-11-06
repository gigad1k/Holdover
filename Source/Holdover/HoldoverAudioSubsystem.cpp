// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverAudioSubsystem.h"
#include "Holdover.h"
#include "Sound/SoundWave.h"

USoundWave* UHoldoverAudioSubsystem::GetSound(FName Name)
{
	if (const TObjectPtr<USoundWave>* Found = CachedSounds.Find(Name))
	{
		return Found->Get();
	}

	const FString Path = FString::Printf(TEXT("/Game/Holdover/Audio/%s.%s"), *Name.ToString(), *Name.ToString());
	USoundWave* Sound = LoadObject<USoundWave>(nullptr, *Path);
	if (!Sound)
	{
		UE_LOG(LogHoldover, Warning, TEXT("PLACEHOLDER AUDIO MISSING: %s (run Tools/build_placeholder_audio.py)"), *Path);
	}
	CachedSounds.Add(Name, Sound);
	return Sound;
}

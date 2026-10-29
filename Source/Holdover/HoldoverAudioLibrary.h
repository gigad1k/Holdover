// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HoldoverAudioLibrary.generated.h"

class USoundWave;
class UAudioComponent;

/**
 * Placeholder audio helper. Loads named sound waves from /Game/Holdover/Audio
 * and plays 2D cues. Missing assets log an obvious warning instead of failing
 * silently (playbook rule for placeholder audio).
 */
UCLASS()
class HOLDOVER_API UHoldoverAudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the sound wave at /Game/Holdover/Audio/<Name>, or null with a loud warning. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	static USoundWave* LoadSound(UObject* WorldContextObject, FName Name);

	/** Plays a 2D cue. Returns the audio component (null when the asset is missing). */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	static UAudioComponent* Play2D(UObject* WorldContextObject, FName Name, float VolumeMultiplier = 1.f, bool bLooping = false);
};

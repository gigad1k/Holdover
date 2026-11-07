// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HoldoverAudioSubsystem.generated.h"

class USoundWave;

/**
 * Sonnet M14: holds the placeholder sound waves so Play2D stops doing a
 * LoadObject by string path on every call (a disk/asset-registry hitch on
 * first use, a MISSING warning on every call, and a GC hole - nothing held the
 * wave between plays). Misses are cached too, so a missing asset warns once.
 */
UCLASS()
class UHoldoverAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Cached load of /Game/Holdover/Audio/<Name> (null = cached miss). */
	USoundWave* GetSound(FName Name);

	/** Cached entries so far (hits and misses alike; test/telemetry hook). */
	int32 GetCachedSoundCount() const { return CachedSounds.Num(); }

protected:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundWave>> CachedSounds;
};

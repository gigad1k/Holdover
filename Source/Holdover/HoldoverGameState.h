// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HoldoverGameState.generated.h"

/**
 * Server-authoritative shift-wide state. Weather severity drives protection
 * degradation and holdover burn (see AHoldoverAircraft::SimulateTreatment).
 */
UCLASS()
class HOLDOVER_API AHoldoverGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHoldoverGameState();

	/** 0 = calm, 1 = worst. Server writes; all clients read. */
	UPROPERTY(ReplicatedUsing = OnRep_Weather, BlueprintReadOnly, Category = "Weather")
	float WeatherSeverity = 0.f;

	/** While true the server slowly ramps severity (greybox demo behaviour). */
	UPROPERTY(EditDefaultsOnly, Category = "Weather")
	bool bAutoRampWeather = true;

	UPROPERTY(EditDefaultsOnly, Category = "Weather")
	float WeatherRampPerSecond = 0.01f;

	/** Server-only setter. */
	void SetWeatherSeverity(float NewSeverity);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UFUNCTION()
	void OnRep_Weather();
};

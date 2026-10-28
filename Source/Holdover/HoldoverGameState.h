// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HoldoverGameState.generated.h"

UENUM(BlueprintType)
enum class EShiftState : uint8
{
	WaitingToStart	UMETA(DisplayName = "Waiting To Start"),
	InProgress		UMETA(DisplayName = "In Progress"),
	ShiftEnded		UMETA(DisplayName = "Shift Ended")
};

/**
 * Server-authoritative shift-wide state: weather, shift clock, quota, pay and
 * the dispatcher feed. All updates go through the deterministic methods below.
 */
UCLASS()
class HOLDOVER_API AHoldoverGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHoldoverGameState();

	// --- weather ---
	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Weather")
	float WeatherSeverity = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weather")
	bool bAutoRampWeather = true;

	UPROPERTY(EditDefaultsOnly, Category = "Weather")
	float WeatherRampPerSecond = 0.01f;

	void SetWeatherSeverity(float NewSeverity);

	// --- shift ---
	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	EShiftState ShiftState = EShiftState::WaitingToStart;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	float ShiftTimeRemaining = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	int32 Quota = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	int32 OnTimeDepartures = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	int32 LateDepartures = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	int32 Money = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	int32 Deductions = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Shift, BlueprintReadOnly, Category = "Shift")
	bool bShiftPassed = false;

	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	int32 PayPerOnTimeDeparture = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Shift")
	int32 LateDepartureDeduction = 50;

	// --- dispatcher feed ---
	UPROPERTY(ReplicatedUsing = OnRep_Dispatch, BlueprintReadOnly, Category = "Dispatch")
	TArray<FText> DispatchFeed;

	// --- deterministic shift API (server; tests call directly) ---
	void StartShift(float DurationSeconds, int32 InQuota);
	void TickShift(float DeltaSeconds);
	void EndShift();

	/** Starts a fresh shift with the previous duration/quota (repeatable loop). */
	void RestartShift();

	void RegisterDeparture(bool bOnTime);
	void RegisterPenalty(int32 Amount, const FString& Reason);
	void AddDispatch(const FString& Line);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UFUNCTION()
	void OnRep_Shift();

	UFUNCTION()
	void OnRep_Dispatch();

	float LastShiftDuration = 120.f;
};

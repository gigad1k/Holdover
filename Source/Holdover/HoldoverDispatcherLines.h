// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HoldoverDispatcherLines.generated.h"

/**
 * DES-3: reactive dispatcher line bank keyed by event + repetition + blame.
 * Lines are content, not code — this is the greybox bank; DES-5 moves it to a
 * data table. The voice is the RESTRUCTURED crew: underqualified, overconfident,
 * and always on the radio.
 */
UENUM(BlueprintType)
enum class EDispatchEvent : uint8
{
	ShiftStart			UMETA(DisplayName = "Shift Start"),
	AircraftArrival		UMETA(DisplayName = "Aircraft Arrival"),
	TreatmentComplete	UMETA(DisplayName = "Treatment Complete"),
	HoldoverStarted		UMETA(DisplayName = "Holdover Started"),
	HoldoverWarning		UMETA(DisplayName = "Holdover Warning (<30s)"),
	HoldoverExpired		UMETA(DisplayName = "Holdover Expired"),
	DepartureOnTime		UMETA(DisplayName = "Departure On-Time"),
	DepartureLate		UMETA(DisplayName = "Departure Late"),
	IceStumble			UMETA(DisplayName = "Ice Stumble"),
	JetBlast			UMETA(DisplayName = "Jet Blast"),
	RefillComplete		UMETA(DisplayName = "Refill Complete"),
	TankEmpty			UMETA(DisplayName = "Tank Empty"),
	ShiftPassed			UMETA(DisplayName = "Shift Passed"),
	ShiftFailed			UMETA(DisplayName = "Shift Failed"),
	NewShift			UMETA(DisplayName = "New Shift"),
	PlayerConnected		UMETA(DisplayName = "Player Connected"),
	PlayerDisconnected	UMETA(DisplayName = "Player Disconnected")
};

namespace HoldoverDispatch
{
	/**
	 * Returns a reactive line for the given event. RepetitionCount selects the
	 * tier (0 = first time, 1 = second, ...); once tiers are exhausted the last
	 * tier is cycled. PlayerName is injected where the line has a {player} slot.
	 */
	FString GetLine(EDispatchEvent Event, int32 RepetitionCount, const FString& PlayerName);
}

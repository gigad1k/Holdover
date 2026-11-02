// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverShiftProfiles.h"

namespace
{
	UHoldoverShiftData* MakeProfile(UObject* Outer,
		float Duration, int32 Quota,
		int32 Pay, int32 LatePenalty, int32 AbortPenalty,
		float WMin, float WMax, float WAmplitude, float WPeriod,
		float HoldoverDur, float BurnMult, float DegRate, float DegScale,
		float ContamRate, float ContamThreshold,
		float Deadline)
	{
		UHoldoverShiftData* P = NewObject<UHoldoverShiftData>(Outer);
		P->ShiftDuration = Duration;
		P->ShiftQuota = Quota;
		P->PayPerOnTimeDeparture = Pay;
		P->LateDepartureDeduction = LatePenalty;
		P->HoldoverAbortPenalty = AbortPenalty;
		P->WeatherSeverityMin = WMin;
		P->WeatherSeverityMax = WMax;
		P->WeatherAmplitude = WAmplitude;
		P->WeatherPeriodSeconds = WPeriod;
		P->HoldoverDuration = HoldoverDur;
		P->WeatherBurnMultiplier = BurnMult;
		P->ProtectionDegradationPerSecond = DegRate;
		P->HoldoverDegradationScale = DegScale;
		P->ReContaminationRatePerSecond = ContamRate;
		P->ReContaminationProtectionThreshold = ContamThreshold;
		P->PerAircraftDeadlineSeconds = Deadline;
		return P;
	}
}

TArray<UHoldoverShiftData*> HoldoverShiftProfiles::CreateDefaultProfiles(UObject* Outer)
{
	TArray<UHoldoverShiftData*> Profiles;

	// Night 1 — orientation. Gentle weather, generous deadlines.
	Profiles.Add(MakeProfile(Outer,
		480.f, 2,        // 8 min, quota 2
		100, 50, 15,     // pay $100, late -$50, abort -$15
		0.1f, 0.3f, 0.15f, 100.f,  // weather: mild
		120.f, 2.f, 2.f, 0.f,      // holdover: 2 min, standard burn
		2.f, 60.f,                  // contamination: low
		300.f));                    // deadline: 5 min

	// Night 2 — tightening. Weather picks up, deadlines shorten.
	Profiles.Add(MakeProfile(Outer,
		480.f, 2,
		100, 50, 20,
		0.15f, 0.4f, 0.2f, 90.f,
		110.f, 2.2f, 2.5f, 0.f,
		2.5f, 55.f,
		270.f));

	// Night 3 — pressure. Two competing clocks, faster weather.
	Profiles.Add(MakeProfile(Outer,
		480.f, 3,
		100, 50, 25,
		0.2f, 0.5f, 0.25f, 80.f,
		100.f, 2.5f, 3.f, 0.f,
		3.f, 50.f,
		240.f));

	// Night 4 — hard. Short holdover, aggressive weather.
	Profiles.Add(MakeProfile(Outer,
		420.f, 3,
		100, 75, 30,
		0.25f, 0.6f, 0.3f, 70.f,
		90.f, 3.f, 3.5f, 0.f,
		3.5f, 45.f,
		210.f));

	// Night 5 — blizzard. Maximum chaos.
	Profiles.Add(MakeProfile(Outer,
		420.f, 4,
		100, 75, 40,
		0.3f, 0.7f, 0.35f, 60.f,
		80.f, 3.5f, 4.f, 0.25f,  // degradation resumes during holdover
		4.f, 40.f,
		180.f));

	return Profiles;
}

FString HoldoverShiftProfiles::GetProfileName(int32 ShiftNumber)
{
	switch (FMath::Clamp(ShiftNumber, 1, 5))
	{
	case 1: return TEXT("Night 1 — Orientation");
	case 2: return TEXT("Night 2 — Tightening");
	case 3: return TEXT("Night 3 — Pressure");
	case 4: return TEXT("Night 4 — Hard");
	case 5: return TEXT("Night 5 — Blizzard");
	default: return TEXT("Night ?");
	}
}

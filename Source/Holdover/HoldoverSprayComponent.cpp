// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverSprayComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Holdover.h"
#include "HoldoverAircraft.h"
#include "HoldoverAudioLibrary.h"
#include "HoldoverCharacter.h"
#include "HoldoverSurfaceSection.h"
#include "HoldoverTestInteractable.h"
#include "Net/UnrealNetwork.h"

UHoldoverSprayComponent::UHoldoverSprayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UHoldoverSprayComponent::SetSpraying(bool bEnabled)
{
	bSpraying = bEnabled;
	RequestAccumulator = 0.f;

	// Cosmetic spray hiss on the owning client only. The stop is unconditional so
	// server-side stops are harmless no-ops and owner-side stops always land.
	if (bEnabled)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !HissComponent)
		{
			HissComponent = UHoldoverAudioLibrary::Play2D(GetOwner(), FName("spray_hiss"), 0.9f, true);
			UE_LOG(LogHoldover, Display, TEXT("spray hiss start (%s)"), *GetNameSafe(GetOwner()));
		}
	}
	else if (HissComponent)
	{
		HissComponent->Stop();
		HissComponent = nullptr;
		UE_LOG(LogHoldover, Display, TEXT("spray hiss stop (%s)"), *GetNameSafe(GetOwner()));
	}
}

void UHoldoverSprayComponent::BeginRefill(EHoldoverFluidType NewType)
{
	// Owner sends the request; the server validates and starts the timed refill.
	ServerBeginRefill(NewType);
}

float UHoldoverSprayComponent::GetRefillProgress() const
{
	const float Duration = GetRefillDuration();
	if (!bRefilling || Duration <= 0.f)
	{
		return bRefilling ? 1.f : 0.f;
	}
	return 1.f - (RefillTimeRemaining / Duration);
}

bool UHoldoverSprayComponent::ApplySpray(AHoldoverSurfaceSection* Target, float DeltaSeconds)
{
	if (!IsValid(Target) || DeltaSeconds <= 0.f)
	{
		return false;
	}

	if (CarriedFluidType == EHoldoverFluidType::TypeI)
	{
		if (FluidLevel <= 0.f)
		{
			return false;
		}
		Target->ApplyCleaning(GetCleanRate() * DeltaSeconds);
		FluidLevel = FMath::Max(0.f, FluidLevel - GetTypeIUseRate() * DeltaSeconds);
		return true;
	}

	// Type IV: wrong fluid on a dirty section — rejected by rule.
	if (!Target->IsClean())
	{
		UE_LOG(LogHoldover, Verbose, TEXT("Type IV rejected (section not clean): %s"), *Target->GetName());
		return false;
	}
	if (FluidLevel <= 0.f)
	{
		return false;
	}
	Target->ApplyProtection(GetProtectRate() * DeltaSeconds);
	FluidLevel = FMath::Max(0.f, FluidLevel - GetTypeIVUseRate() * DeltaSeconds);
	return true;
}

void UHoldoverSprayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Timed refill countdown (server-driven).
	if (bRefilling && GetOwner() && GetOwner()->HasAuthority())
	{
		RefillTimeRemaining -= DeltaTime;
		if (RefillTimeRemaining <= 0.f)
		{
			RefillTimeRemaining = 0.f;
			bRefilling = false;
			FluidLevel = GetFluidCapacity();
			UE_LOG(LogHoldover, Display, TEXT("Refill complete (%s) -> %s, %.0f"),
				*GetNameSafe(GetOwner()),
				CarriedFluidType == EHoldoverFluidType::TypeI ? TEXT("Type I") : TEXT("Type IV"),
				FluidLevel);
		}
	}

	const AActor* Owner = GetOwner();
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		// Only the owning client (incl. listen host) drives spray requests.
		return;
	}

	if (!bSpraying)
	{
		CurrentTargetSection.Reset();
		TargetHint = FText::GetEmpty();
		return;
	}

	UpdateTargetFromTrace();

	RequestAccumulator += DeltaTime;
	if (RequestAccumulator >= SprayRequestInterval)
	{
		if (AHoldoverSurfaceSection* Target = CurrentTargetSection.Get())
		{
			// On the listen host this calls the implementation directly; on clients it is an RPC.
			ServerSpray(Target, RequestAccumulator);
		}
		RequestAccumulator = 0.f;
	}
}

void UHoldoverSprayComponent::UpdateTargetFromTrace()
{
	CurrentTargetSection.Reset();
	TargetHint = FText::GetEmpty();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Owner);
	FVector Start = Owner->GetActorLocation();
	FRotator Aim = Owner->GetActorRotation();

	if (USceneComponent* Origin = SprayOrigin.Get())
	{
		// Truck nozzle / custom origin wins.
		Start = Origin->GetComponentLocation();
		Aim = Origin->GetComponentRotation();
	}
	else if (Character && Character->GetFirstPersonCameraComponent())
	{
		Start = Character->GetFirstPersonCameraComponent()->GetComponentLocation();
		Aim = Character->GetControlRotation();
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverSpray), false, Owner);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Aim.Vector() * GetSprayRange(), ECC_Visibility, Params))
	{
		if (AHoldoverSurfaceSection* Section = Cast<AHoldoverSurfaceSection>(Hit.GetActor()))
		{
			CurrentTargetSection = Section;

			// Cosmetic pre-validation hint (the server enforces the real rules).
			if (CarriedFluidType == EHoldoverFluidType::TypeIV && !Section->IsClean())
			{
				TargetHint = FText::FromString(TEXT("Section not clean - need Type I crew"));
			}
			else if (FluidLevel <= 0.f)
			{
				TargetHint = FText::FromString(TEXT("Tank empty - refill at station"));
			}
		}
	}
}

float UHoldoverSprayComponent::ClampSprayDt(float RequestedDt, float Interval)
{
	if (RequestedDt <= 0.f || Interval <= 0.f)
	{
		return 0.f;
	}
	return FMath::Min(RequestedDt, Interval * 2.f);
}

void UHoldoverSprayComponent::ServerSpray_Implementation(AHoldoverSurfaceSection* Target, float DeltaSeconds)
{
	ServerApplySprayRequest(Target, DeltaSeconds);
}

bool UHoldoverSprayComponent::ServerApplySprayRequest(AHoldoverSurfaceSection* Target, float DeltaSeconds, double NowSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner || !IsValid(Target))
	{
		return false;
	}

	// Sonnet M17: sections of a not-yet-arrived or already-departed aircraft
	// must not be sprayable (a modified client can name any target).
	if (const AHoldoverSurfaceSection* TargetSection = Cast<AHoldoverSurfaceSection>(Target))
	{
		if (const AHoldoverAircraft* Aircraft = TargetSection->OwningAircraft.Get())
		{
			if (!Aircraft->bArrived || Aircraft->bDeparted)
			{
				UE_LOG(LogHoldover, Warning, TEXT("Spray rejected (aircraft not live): %s -> %s"), *Owner->GetName(), *Target->GetName());
				return false;
			}
		}
	}

	// MP-8: never trust client dt — one RPC with dt=1000 must not clean a section.
	const float Dt = ClampSprayDt(DeltaSeconds, SprayRequestInterval);
	if (Dt <= 0.f)
	{
		return false;
	}

	// MP-8: measure from the active spray origin (truck nozzle / camera), not the
	// owner root — full-extension truck sprays are legitimate.
	FVector Origin = Owner->GetActorLocation();
	if (USceneComponent* OriginComp = SprayOrigin.Get())
	{
		Origin = OriginComp->GetComponentLocation();
	}
	else if (const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Owner))
	{
		if (Character->GetFirstPersonCameraComponent())
		{
			Origin = Character->GetFirstPersonCameraComponent()->GetComponentLocation();
		}
	}

	if (FVector::DistSquared(Origin, Target->GetActorLocation()) > FMath::Square(GetSprayRange() * 2.f))
	{
		UE_LOG(LogHoldover, Warning, TEXT("Spray rejected (out of range): %s -> %s"), *Owner->GetName(), *Target->GetName());
		return false;
	}

	// G-3: server-side line of sight — a modified client must not spray through
	// walls. Same channel the owning client's targeting trace uses, so a target
	// the client could see always passes here.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverSprayLOS), false, Owner);
	Params.AddIgnoredActor(Target);
	// Sonnet M2: surface sections are separate BlockAll actors — a section whose
	// centre sits behind a sibling panel (tail behind fuselage) is legitimately
	// hittable, so the target's sibling panels must not block the server trace.
	if (const AHoldoverSurfaceSection* M2TargetSection = Cast<AHoldoverSurfaceSection>(Target))
	{
		if (const AHoldoverAircraft* M2Aircraft = M2TargetSection->OwningAircraft.Get())
		{
			for (const AHoldoverSurfaceSection* Sibling : M2Aircraft->GetSections())
			{
				if (Sibling && Sibling != Target)
				{
					Params.AddIgnoredActor(Sibling);
				}
			}
		}
	}
	if (Owner->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Target->GetActorLocation(), ECC_Visibility, Params))
	{
		UE_LOG(LogHoldover, Warning, TEXT("Spray rejected (no line of sight): %s -> %s"), *Owner->GetName(), *Target->GetName());
		return false;
	}

	// Sonnet M1: token bucket — spray time accrues at 1x real time, banked up to
	// 2x the request interval. dt is already clamped per request; this bounds
	// REQUESTS per second, so a 1000 RPC/s flood cannot overspeed treatment.
	const double Now = NowSeconds >= 0.0 ? NowSeconds : (GetWorld() ? (double)GetWorld()->GetTimeSeconds() : 0.0);
	const float Burst = SprayRequestInterval * 2.f;
	if (LastSprayRateTime < 0.0)
	{
		SprayTimeAllowance = Burst;
	}
	else
	{
		const float Elapsed = (float)FMath::Clamp<double>(Now - LastSprayRateTime, 0.0, (double)Burst);
		SprayTimeAllowance = FMath::Min(SprayTimeAllowance + Elapsed, Burst);
	}
	LastSprayRateTime = Now;
	const float FinalDt = FMath::Min(Dt, SprayTimeAllowance);
	if (FinalDt <= 0.f)
	{
		UE_LOG(LogHoldover, Verbose, TEXT("Spray rejected (rate limit): %s -> %s"), *Owner->GetName(), *Target->GetName());
		return false;
	}
	SprayTimeAllowance -= FinalDt;

	return ApplySpray(Target, FinalDt);
}

void UHoldoverSprayComponent::ServerBeginRefill_Implementation(EHoldoverFluidType NewType)
{
	ValidateAndBeginRefill(NewType);
}

bool UHoldoverSprayComponent::ValidateAndBeginRefill(EHoldoverFluidType NewType)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetWorld())
	{
		return false;
	}

	// G-1: only two fluid types exist; a garbage enum must not enter replicated state.
	if (NewType != EHoldoverFluidType::TypeI && NewType != EHoldoverFluidType::TypeIV)
	{
		UE_LOG(LogHoldover, Warning, TEXT("Refill rejected (invalid fluid type): %s"), *GetNameSafe(Owner));
		return false;
	}

	// G-1: a refill in progress cannot be restarted (would refresh the tank early).
	if (bRefilling)
	{
		UE_LOG(LogHoldover, Warning, TEXT("Refill rejected (already refilling): %s"), *GetNameSafe(Owner));
		return false;
	}

	// G-1: the RPC is client-callable, so station proximity is validated here,
	// not only on the interact path. Same slack as ServerInteract's range check.
	// N7: cache the station scan (refresh when empty or stale).
	const double Now = (double)GetWorld()->GetTimeSeconds();
	if (CachedRefillStations.Num() == 0 || Now - LastStationScanTime > StationRescanSeconds)
	{
		CachedRefillStations.Reset();
		for (TActorIterator<AHoldoverTestInteractable> It(Owner->GetWorld()); It; ++It)
		{
			CachedRefillStations.Add(*It);
		}
		LastStationScanTime = Now;
	}
	bool bNearStation = false;
	for (const TWeakObjectPtr<AActor>& Station : CachedRefillStations)
	{
		if (Station.IsValid()
			&& FVector::DistSquared(Owner->GetActorLocation(), Station->GetActorLocation()) <= FMath::Square(RefillStationRange))
		{
			bNearStation = true;
			break;
		}
	}
	if (!bNearStation)
	{
		UE_LOG(LogHoldover, Warning, TEXT("Refill rejected (not at a refill station): %s"), *GetNameSafe(Owner));
		return false;
	}

	// DES-2: one-tank rule — choosing a fluid at the station replaces whatever
	// you were carrying. The refill takes RefillDuration seconds.
	const float Duration = GetRefillDuration();
	CarriedFluidType = NewType;
	FluidLevel = 0.f;
	bRefilling = true;
	RefillTimeRemaining = Duration;
	UE_LOG(LogHoldover, Display, TEXT("Refill started (%s) -> %s, %.1fs"),
		*GetNameSafe(Owner),
		NewType == EHoldoverFluidType::TypeI ? TEXT("Type I") : TEXT("Type IV"),
		Duration);

	// Sonnet M10: mirror the fluid choice onto the player's PlayerState so the
	// results screen shows the right tank tag instead of a stale default.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* Controller = Pawn->GetController())
		{
			if (AHoldoverPlayerState* PS = Controller->GetPlayerState<AHoldoverPlayerState>())
			{
				PS->SetCarriedFluid(NewType);
			}
		}
	}
	return true;
}

void UHoldoverSprayComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoldoverSprayComponent, FluidLevel);
	DOREPLIFETIME(UHoldoverSprayComponent, CarriedFluidType);
	DOREPLIFETIME(UHoldoverSprayComponent, bRefilling);
	// Sonnet M3: only the owner's HUD shows the refill countdown - stop sending
	// this float to every client every server tick.
	DOREPLIFETIME_CONDITION(UHoldoverSprayComponent, RefillTimeRemaining, COND_OwnerOnly);
}

void UHoldoverSprayComponent::OnRep_Fluid()
{
	// HUD reads the tank level and fluid type directly.
}

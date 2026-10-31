// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverInputToolset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HoldoverAircraft.h"
#include "HoldoverDeicingTruck.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"
#include "InputCoreTypes.h"

namespace
{
	UWorld* GetPIEWorld(int32 PIEInstance)
	{
		if (!GEngine)
		{
			return nullptr;
		}
		const FWorldContext* Ctx = GEngine->GetWorldContextFromPIEInstance(PIEInstance);
		return Ctx ? Ctx->World() : nullptr;
	}

	APlayerController* GetPIEPlayer(int32 PIEInstance)
	{
		UWorld* World = GetPIEWorld(PIEInstance);
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

FString UHoldoverInputToolset::InjectKey(int32 PIEInstance, const FString& KeyName, bool bDown)
{
	APlayerController* PC = GetPIEPlayer(PIEInstance);
	if (!PC || !PC->PlayerInput)
	{
		return FString::Printf(TEXT("ERROR: no player input for PIE instance %d"), PIEInstance);
	}

	const FKey Key = FKey(FName(*KeyName));
	if (!Key.IsValid())
	{
		return FString::Printf(TEXT("ERROR: unknown key '%s'"), *KeyName);
	}

	// Through PlayerInput directly so UInputSettings::bFilterInputByPlatformUser
	// does not drop the synthetic event (see APlayerController::InputKey).
	FInputKeyEventArgs Args(
		/*Viewport*/ nullptr,
		FInputDeviceId::CreateFromInternalId(0),
		Key,
		bDown ? IE_Pressed : IE_Released,
		/*EventTimestamp*/ 0u);

	const bool bHandled = PC->PlayerInput->InputKey(Args);
	return FString::Printf(TEXT("OK: %s %s on PIE %d (handled=%s)"),
		*KeyName, bDown ? TEXT("down") : TEXT("up"), PIEInstance,
		bHandled ? TEXT("true") : TEXT("false"));
}

FString UHoldoverInputToolset::InjectMouseDelta(int32 PIEInstance, float DeltaX, float DeltaY)
{
	APlayerController* PC = GetPIEPlayer(PIEInstance);
	if (!PC || !PC->PlayerInput)
	{
		return FString::Printf(TEXT("ERROR: no player input for PIE instance %d"), PIEInstance);
	}

	const FInputDeviceId Device = FInputDeviceId::CreateFromInternalId(0);
	const uint64 Now = FPlatformTime::Cycles64();
	if (FMath::Abs(DeltaX) > KINDA_SMALL_NUMBER)
	{
		FInputKeyEventArgs Args(nullptr, Device, EKeys::MouseX, DeltaX, /*DeltaTime*/ 0.016f, /*NumSamples*/ 1, Now);
		PC->PlayerInput->InputKey(Args);
	}
	if (FMath::Abs(DeltaY) > KINDA_SMALL_NUMBER)
	{
		FInputKeyEventArgs Args(nullptr, Device, EKeys::MouseY, DeltaY, /*DeltaTime*/ 0.016f, /*NumSamples*/ 1, Now);
		PC->PlayerInput->InputKey(Args);
	}
	return FString::Printf(TEXT("OK: mouse delta (%.2f, %.2f) on PIE %d"), DeltaX, DeltaY, PIEInstance);
}

FString UHoldoverInputToolset::ExecConsole(int32 PIEInstance, const FString& Command)
{
	UWorld* World = GetPIEWorld(PIEInstance);
	if (!World || !GEngine)
	{
		return FString::Printf(TEXT("ERROR: no PIE world for instance %d"), PIEInstance);
	}
	GEngine->Exec(World, *Command);
	return FString::Printf(TEXT("OK: '%s' executed in PIE %d"), *Command, PIEInstance);
}

FString UHoldoverInputToolset::TeleportPlayer(int32 PIEInstance, float X, float Y, float Z, float Yaw, float Pitch)
{
	// Authority: in a listen-server PIE every pawn is owned by the server world
	// (instance 0), so teleport the instance-th player there — a client-side
	// SetActorLocation gets reverted by server movement correction.
	APawn* Pawn = nullptr;
	if (UWorld* ServerWorld = GetPIEWorld(0))
	{
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			if (Index == PIEInstance)
			{
				Pawn = It->Get()->GetPawn();
				break;
			}
		}
	}
	if (!Pawn)
	{
		if (APlayerController* FallbackPC = GetPIEPlayer(PIEInstance))
		{
			Pawn = FallbackPC->GetPawn();
		}
	}
	if (!Pawn)
	{
		return FString::Printf(TEXT("ERROR: no pawn for PIE instance %d"), PIEInstance);
	}

	Pawn->TeleportTo(FVector(X, Y, Z), FRotator(0.f, Yaw, 0.f), false, true);
	if (APlayerController* LocalPC = GetPIEPlayer(PIEInstance))
	{
		LocalPC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	}
	return FString::Printf(TEXT("OK: PIE %d pawn -> (%.0f, %.0f, %.0f) yaw %.0f pitch %.0f (%s)"),
		PIEInstance, X, Y, Z, Yaw, Pitch, *Pawn->GetName());
}

FString UHoldoverInputToolset::GetPawnInfo(int32 PIEInstance)
{
	APlayerController* PC = GetPIEPlayer(PIEInstance);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return FString::Printf(TEXT("ERROR: no pawn for PIE instance %d"), PIEInstance);
	}
	const FVector Loc = Pawn->GetActorLocation();
	const float Speed = Pawn->GetVelocity().Size();
	float MaxWalkSpeed = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			MaxWalkSpeed = Move->MaxWalkSpeed;
		}
	}
	float TypeI = -1.f, TypeIV = -1.f;
	FString Fluid = TEXT("n/a");
	if (const UHoldoverSprayComponent* Spray = Pawn->FindComponentByClass<UHoldoverSprayComponent>())
	{
		TypeI = Spray->TypeIFluid;
		TypeIV = Spray->TypeIVFluid;
		Fluid = Spray->ActiveFluidType == EHoldoverFluidType::TypeI ? TEXT("TypeI") : TEXT("TypeIV");
	}
	return FString::Printf(
		TEXT("{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"speed\":%.1f,\"maxWalkSpeed\":%.1f,\"typeI\":%.1f,\"typeIV\":%.1f,\"fluid\":\"%s\"}"),
		Loc.X, Loc.Y, Loc.Z, Speed, MaxWalkSpeed, TypeI, TypeIV, *Fluid);
}

FString UHoldoverInputToolset::ForceTreatment(float CleanPercent, float ProtectPercent)
{
	UWorld* World = GetPIEWorld(0);
	if (!World)
	{
		return TEXT("ERROR: no server world");
	}
	int32 Count = 0;
	for (TActorIterator<AHoldoverSurfaceSection> It(World); It; ++It)
	{
		AHoldoverSurfaceSection* Section = *It;
		const float CleanAdd = FMath::Clamp(CleanPercent, 0.f, 100.f) - Section->CleanlinessPercent;
		const float ProtectAdd = FMath::Clamp(ProtectPercent, 0.f, 100.f) - Section->ProtectionPercent;
		if (CleanAdd > 0.f)
		{
			Section->ApplyCleaning(CleanAdd);
		}
		if (ProtectAdd > 0.f)
		{
			Section->ApplyProtection(ProtectAdd);
		}
		++Count;
	}
	return FString::Printf(TEXT("OK: %d sections set to clean %.0f protect %.0f"), Count, CleanPercent, ProtectPercent);
}

FString UHoldoverInputToolset::GetSectionStates(int32 PIEInstance)
{
	UWorld* World = GetPIEWorld(PIEInstance);
	if (!World)
	{
		return TEXT("ERROR: no server world");
	}
	FString Holdover = TEXT("none");
	float Remaining = 0.f;
	float AircraftZ = 0.f;
	FString Sections = TEXT("[");
	bool bFirst = true;
	for (TActorIterator<AHoldoverAircraft> It(World); It; ++It)
	{
		const AHoldoverAircraft* Aircraft = *It;
		switch (Aircraft->HoldoverState)
		{
		case EHoldoverState::HoldoverActive: Holdover = TEXT("HoldoverActive"); break;
		case EHoldoverState::HoldoverExpired: Holdover = TEXT("HoldoverExpired"); break;
		case EHoldoverState::Departed: Holdover = TEXT("Departed"); break;
		default: Holdover = TEXT("Incomplete"); break;
		}
		Remaining = Aircraft->HoldoverRemaining;
		AircraftZ = Aircraft->GetActorLocation().Z;
		for (const AHoldoverSurfaceSection* Section : Aircraft->GetSections())
		{
			if (!Section)
			{
				continue;
			}
			if (!bFirst)
			{
				Sections += TEXT(",");
			}
			bFirst = false;
			Sections += FString::Printf(TEXT("{\"name\":\"%s\",\"clean\":%.0f,\"prot\":%.0f}"),
				*Section->SectionName.ToString(), Section->CleanlinessPercent, Section->ProtectionPercent);
		}
		break;
	}
	Sections += TEXT("]");
	return FString::Printf(TEXT("{\"holdover\":\"%s\",\"remaining\":%.0f,\"aircraftZ\":%.0f,\"sections\":%s}"),
		*Holdover, Remaining, AircraftZ, *Sections);
}


FString UHoldoverInputToolset::GetTruckState(int32 PIEInstance)
{
	UWorld* World = GetPIEWorld(PIEInstance);
	if (!World)
	{
		return TEXT("ERROR: no world");
	}
	for (TActorIterator<AHoldoverDeicingTruck> It(World); It; ++It)
	{
		const AHoldoverDeicingTruck* Truck = *It;
		return FString::Printf(TEXT("{\"boomYaw\":%.0f,\"boomExt\":%.0f,\"occupied\":%s,\"occupant\":\"%s\"}"),
			Truck->BoomYaw, Truck->BoomExtension,
			Truck->IsOccupied() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Truck->GetOccupant()));
	}
	return TEXT("ERROR: no truck");
}
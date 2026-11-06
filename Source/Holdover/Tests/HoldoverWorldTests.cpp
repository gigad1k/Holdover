// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Holdover.h"
#include "HoldoverAircraft.h"
#include "HoldoverCharacter.h"
#include "HoldoverDeicingTruck.h"
#include "HoldoverGameState.h"
#include "HoldoverHazardVolume.h"
#include "HoldoverPanelWidgets.h"
#include "HoldoverPlayerState.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"
#include "HoldoverTestInteractable.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Tests/HoldoverTestFixtures.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// PROC-1: world-based multiplayer automation. Unlike the world-free unit tests
// in this folder, these spawn real actors into a real UWorld (authority,
// transforms, components) and drive the exact server-side request handlers a
// remote client reaches over RPC — `ServerApplySprayRequest`, `ApplyBoomRequest`,
// `Execute_Interact` — plus a replication contract that keeps the MP-1..MP-9
// fix properties registered for clients.
//
// NOTE: AHoldoverCharacter and AHoldoverGameMode are UCLASS(abstract) (the game
// spawns Blueprint subclasses), so tests use class default objects for the
// replication contract and plain AActor spray owners for behaviour.

namespace HoldoverWorldTest
{
	UWorld* CreateTestWorld()
	{
		return UWorld::CreateWorld(EWorldType::Game, false);
	}

	void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	template <typename T>
	T* Spawn(UWorld* World, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), FTransform(Location), Params);
	}

	// A spray-capable owner: plain actor + root + registered spray component.
	// Replaces the abstract AHoldoverCharacter as the origin actor in world tests.
	// The root matters: a bare AActor has none and GetActorLocation() lies (0,0,0).
	AActor* SpawnSprayOwner(UWorld* World, const FVector& Location, UHoldoverSprayComponent*& OutSpray)
	{
		AActor* Owner = Spawn<AActor>(World, Location);
		if (Owner)
		{
			USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"));
			Owner->SetRootComponent(Root);
			Root->RegisterComponent();
			Owner->SetActorLocation(Location);
			OutSpray = NewObject<UHoldoverSprayComponent>(Owner, TEXT("Spray Component"));
			OutSpray->RegisterComponent();
		}
		return Owner;
	}

	// A solid blocker between two points (G-3/G-4 LOS tests).
	AActor* SpawnBlocker(UWorld* World, const FVector& Location, float HalfExtent = 50.f)
	{
		AActor* Blocker = Spawn<AActor>(World, Location);
		if (Blocker)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Blocker, TEXT("Box"));
			Blocker->SetRootComponent(Box);
			Box->RegisterComponent();
			Box->SetBoxExtent(FVector(HalfExtent));
			Box->SetCollisionProfileName(FName(TEXT("BlockAll")));
			Box->SetWorldLocation(Location);
		}
		return Blocker;
	}

	template <typename T>
	T* CDO()
	{
		return Cast<T>(T::StaticClass()->GetDefaultObject());
	}

	// A property participates in replication when it has CPF_Net (set by the
	// UPROPERTY(Replicated/ReplicatedUsing) specifier). NOTE: direct enumeration
	// via GetLifetimeReplicatedProps is deliberately NOT done here — in UE 5.8 the
	// DOREPLIFETIME macro's AddUnique hits a FLifetimeProperty push-model assert
	// (CoreNet.h operator==) when called from automation on AHoldoverCharacter,
	// crashing the whole run. DOREPLIFETIME parity is verified by code audit
	// (Docs/MP_AUDIT_2026-11-02.md) and live PIE sessions instead.
	void TestReplicatedProperty(FAutomationTestBase& Test, UObject* Object, const TCHAR* PropName)
	{
		const FProperty* Prop = FindFProperty<FProperty>(Object->GetClass(), FName(PropName));
		if (!Prop)
		{
			Test.AddError(*FString::Printf(TEXT("%s has no property '%s'"), *Object->GetClass()->GetName(), PropName));
			return;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s '%s' is marked Replicated"), *Object->GetClass()->GetName(), PropName),
			Prop->HasAnyPropertyFlags(CPF_Net));
	}
}

// ---------------------------------------------------------------------------
// Client-truth regression: the properties the MP-1..MP-9 fixes rely on must
// stay in each class's replication contract, or clients silently desync again.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverReplicationContractTest,
	"Holdover.MP.ReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverReplicationContractTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UObject* Character = CDO<AHoldoverCharacter>();
	UObject* Spray = CDO<UHoldoverSprayComponent>();
	UObject* Truck = CDO<AHoldoverDeicingTruck>();
	UObject* Aircraft = CDO<AHoldoverAircraft>();
	UObject* Section = CDO<AHoldoverSurfaceSection>();
	UObject* GS = CDO<AHoldoverGameState>();
	UObject* PS = CDO<AHoldoverPlayerState>();

	if (!TestNotNull(TEXT("Character CDO"), Character) || !TestNotNull(TEXT("Spray CDO"), Spray)
		|| !TestNotNull(TEXT("Truck CDO"), Truck) || !TestNotNull(TEXT("Aircraft CDO"), Aircraft)
		|| !TestNotNull(TEXT("Section CDO"), Section) || !TestNotNull(TEXT("GameState CDO"), GS)
		|| !TestNotNull(TEXT("PlayerState CDO"), PS))
	{
		return false;
	}

	// MP-2 (speed state) + MP-6 (vehicle hidden/collision).
	TestReplicatedProperty(*this, Character, TEXT("SpeedScale"));
	TestReplicatedProperty(*this, Character, TEXT("bVehicleHidden"));
	// M4: the client's sprint prediction multiplies by IceScale — if it is not
	// replicated the client predicts Base*Sprint while the server clamps to
	// Base*Ice*Sprint (CMC corrections until OnRep_SpeedScale).
	TestReplicatedProperty(*this, Character, TEXT("IceScale"));

	// MP-1 (carried fluid) + tank/refill HUD state.
	TestReplicatedProperty(*this, Spray, TEXT("FluidLevel"));
	TestReplicatedProperty(*this, Spray, TEXT("CarriedFluidType"));
	TestReplicatedProperty(*this, Spray, TEXT("bRefilling"));
	TestReplicatedProperty(*this, Spray, TEXT("RefillTimeRemaining"));

	// MP-6 (occupancy) + MP-7 (boom presentation).
	TestReplicatedProperty(*this, Truck, TEXT("OccupantCharacter"));
	TestReplicatedProperty(*this, Truck, TEXT("BoomYaw"));
	TestReplicatedProperty(*this, Truck, TEXT("BoomExtension"));

	// MP-5 (departure/arrival state).
	TestReplicatedProperty(*this, Aircraft, TEXT("bDeparted"));
	TestReplicatedProperty(*this, Aircraft, TEXT("bArrived"));
	TestReplicatedProperty(*this, Aircraft, TEXT("HoldoverState"));
	TestReplicatedProperty(*this, Aircraft, TEXT("HoldoverRemaining"));

	// Server-owned treatment values.
	TestReplicatedProperty(*this, Section, TEXT("CleanlinessPercent"));
	TestReplicatedProperty(*this, Section, TEXT("ProtectionPercent"));

	// Server-owned shift ledger (host/client score agreement surface).
	TestReplicatedProperty(*this, GS, TEXT("WeatherSeverity"));
	TestReplicatedProperty(*this, GS, TEXT("ShiftState"));
	TestReplicatedProperty(*this, GS, TEXT("ShiftTimeRemaining"));
	TestReplicatedProperty(*this, GS, TEXT("Quota"));
	TestReplicatedProperty(*this, GS, TEXT("OnTimeDepartures"));
	TestReplicatedProperty(*this, GS, TEXT("LateDepartures"));
	TestReplicatedProperty(*this, GS, TEXT("Money"));
	TestReplicatedProperty(*this, GS, TEXT("Deductions"));
	TestReplicatedProperty(*this, GS, TEXT("bShiftPassed"));
	TestReplicatedProperty(*this, GS, TEXT("DispatchFeed"));
	// G-5: late joiners must see the real duration and pay rules.
	TestReplicatedProperty(*this, GS, TEXT("LastShiftDuration"));
	TestReplicatedProperty(*this, GS, TEXT("PayPerOnTimeDeparture"));
	TestReplicatedProperty(*this, GS, TEXT("LateDepartureDeduction"));
	// Sonnet H1: shift number + company ledger must reach clients.
	TestReplicatedProperty(*this, GS, TEXT("ShiftNumber"));
	TestReplicatedProperty(*this, GS, TEXT("TotalCompanyPay"));
	TestReplicatedProperty(*this, GS, TEXT("TotalCompanyDeductions"));
	TestReplicatedProperty(*this, GS, TEXT("CompanyLedger"));

	// Per-player attribution.
	TestReplicatedProperty(*this, PS, TEXT("Earnings"));
	TestReplicatedProperty(*this, PS, TEXT("Penalties"));
	TestReplicatedProperty(*this, PS, TEXT("DeparturesHandled"));
	TestReplicatedProperty(*this, PS, TEXT("CarriedFluidType"));

	// The actors clients talk to must actually replicate.
	TestTrue(TEXT("Character replicates"), CDO<AHoldoverCharacter>()->GetIsReplicated());
	TestTrue(TEXT("Truck replicates"), CDO<AHoldoverDeicingTruck>()->GetIsReplicated());
	TestTrue(TEXT("Aircraft replicates"), CDO<AHoldoverAircraft>()->GetIsReplicated());
	TestTrue(TEXT("Section replicates"), CDO<AHoldoverSurfaceSection>()->GetIsReplicated());

	return true;
}

// ---------------------------------------------------------------------------
// MP-8 server spray validation: dt clamp, range rejection, fluid rules — run
// through the same handler the ServerSpray RPC delegates to.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverSprayValidationWorldTest,
	"Holdover.MP.SprayValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverSprayValidationWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	UHoldoverSprayComponent* Spray = nullptr;
	AActor* Owner = SpawnSprayOwner(World, FVector::ZeroVector, Spray);
	AHoldoverSurfaceSection* Near = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	AHoldoverSurfaceSection* Far = Spawn<AHoldoverSurfaceSection>(World, FVector(5000.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Spray owner"), Owner) || !TestNotNull(TEXT("Near section"), Near)
		|| !TestNotNull(TEXT("Far section"), Far) || !TestNotNull(TEXT("Spray component"), Spray))
	{
		DestroyTestWorld(World);
		return false;
	}

	// dt=1000 must be clamped to 2x the request interval (0.2s): 25 clean/s * 0.2 = 5.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("In-range spray applied"), Spray->ServerApplySprayRequest(Near, 1000.f, 0.0));
	TestEqual(TEXT("dt clamp caps cleanliness gain"), Near->CleanlinessPercent, 5.f, 0.01f);
	TestEqual(TEXT("dt clamp caps tank drain"), Spray->FluidLevel, 100.f - 8.f * 0.2f, 0.01f);

	// Out-of-range target: no change regardless of claimed dt.
	const float FarBefore = Far->CleanlinessPercent;
	TestFalse(TEXT("Out-of-range spray rejected"), Spray->ServerApplySprayRequest(Far, 0.1f, 0.0));
	TestEqual(TEXT("Far section untouched"), Far->CleanlinessPercent, FarBefore, 0.01f);

	// Type IV on a dirty section is rejected server-side and consumes nothing.
	AHoldoverSurfaceSection* Dirty = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 300.f, 0.f));
	Spray->CarriedFluidType = EHoldoverFluidType::TypeIV;
	Spray->FluidLevel = 100.f;
	TestFalse(TEXT("Type IV rejected on dirty section"), Spray->ServerApplySprayRequest(Dirty, 0.1f, 0.05));
	TestEqual(TEXT("Protection unchanged"), Dirty->ProtectionPercent, 0.f, 0.01f);
	TestEqual(TEXT("Tank untouched on rejection"), Spray->FluidLevel, 100.f, 0.01f);

	// Legitimate request applies clean rules.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	const float Before = Dirty->CleanlinessPercent;
	TestTrue(TEXT("Legitimate spray applied"), Spray->ServerApplySprayRequest(Dirty, 0.1f, 0.15));
	TestEqual(TEXT("Cleanliness rose"), Dirty->CleanlinessPercent, Before + 25.f * 0.1f, 0.01f);

	// Sonnet M1: a same-time RPC flood cannot overspeed treatment — the token
	// bucket banks at most 2x the request interval (0.2s of spray time).
	const float FloodBefore = Dirty->CleanlinessPercent;
	for (int32 i = 0; i < 100; ++i)
	{
		Spray->ServerApplySprayRequest(Dirty, 0.2f, 0.15);
	}
	TestEqual(TEXT("same-time flood moves nothing"), Dirty->CleanlinessPercent, FloodBefore, 0.01f);

	// After idle time the bucket refills to ONE burst — then exactly one burst
	// applies no matter how many requests arrive in the same instant.
	for (int32 i = 0; i < 100; ++i)
	{
		Spray->ServerApplySprayRequest(Dirty, 0.2f, 10.0);
	}
	TestEqual(TEXT("idle refills one burst only"), Dirty->CleanlinessPercent, FloodBefore + 25.f * 0.2f, 0.01f);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Double-clear / double-award: a second clearance request on a departed
// aircraft must not register another departure or another payment.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverDepartOnceWorldTest,
	"Holdover.MP.DepartOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverDepartOnceWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverGameState* GS = Spawn<AHoldoverGameState>(World, FVector::ZeroVector);
	AHoldoverAircraft* Aircraft = Spawn<AHoldoverAircraft>(World, FVector::ZeroVector);
	AHoldoverSurfaceSection* Section = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	AActor* Interactor = Spawn<AActor>(World, FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("GameState"), GS) || !TestNotNull(TEXT("Aircraft"), Aircraft)
		|| !TestNotNull(TEXT("Section"), Section) || !TestNotNull(TEXT("Interactor"), Interactor))
	{
		DestroyTestWorld(World);
		return false;
	}

	// Wire the spawned GameState as the world's server state. UWorld::GameState is
	// a private UPROPERTY, so this is done by reflection — the same assignment
	// GameMode::InitGameState performs in a real session.
	FObjectPropertyBase* GameStateProp =
		FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), FName(TEXT("GameState")));
	if (!TestNotNull(TEXT("UWorld::GameState property exists"), GameStateProp))
	{
		DestroyTestWorld(World);
		return false;
	}
	GameStateProp->SetObjectPropertyValue_InContainer(World, GS);
	if (!TestNotNull(TEXT("GameState wired into world"), World->GetGameState<AHoldoverGameState>()))
	{
		DestroyTestWorld(World);
		return false;
	}

	Aircraft->RegisterSection(Section);
	Section->ApplyCleaning(Section->CleanThreshold);
	Section->ApplyProtection(100.f);
	Aircraft->HoldoverState = EHoldoverState::HoldoverActive;
	Aircraft->HoldoverRemaining = 60.f;

	// Shift in progress and ArrivalTime unset -> the first departure is on-time.
	GS->ShiftState = EShiftState::InProgress;

	// Preconditions (fail with a named cause instead of a silent early-return in
	// Interact_Implementation).
	TestTrue(TEXT("Aircraft has authority in test world"), Aircraft->HasAuthority());
	TestTrue(TEXT("Aircraft cleared for takeoff"), Aircraft->IsClearedForTakeoff());

	// Direct call on the server-side implementation (the exact body the
	// ServerInteract RPC reaches). Interface Execute_ dispatch is covered by the
	// CoverageAuthority test.
	Aircraft->Interact_Implementation(Interactor);
	TestTrue(TEXT("Aircraft departed"), Aircraft->bDeparted);
	TestEqual(TEXT("Exactly one on-time departure"), GS->OnTimeDepartures, 1);
	TestEqual(TEXT("Paid exactly once"), GS->Money, GS->PayPerOnTimeDeparture);
	TestEqual(TEXT("No late departures"), GS->LateDepartures, 0);

	// Second clearance request (the "two clients press E" race) must not pay again.
	Aircraft->Interact_Implementation(Interactor);
	TestEqual(TEXT("Still exactly one departure"), GS->OnTimeDepartures, 1);
	TestEqual(TEXT("Still paid once"), GS->Money, GS->PayPerOnTimeDeparture);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Self-set coverage: interaction never writes treatment values. Coverage moves
// only through the server spray path.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverCoverageAuthorityWorldTest,
	"Holdover.MP.CoverageAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverCoverageAuthorityWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	UHoldoverSprayComponent* Spray = nullptr;
	AActor* Owner = SpawnSprayOwner(World, FVector::ZeroVector, Spray);
	AHoldoverAircraft* Aircraft = Spawn<AHoldoverAircraft>(World, FVector(0.f, 500.f, 0.f));
	AHoldoverSurfaceSection* Section = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Spray owner"), Owner) || !TestNotNull(TEXT("Aircraft"), Aircraft)
		|| !TestNotNull(TEXT("Section"), Section) || !TestNotNull(TEXT("Spray component"), Spray))
	{
		DestroyTestWorld(World);
		return false;
	}

	Aircraft->RegisterSection(Section);

	// Interacting with a section forwards to the aircraft's clearance request and
	// must NOT change the section's treatment values (no self-set coverage).
	IHoldoverInteractableInterface::Execute_Interact(Section, Owner);
	TestEqual(TEXT("Cleanliness untouched by interact"), Section->CleanlinessPercent, 0.f, 0.01f);
	TestEqual(TEXT("Protection untouched by interact"), Section->ProtectionPercent, 0.f, 0.01f);
	TestFalse(TEXT("Not cleared for takeoff yet"), Aircraft->bDeparted);

	// The server spray handler is the only coverage writer.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("Spray path writes coverage"), Spray->ServerApplySprayRequest(Section, 0.1f));
	TestTrue(TEXT("Coverage rose via server spray"), Section->CleanlinessPercent > 0.f);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// MP-7 boom validation: requests cannot teleport the boom past its speed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverBoomValidationWorldTest,
	"Holdover.MP.BoomValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverBoomValidationWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverDeicingTruck* Truck = Spawn<AHoldoverDeicingTruck>(World, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Truck"), Truck))
	{
		DestroyTestWorld(World);
		return false;
	}

	// G-2 token bucket: a teleport request may only cover BoomBurstSeconds of
	// motion (0.25s * 60 deg/s = 15 deg yaw, 0.25s * 300 uu/s = 75 uu extension).
	const float BurstYaw = Truck->YawSpeed * Truck->BoomBurstSeconds;
	const float BurstExt = Truck->ExtensionSpeed * Truck->BoomBurstSeconds;

	Truck->ApplyBoomRequest(999.f, 9999.f, 0.0);
	TestEqual(TEXT("Yaw clamped to one burst of motion"), Truck->BoomYaw, BurstYaw, 0.01f);
	TestEqual(TEXT("Extension clamped to one burst of motion"), Truck->BoomExtension, 400.f + BurstExt, 0.01f);

	// A same-frame RPC flood cannot overspeed the boom.
	for (int32 i = 0; i < 100; ++i)
	{
		Truck->ApplyBoomRequest(999.f, 9999.f, 0.0);
	}
	TestEqual(TEXT("Yaw unchanged by same-frame flood"), Truck->BoomYaw, BurstYaw, 0.01f);
	TestEqual(TEXT("Extension unchanged by same-frame flood"), Truck->BoomExtension, 400.f + BurstExt, 0.01f);

	// After idle time the bucket refills to one burst — reverse teleport is
	// clamped symmetrically (and can never overshoot the absolute clamps).
	Truck->ApplyBoomRequest(-999.f, -9999.f, 10.0);
	TestEqual(TEXT("Yaw clamped back"), Truck->BoomYaw, 0.f, 0.01f);
	TestEqual(TEXT("Extension clamped back"), Truck->BoomExtension, 400.f, 0.01f);

	for (int32 i = 0; i < 100; ++i)
	{
		Truck->ApplyBoomRequest(-999.f, -9999.f, 10.0);
	}
	TestEqual(TEXT("Yaw unchanged by reverse flood"), Truck->BoomYaw, 0.f, 0.01f);
	TestEqual(TEXT("Extension unchanged by reverse flood"), Truck->BoomExtension, 400.f, 0.01f);

	// Absolute clamps still apply at the edges.
	Truck->ApplyBoomRequest(100000.f, 100000.f, 100.0);
	TestTrue(TEXT("Yaw within MaxAbsYaw"), FMath::Abs(Truck->BoomYaw) <= Truck->MaxAbsYaw + 0.01f);
	TestTrue(TEXT("Extension within MaxExtension"), Truck->BoomExtension <= Truck->MaxExtension + 0.01f);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// G-1 refill guard: the refill RPC is client-callable, so the server must
// validate station proximity, the fluid enum and refill-in-progress itself.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverRefillGuardWorldTest,
	"Holdover.MP.RefillGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverRefillGuardWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverTestInteractable* Station = Spawn<AHoldoverTestInteractable>(World, FVector(100.f, 0.f, 0.f));
	UHoldoverSprayComponent* NearSpray = nullptr;
	UHoldoverSprayComponent* FarSpray = nullptr;
	UHoldoverSprayComponent* SecondNearSpray = nullptr;
	AActor* NearOwner = SpawnSprayOwner(World, FVector::ZeroVector, NearSpray);
	AActor* FarOwner = SpawnSprayOwner(World, FVector(5000.f, 0.f, 0.f), FarSpray);
	AActor* SecondNearOwner = SpawnSprayOwner(World, FVector(200.f, 0.f, 0.f), SecondNearSpray);
	if (!TestNotNull(TEXT("Station"), Station) || !TestNotNull(TEXT("Near owner"), NearOwner)
		|| !TestNotNull(TEXT("Far owner"), FarOwner) || !TestNotNull(TEXT("Second near owner"), SecondNearOwner)
		|| !TestNotNull(TEXT("Near spray"), NearSpray) || !TestNotNull(TEXT("Far spray"), FarSpray)
		|| !TestNotNull(TEXT("Second near spray"), SecondNearSpray))
	{
		DestroyTestWorld(World);
		return false;
	}

	// At a station: refill starts (one-tank rule: type swaps, tank empties).
	NearSpray->FluidLevel = 40.f;
	TestTrue(TEXT("Refill at station accepted"), NearSpray->ValidateAndBeginRefill(EHoldoverFluidType::TypeIV));
	TestTrue(TEXT("Refilling flag set"), NearSpray->IsRefilling());
	TestEqual(TEXT("Fluid type swapped"), (int32)NearSpray->CarriedFluidType, (int32)EHoldoverFluidType::TypeIV);
	TestEqual(TEXT("Tank emptied for refill"), NearSpray->FluidLevel, 0.f, 0.01f);
	TestEqual(TEXT("Refill timer running"), NearSpray->GetRefillProgress(), 0.f, 0.01f);

	// A second request mid-refill must not restart the timer.
	TestFalse(TEXT("Double refill rejected"), NearSpray->ValidateAndBeginRefill(EHoldoverFluidType::TypeI));
	TestEqual(TEXT("Refill timer untouched"), NearSpray->GetRefillProgress(), 0.f, 0.01f);
	TestEqual(TEXT("Fluid type untouched"), (int32)NearSpray->CarriedFluidType, (int32)EHoldoverFluidType::TypeIV);

	// Far from every station: rejected outright.
	FarSpray->FluidLevel = 100.f;
	TestFalse(TEXT("Refill away from station rejected"), FarSpray->ValidateAndBeginRefill(EHoldoverFluidType::TypeI));
	TestFalse(TEXT("Not refilling"), FarSpray->IsRefilling());
	TestEqual(TEXT("Tank untouched"), FarSpray->FluidLevel, 100.f, 0.01f);

	// Garbage enum values never enter replicated state.
	TestFalse(TEXT("Invalid fluid enum rejected"),
		SecondNearSpray->ValidateAndBeginRefill(static_cast<EHoldoverFluidType>(7)));
	TestFalse(TEXT("Not refilling after invalid enum"), SecondNearSpray->IsRefilling());

	// The enum check must not block legitimate refills.
	TestTrue(TEXT("Legitimate refill still accepted"), SecondNearSpray->ValidateAndBeginRefill(EHoldoverFluidType::TypeI));

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// G-3/G-4 line of sight: the server re-traces spray and interact requests so a
// modified client cannot act through walls.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverLineOfSightWorldTest,
	"Holdover.MP.LineOfSight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverLineOfSightWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	UHoldoverSprayComponent* Spray = nullptr;
	AActor* Owner = SpawnSprayOwner(World, FVector::ZeroVector, Spray);
	AHoldoverSurfaceSection* Section = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	AActor* Blocker = SpawnBlocker(World, FVector(150.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Spray owner"), Owner) || !TestNotNull(TEXT("Section"), Section)
		|| !TestNotNull(TEXT("Blocker"), Blocker) || !TestNotNull(TEXT("Spray component"), Spray))
	{
		DestroyTestWorld(World);
		return false;
	}

	// Spray through a wall: rejected, coverage unchanged.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	Spray->FluidLevel = 100.f;
	TestFalse(TEXT("Spray through blocker rejected"), Spray->ServerApplySprayRequest(Section, 0.1f));
	TestEqual(TEXT("Coverage unchanged"), Section->CleanlinessPercent, 0.f, 0.01f);

	// Clear the wall: same request is now legitimate.
	Blocker->Destroy();
	TestTrue(TEXT("Spray with clear LOS accepted"), Spray->ServerApplySprayRequest(Section, 0.1f));
	TestTrue(TEXT("Coverage rose"), Section->CleanlinessPercent > 0.f);

	// Interact validation: range AND line of sight.
	AActor* Caller = SpawnSprayOwner(World, FVector(0.f, 500.f, 0.f), Spray);
	AActor* NearTarget = SpawnBlocker(World, FVector(300.f, 500.f, 0.f), 25.f);
	AActor* Wall = SpawnBlocker(World, FVector(150.f, 500.f, 0.f));
	AActor* FarTarget = SpawnBlocker(World, FVector(5000.f, 500.f, 0.f), 25.f);
	if (!TestNotNull(TEXT("Caller"), Caller) || !TestNotNull(TEXT("Near target"), NearTarget)
		|| !TestNotNull(TEXT("Wall"), Wall) || !TestNotNull(TEXT("Far target"), FarTarget))
	{
		DestroyTestWorld(World);
		return false;
	}

	TestFalse(TEXT("Far interact target rejected by range"),
		AHoldoverCharacter::ValidateInteractTarget(Caller, FarTarget, 1000.f));
	TestFalse(TEXT("Interact through wall rejected by LOS"),
		AHoldoverCharacter::ValidateInteractTarget(Caller, NearTarget, 1000.f));
	Wall->Destroy();
	TestTrue(TEXT("Interact with clear LOS accepted"),
		AHoldoverCharacter::ValidateInteractTarget(Caller, NearTarget, 1000.f));

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet H2/M17: arrival visuals are derived from the replicated flags only.
// Clients never write bArrived/bDeparted; SyncArrivalVisuals is exactly the
// client-side BeginPlay path, so this covers the "replication applied before
// BeginPlay" late-joiner ordering that used to leave aircraft hidden forever.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverArrivalVisualsWorldTest,
	"Holdover.MP.ArrivalVisuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverArrivalVisualsWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverAircraft* Aircraft = Spawn<AHoldoverAircraft>(World, FVector::ZeroVector);
	AHoldoverSurfaceSection* Section = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Aircraft"), Aircraft) || !TestNotNull(TEXT("Section"), Section))
	{
		DestroyTestWorld(World);
		return false;
	}
	Aircraft->RegisterSection(Section);

	// Not arrived: everything hidden and collision-less.
	Aircraft->bArrived = false;
	Aircraft->bDeparted = false;
	Aircraft->SyncArrivalVisuals();
	TestTrue(TEXT("aircraft hidden before arrival"), Aircraft->IsHidden());
	TestTrue(TEXT("section hidden before arrival"), Section->IsHidden());

	// Replicated arrival (the late-joiner case): the client-side BeginPlay path
	// is SyncArrivalVisuals - it must show the aircraft AND its sections and
	// must not write the replicated flags back.
	Aircraft->bArrived = true;
	Aircraft->SyncArrivalVisuals();
	TestFalse(TEXT("aircraft visible after replicated arrival"), Aircraft->IsHidden());
	TestFalse(TEXT("section visible after replicated arrival"), Section->IsHidden());
	TestTrue(TEXT("bArrived untouched by client path"), Aircraft->bArrived);

	// Departed: the plane stays visible for the climb-out cue, but its
	// unattached sections must not be left behind, visible and sprayable.
	Aircraft->bDeparted = true;
	Aircraft->SyncArrivalVisuals();
	TestFalse(TEXT("departed aircraft stays visible for climb-out"), Aircraft->IsHidden());
	TestTrue(TEXT("departed sections hidden"), Section->IsHidden());
	TestTrue(TEXT("bDeparted untouched by client path"), Aircraft->bDeparted);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet H1 test gap: the contract test above cannot see DOREPLIFETIME
// registration (enumerating GetLifetimeReplicatedProps asserts in UE 5.8
// automation - see the note in this file's header). This lint closes the hole
// mechanically: every property the contract expects must have a real
// `DOREPLIFETIME(<Class>, <Prop>)` line in the class's .cpp.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverReplicationLintTest,
	"Holdover.MP.ReplicationLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverReplicationLintTest::RunTest(const FString& Parameters)
{
	struct FEntry { const TCHAR* Cpp; const TCHAR* Cls; const TCHAR* Prop; };
	static const FEntry Entries[] = {
		{ TEXT("HoldoverCharacter.cpp"),        TEXT("AHoldoverCharacter"),        TEXT("SpeedScale") },
		{ TEXT("HoldoverCharacter.cpp"),        TEXT("AHoldoverCharacter"),        TEXT("IceScale") },
		{ TEXT("HoldoverCharacter.cpp"),        TEXT("AHoldoverCharacter"),        TEXT("bVehicleHidden") },
		{ TEXT("HoldoverSprayComponent.cpp"),   TEXT("UHoldoverSprayComponent"),   TEXT("FluidLevel") },
		{ TEXT("HoldoverSprayComponent.cpp"),   TEXT("UHoldoverSprayComponent"),   TEXT("CarriedFluidType") },
		{ TEXT("HoldoverSprayComponent.cpp"),   TEXT("UHoldoverSprayComponent"),   TEXT("bRefilling") },
		{ TEXT("HoldoverSprayComponent.cpp"),   TEXT("UHoldoverSprayComponent"),   TEXT("RefillTimeRemaining") },
		{ TEXT("HoldoverDeicingTruck.cpp"),     TEXT("AHoldoverDeicingTruck"),     TEXT("OccupantCharacter") },
		{ TEXT("HoldoverDeicingTruck.cpp"),     TEXT("AHoldoverDeicingTruck"),     TEXT("BoomYaw") },
		{ TEXT("HoldoverDeicingTruck.cpp"),     TEXT("AHoldoverDeicingTruck"),     TEXT("BoomExtension") },
		{ TEXT("HoldoverAircraft.cpp"),         TEXT("AHoldoverAircraft"),         TEXT("bDeparted") },
		{ TEXT("HoldoverAircraft.cpp"),         TEXT("AHoldoverAircraft"),         TEXT("bArrived") },
		{ TEXT("HoldoverAircraft.cpp"),         TEXT("AHoldoverAircraft"),         TEXT("HoldoverState") },
		{ TEXT("HoldoverAircraft.cpp"),         TEXT("AHoldoverAircraft"),         TEXT("HoldoverRemaining") },
		{ TEXT("HoldoverSurfaceSection.cpp"),   TEXT("AHoldoverSurfaceSection"),   TEXT("CleanlinessPercent") },
		{ TEXT("HoldoverSurfaceSection.cpp"),   TEXT("AHoldoverSurfaceSection"),   TEXT("ProtectionPercent") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("WeatherSeverity") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("ShiftState") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("ShiftTimeRemaining") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("Quota") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("OnTimeDepartures") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("LateDepartures") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("Money") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("Deductions") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("bShiftPassed") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("DispatchFeed") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("LastShiftDuration") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("PayPerOnTimeDeparture") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("LateDepartureDeduction") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("ShiftNumber") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("TotalCompanyPay") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("TotalCompanyDeductions") },
		{ TEXT("HoldoverGameState.cpp"),        TEXT("AHoldoverGameState"),        TEXT("CompanyLedger") },
		{ TEXT("HoldoverPlayerState.cpp"),      TEXT("AHoldoverPlayerState"),      TEXT("Earnings") },
		{ TEXT("HoldoverPlayerState.cpp"),      TEXT("AHoldoverPlayerState"),      TEXT("Penalties") },
		{ TEXT("HoldoverPlayerState.cpp"),      TEXT("AHoldoverPlayerState"),      TEXT("DeparturesHandled") },
		{ TEXT("HoldoverPlayerState.cpp"),      TEXT("AHoldoverPlayerState"),      TEXT("CarriedFluidType") },
	};

	TMap<FString, FString> FileCache;
	for (const FEntry& E : Entries)
	{
		FString* Cached = FileCache.Find(E.Cpp);
		if (!Cached)
		{
			const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/Holdover"), E.Cpp);
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				AddError(*FString::Printf(TEXT("cannot read %s for the registration lint"), *Path));
				continue;
			}
			Cached = &FileCache.Add(E.Cpp, MoveTemp(Text));
		}
		// Accept both DOREPLIFETIME(Cls, Prop) and DOREPLIFETIME_CONDITION(Cls, Prop, ...).
		const FString NeedlePlain = FString::Printf(TEXT("DOREPLIFETIME(%s, %s"), E.Cls, E.Prop);
		const FString NeedleCond = FString::Printf(TEXT("DOREPLIFETIME_CONDITION(%s, %s"), E.Cls, E.Prop);
		TestTrue(*FString::Printf(TEXT("%s registers %s"), E.Cls, E.Prop),
			Cached->Contains(NeedlePlain) || Cached->Contains(NeedleCond));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet H4: the shift ledger freezes at the verdict. Post-EndShift departures,
// penalties, holdover aborts and un-arrived "arrivals" must not move
// Money/Deductions under the already-computed PASSED/FAILED.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverShiftEndFreezeWorldTest,
	"Holdover.MP.ShiftEndFreeze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverShiftEndFreezeWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverGameState* GS = Spawn<AHoldoverGameState>(World, FVector::ZeroVector);
	AHoldoverAircraft* Aircraft1 = Spawn<AHoldoverAircraft>(World, FVector::ZeroVector);
	AHoldoverAircraft* Aircraft2 = Spawn<AHoldoverAircraft>(World, FVector(1000.f, 0.f, 0.f));
	AHoldoverAircraft* Aircraft3 = Spawn<AHoldoverAircraft>(World, FVector(2000.f, 0.f, 0.f));
	AHoldoverSurfaceSection* Section1 = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 0.f, 0.f));
	AHoldoverSurfaceSection* Section2 = Spawn<AHoldoverSurfaceSection>(World, FVector(1300.f, 0.f, 0.f));
	AActor* Interactor = Spawn<AActor>(World, FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("GameState"), GS) || !TestNotNull(TEXT("Aircraft1"), Aircraft1)
		|| !TestNotNull(TEXT("Aircraft2"), Aircraft2) || !TestNotNull(TEXT("Aircraft3"), Aircraft3)
		|| !TestNotNull(TEXT("Section1"), Section1) || !TestNotNull(TEXT("Section2"), Section2)
		|| !TestNotNull(TEXT("Interactor"), Interactor))
	{
		DestroyTestWorld(World);
		return false;
	}

	FObjectPropertyBase* GameStateProp =
		FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), FName(TEXT("GameState")));
	if (!TestNotNull(TEXT("UWorld::GameState property exists"), GameStateProp))
	{
		DestroyTestWorld(World);
		return false;
	}
	GameStateProp->SetObjectPropertyValue_InContainer(World, GS);
	GS->ShiftState = EShiftState::InProgress;

	// One legitimate on-time departure, then the verdict.
	Aircraft1->RegisterSection(Section1);
	Section1->ApplyCleaning(Section1->CleanThreshold);
	Section1->ApplyProtection(100.f);
	Aircraft1->HoldoverState = EHoldoverState::HoldoverActive;
	Aircraft1->HoldoverRemaining = 60.f;
	Aircraft1->Interact_Implementation(Interactor);
	TestTrue(TEXT("departed before the verdict"), Aircraft1->bDeparted);
	TestEqual(TEXT("one on-time departure"), GS->OnTimeDepartures, 1);

	GS->EndShift();
	TestEqual(TEXT("shift ended"), (int32)GS->ShiftState, (int32)EShiftState::ShiftEnded);
	const int32 FrozenOnTime = GS->OnTimeDepartures;
	const int32 FrozenLate = GS->LateDepartures;
	const int32 FrozenMoney = GS->Money;
	const int32 FrozenDeductions = GS->Deductions;

	// Post-verdict scoring attempts must all be ignored.
	GS->RegisterDeparture(true);
	GS->RegisterPenalty(50, TEXT("post-verdict penalty"));

	Aircraft2->RegisterSection(Section2);
	Section2->ApplyCleaning(Section2->CleanThreshold);
	Section2->ApplyProtection(100.f);
	Aircraft2->HoldoverState = EHoldoverState::HoldoverActive;
	Aircraft2->HoldoverRemaining = 60.f;
	Aircraft2->Interact_Implementation(Interactor);
	TestFalse(TEXT("no departure after the verdict"), Aircraft2->bDeparted);

	// Holdover abort after the verdict must not deduct (RegisterPenalty frozen).
	Section2->ApplyDegradation(100.f);
	Aircraft2->SimulateTreatment(0.1f, 0.f);

	// Un-arrived aircraft must not "arrive" post-verdict (Elapsed == duration
	// used to fire the arrival for everyone at EndShift).
	Aircraft3->bArrived = false;
	Aircraft3->Tick(0.1f);
	TestFalse(TEXT("no arrival after the verdict"), Aircraft3->bArrived);

	TestEqual(TEXT("on-time frozen"), GS->OnTimeDepartures, FrozenOnTime);
	TestEqual(TEXT("late frozen"), GS->LateDepartures, FrozenLate);
	TestEqual(TEXT("money frozen"), GS->Money, FrozenMoney);
	TestEqual(TEXT("deductions frozen"), GS->Deductions, FrozenDeductions);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet M2/M17: surface sections are separate BlockAll actors. A target
// section behind a sibling panel must still be hittable (siblings ignored in
// the LOS traces), and sections of a not-live aircraft must not be sprayable.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverSectionTargetValidationWorldTest,
	"Holdover.MP.SectionTargetValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverSectionTargetValidationWorldTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	UHoldoverSprayComponent* Spray = nullptr;
	AActor* Owner = SpawnSprayOwner(World, FVector::ZeroVector, Spray);
	AHoldoverAircraft* Aircraft = Spawn<AHoldoverAircraft>(World, FVector(0.f, 1000.f, 0.f));
	// Front panel sits ON the line to the back panel - without sibling-ignore
	// the server LOS trace is falsely blocked by the target's own aircraft.
	AHoldoverSurfaceSection* Front = Spawn<AHoldoverSurfaceSection>(World, FVector(200.f, 0.f, 0.f));
	AHoldoverSurfaceSection* Back = Spawn<AHoldoverSurfaceSection>(World, FVector(400.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Spray owner"), Owner) || !TestNotNull(TEXT("Aircraft"), Aircraft)
		|| !TestNotNull(TEXT("Front panel"), Front) || !TestNotNull(TEXT("Back panel"), Back)
		|| !TestNotNull(TEXT("Spray component"), Spray))
	{
		DestroyTestWorld(World);
		return false;
	}

	Aircraft->RegisterSection(Front);
	Aircraft->RegisterSection(Back);
	Aircraft->bArrived = true;
	Aircraft->bDeparted = false;

	// M2: the back panel is behind its sibling - spray AND interact must reach it.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	Spray->FluidLevel = 100.f;
	TestTrue(TEXT("spray reaches a panel behind its sibling"), Spray->ServerApplySprayRequest(Back, 0.1f, 0.0));
	TestTrue(TEXT("back panel cleaned through sibling"), Back->CleanlinessPercent > 0.f);
	TestTrue(TEXT("interact reaches a panel behind its sibling"),
		AHoldoverCharacter::ValidateInteractTarget(Owner, Back, 1000.f));

	// M17: a departed aircraft's panels are not sprayable, even on request.
	Aircraft->bDeparted = true;
	const float Before = Back->CleanlinessPercent;
	TestFalse(TEXT("departed aircraft panels rejected"), Spray->ServerApplySprayRequest(Back, 0.1f, 1.0));
	TestEqual(TEXT("no treatment on departed panels"), Back->CleanlinessPercent, Before, 0.01f);

	// M17: not-yet-arrived panels are not sprayable either.
	Aircraft->bDeparted = false;
	Aircraft->bArrived = false;
	TestFalse(TEXT("un-arrived aircraft panels rejected"), Spray->ServerApplySprayRequest(Front, 0.1f, 2.0));

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet M5: the ice slowdown is overlap-counted. Overlapping two ice patches
// and leaving one used to clear the slowdown (OnOverlapEnd always released) -
// the counter must hold it until the LAST release. Drives the same
// SetIceSlowed calls the hazard volumes make (the volumes are symmetric
// begin/end, so the counting contract lives on the character).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverIceOverlapCountTest,
	"Holdover.Hazard.IceOverlapCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverIceOverlapCountTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverTestCharacter* Char = Spawn<AHoldoverTestCharacter>(World, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Character"), Char))
	{
		DestroyTestWorld(World);
		return false;
	}
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (!TestNotNull(TEXT("Character movement"), Move))
	{
		DestroyTestWorld(World);
		return false;
	}
	const float Base = Move->MaxWalkSpeed;

	// One ice patch: slowed.
	Char->SetIceSlowed(true, 0.5f);
	TestEqual(TEXT("slowed while on ice"), Move->MaxWalkSpeed, Base * 0.5f, KINDA_SMALL_NUMBER);

	// Two overlapping patches, then one released - still slowed.
	Char->SetIceSlowed(true, 0.5f);
	Char->SetIceSlowed(false, 0.5f);
	TestEqual(TEXT("still slowed while a second patch is held"), Move->MaxWalkSpeed, Base * 0.5f, KINDA_SMALL_NUMBER);

	// Last patch released - cleared exactly once.
	Char->SetIceSlowed(false, 0.5f);
	TestEqual(TEXT("cleared only at the last release"), Move->MaxWalkSpeed, Base, KINDA_SMALL_NUMBER);

	// Unbalanced release must not underflow into a permanent slow.
	Char->SetIceSlowed(false, 0.5f);
	TestEqual(TEXT("unbalanced release is clamped"), Move->MaxWalkSpeed, Base, KINDA_SMALL_NUMBER);

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet M6: hazard prediction is gated and deterministic. The locally
// controlled client predicts the launch only when it is NOT the authority -
// in the test world (NM_Standalone) a possessed pawn is both locally controlled
// and authoritative, i.e. exactly the listen-host seat where prediction used to
// launch twice (its own FRandRange, then the server's). Both the ice stumble
// and the jet-blast push must land on the same deterministic direction the
// server computes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverJetBlastPredictionTest,
	"Holdover.Hazard.JetBlastPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverJetBlastPredictionTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	// Prediction gate matrix (M6).
	TestFalse(TEXT("listen host must not predict"),
		AHoldoverHazardVolume::ShouldPredictLaunch(/*bHasAuthority*/ true, /*bLocallyControlled*/ true));
	TestTrue(TEXT("remote client predicts"),
		AHoldoverHazardVolume::ShouldPredictLaunch(false, true));
	TestFalse(TEXT("server does not predict"),
		AHoldoverHazardVolume::ShouldPredictLaunch(true, false));
	TestFalse(TEXT("simulated proxy does not predict"),
		AHoldoverHazardVolume::ShouldPredictLaunch(false, false));

	// Deterministic direction formulas (M6 - the ice path used to be random).
	const FVector Fwd = FVector::ForwardVector;
	const FVector ExpectedStumble = (Fwd * 0.5f + FVector(0.f, 0.f, 0.15f)).GetSafeNormal();
	TestTrue(TEXT("stumble direction is deterministic"),
		AHoldoverHazardVolume::ComputeStumbleDirection(Fwd).Equals(ExpectedStumble, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("stumble direction stable across calls"),
		AHoldoverHazardVolume::ComputeStumbleDirection(Fwd).Equals(
			AHoldoverHazardVolume::ComputeStumbleDirection(Fwd), KINDA_SMALL_NUMBER));

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverTestCharacter* Char = Spawn<AHoldoverTestCharacter>(World, FVector::ZeroVector);
	AHoldoverTestController* PC = Spawn<AHoldoverTestController>(World, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Character"), Char) || !TestNotNull(TEXT("Controller"), PC))
	{
		DestroyTestWorld(World);
		return false;
	}
	PC->Possess(Char);
	TestTrue(TEXT("possessed pawn is the listen-host seat in this world"), Char->IsLocallyControlled());

	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (!TestNotNull(TEXT("Character movement"), Move))
	{
		DestroyTestWorld(World);
		return false;
	}
	// Bare test worlds never BeginPlay, so the movement component is inactive
	// and LaunchCharacter would no-op. Flip the active flag like BeginPlay would
	// (Activate(true) would Reset() the mode to MOVE_None).
	Move->SetActive(true);
	// The possess/restart chain leaves the movement at MOVE_None here; put it
	// back to a live mode so LaunchCharacter applies, as it does in game.
	Move->SetMovementMode(MOVE_Falling);

	// Drive the REAL overlap behaviour (the same entry the engine's overlap
	// delegate reaches).
	AHoldoverHazardVolume* Ice = Spawn<AHoldoverHazardVolume>(World, FVector(0.f, 0.f, 50.f));
	if (!TestNotNull(TEXT("Ice volume"), Ice))
	{
		DestroyTestWorld(World);
		return false;
	}
	Ice->HazardType = EHoldoverHazardType::Ice;
	Ice->HandleBeginOverlap(Char);
	TestTrue(TEXT("ice stumble is the deterministic direction (no per-call FRandRange)"),
		Move->PendingLaunchVelocity.Equals(ExpectedStumble * Ice->IceStumbleStrength, 0.1f));

	AHoldoverHazardVolume* Blast = Spawn<AHoldoverHazardVolume>(World, FVector(300.f, 0.f, 50.f));
	if (!TestNotNull(TEXT("Jet-blast volume"), Blast))
	{
		DestroyTestWorld(World);
		return false;
	}
	Blast->HazardType = EHoldoverHazardType::JetBlast;
	Blast->HandleBeginOverlap(Char);
	const FVector ExpectedBlast =
		(Char->GetActorLocation() - Blast->GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.f, 0.35f);
	TestTrue(TEXT("jet-blast push is the deterministic direction"),
		Move->PendingLaunchVelocity.Equals(ExpectedBlast * Blast->JetBlastLaunchStrength, 0.1f));

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet M8: a disconnecting truck occupant must not leave an orphan
// Character. On disconnect the engine runs APlayerController::Destroyed ->
// PawnLeavingGame while the connection's pawn is the level-placed truck: the
// truck must survive and the abandoned occupant must be destroyed like any
// disconnecting player's pawn. The non-disconnect eviction (OnRep/EndPlay)
// must keep its no-destroy semantics.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverTruckOccupantEvictionTest,
	"Holdover.Truck.OccupantEviction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverTruckOccupantEvictionTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverDeicingTruck* Truck = Spawn<AHoldoverDeicingTruck>(World, FVector::ZeroVector);
	AHoldoverTestCharacter* Char = Spawn<AHoldoverTestCharacter>(World, FVector(200.f, 0.f, 0.f));
	AHoldoverTestController* PC = Spawn<AHoldoverTestController>(World, FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Truck"), Truck) || !TestNotNull(TEXT("Character"), Char)
		|| !TestNotNull(TEXT("Controller"), PC))
	{
		DestroyTestWorld(World);
		return false;
	}

	PC->Possess(Char);
	TestTrue(TEXT("entering the truck succeeds"), Truck->TryEnter(Char));
	TestTrue(TEXT("truck occupied"), Truck->IsOccupied());
	TestTrue(TEXT("occupant hidden in vehicle"), Char->IsHidden());

	// Disconnect: the engine runs PawnLeavingGame while the connection's pawn
	// is the truck.
	PC->PawnLeavingGame();

	TestFalse(TEXT("orphan occupant destroyed on disconnect"), IsValid(Char));
	TestFalse(TEXT("level-placed truck survives the disconnect"), Truck->IsPendingKillPending());
	TestFalse(TEXT("truck freed for the rest of the crew"), Truck->IsOccupied());

	// Eviction is idempotent - a second pass must be a no-op.
	Truck->EvictOccupant();
	TestFalse(TEXT("second eviction is a no-op"), Truck->IsOccupied());
	TestFalse(TEXT("truck alive after double eviction"), Truck->IsPendingKillPending());

	// The non-disconnect path (OnRep_Controller/EndPlay) must NOT destroy the
	// occupant - only the leaving-controller path does.
	AHoldoverTestCharacter* Char2 = Spawn<AHoldoverTestCharacter>(World, FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Second character"), Char2))
	{
		DestroyTestWorld(World);
		return false;
	}
	PC->Possess(Char2);
	TestTrue(TEXT("second occupant enters"), Truck->TryEnter(Char2));
	Truck->EvictOccupant();
	TestTrue(TEXT("non-disconnect eviction keeps the occupant alive"), IsValid(Char2));
	TestFalse(TEXT("second occupant freed"), Truck->IsOccupied());

	DestroyTestWorld(World);
	return true;
}

// ---------------------------------------------------------------------------
// Sonnet M12: the departure board shows ALL arrived, non-departed aircraft.
// It used to cache the first TActorIterator hit and break - with the two-plane
// shift that could be an un-arrived, hidden or already-departed plane.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverDepartureBoardRowsTest,
	"Holdover.UI.DepartureBoardRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverDepartureBoardRowsTest::RunTest(const FString& Parameters)
{
	using namespace HoldoverWorldTest;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AHoldoverAircraft* Live1 = Spawn<AHoldoverAircraft>(World, FVector(0.f, 0.f, 0.f));
	AHoldoverAircraft* NotArrived = Spawn<AHoldoverAircraft>(World, FVector(1000.f, 0.f, 0.f));
	AHoldoverAircraft* Departed = Spawn<AHoldoverAircraft>(World, FVector(2000.f, 0.f, 0.f));
	AHoldoverAircraft* Live2 = Spawn<AHoldoverAircraft>(World, FVector(3000.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Live1"), Live1) || !TestNotNull(TEXT("NotArrived"), NotArrived)
		|| !TestNotNull(TEXT("Departed"), Departed) || !TestNotNull(TEXT("Live2"), Live2))
	{
		DestroyTestWorld(World);
		return false;
	}

	Live1->bArrived = true;
	Live1->bDeparted = false;
	NotArrived->bArrived = false;
	NotArrived->bDeparted = false;
	Departed->bArrived = true;
	Departed->bDeparted = true;
	Live2->bArrived = true;
	Live2->bDeparted = false;

	TArray<AHoldoverAircraft*> Rows;
	UHoldoverDepartureBoardWidget::GatherBoardAircraft(World, Rows);

	TestEqual(TEXT("all arrived, non-departed aircraft are shown"), Rows.Num(), 2);
	TestTrue(TEXT("first live aircraft shown"), Rows.Contains(Live1));
	TestTrue(TEXT("second live aircraft shown"), Rows.Contains(Live2));
	TestFalse(TEXT("un-arrived aircraft hidden"), Rows.Contains(NotArrived));
	TestFalse(TEXT("departed aircraft hidden"), Rows.Contains(Departed));

	// Nothing live: the board is empty rather than showing a stale plane.
	Live1->bDeparted = true;
	Live2->bArrived = false;
	Rows.Reset();
	UHoldoverDepartureBoardWidget::GatherBoardAircraft(World, Rows);
	TestEqual(TEXT("empty when no aircraft is live"), Rows.Num(), 0);

	DestroyTestWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

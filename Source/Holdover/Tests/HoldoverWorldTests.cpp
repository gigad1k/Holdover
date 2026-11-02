// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Holdover.h"
#include "HoldoverAircraft.h"
#include "HoldoverCharacter.h"
#include "HoldoverDeicingTruck.h"
#include "HoldoverGameState.h"
#include "HoldoverPlayerState.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"
#include "HoldoverTestInteractable.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
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
	TestTrue(TEXT("In-range spray applied"), Spray->ServerApplySprayRequest(Near, 1000.f));
	TestEqual(TEXT("dt clamp caps cleanliness gain"), Near->CleanlinessPercent, 5.f, 0.01f);
	TestEqual(TEXT("dt clamp caps tank drain"), Spray->FluidLevel, 100.f - 8.f * 0.2f, 0.01f);

	// Out-of-range target: no change regardless of claimed dt.
	const float FarBefore = Far->CleanlinessPercent;
	TestFalse(TEXT("Out-of-range spray rejected"), Spray->ServerApplySprayRequest(Far, 0.1f));
	TestEqual(TEXT("Far section untouched"), Far->CleanlinessPercent, FarBefore, 0.01f);

	// Type IV on a dirty section is rejected server-side and consumes nothing.
	AHoldoverSurfaceSection* Dirty = Spawn<AHoldoverSurfaceSection>(World, FVector(300.f, 300.f, 0.f));
	Spray->CarriedFluidType = EHoldoverFluidType::TypeIV;
	Spray->FluidLevel = 100.f;
	TestFalse(TEXT("Type IV rejected on dirty section"), Spray->ServerApplySprayRequest(Dirty, 0.1f));
	TestEqual(TEXT("Protection unchanged"), Dirty->ProtectionPercent, 0.f, 0.01f);
	TestEqual(TEXT("Tank untouched on rejection"), Spray->FluidLevel, 100.f, 0.01f);

	// Legitimate request applies clean rules.
	Spray->CarriedFluidType = EHoldoverFluidType::TypeI;
	const float Before = Dirty->CleanlinessPercent;
	TestTrue(TEXT("Legitimate spray applied"), Spray->ServerApplySprayRequest(Dirty, 0.1f));
	TestEqual(TEXT("Cleanliness rose"), Dirty->CleanlinessPercent, Before + 25.f * 0.1f, 0.01f);

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

#endif // WITH_DEV_AUTOMATION_TESTS

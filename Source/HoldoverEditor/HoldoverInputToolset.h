// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "HoldoverInputToolset.generated.h"

/**
 * Editor-only MCP toolset for driving PIE input from agents (Phase 7.5 client-seat
 * verification). Injects key/axis events through UPlayerInput so Enhanced Input
 * actions fire exactly like hardware input. PIEInstance selects the PIE client
 * (0 = first instance; with a 2-player listen server, 1 is the remote client).
 */
UCLASS(MinimalAPI)
class UHoldoverInputToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/*
	 * Injects a key press or release into a PIE instance's player.
	 * @param PIEInstance The PIE instance index (0-based).
	 * @param KeyName Engine key name, e.g. "W", "LeftShift", "E", "LeftMouseButton", "Q".
	 * @param bDown True for press, false for release.
	 * @return "OK: ..." or "ERROR: ...".
	 */
	UFUNCTION(meta = (AICallable))
	static FString InjectKey(int32 PIEInstance, const FString& KeyName, bool bDown);

	/*
	 * Injects a mouse-look axis delta (MouseX/MouseY) into a PIE instance's player.
	 * @param PIEInstance The PIE instance index (0-based).
	 * @param DeltaX Horizontal axis delta (keyboard-scale units).
	 * @param DeltaY Vertical axis delta (keyboard-scale units).
	 * @return "OK: ..." or "ERROR: ...".
	 */
	UFUNCTION(meta = (AICallable))
	static FString InjectMouseDelta(int32 PIEInstance, float DeltaX, float DeltaY);

	/*
	 * Executes a console command inside a PIE instance's world (e.g. "net PktLag=120").
	 * @param PIEInstance The PIE instance index (0-based).
	 * @param Command The console command line.
	 * @return "OK: ..." or "ERROR: ...".
	 */
	UFUNCTION(meta = (AICallable))
	static FString ExecConsole(int32 PIEInstance, const FString& Command);

	/*
	 * Teleports a PIE instance's pawn (server-authoritative) and aims its view.
	 * @param PIEInstance The PIE instance index (0-based).
	 * @param X World X (cm).
	 * @param Y World Y (cm).
	 * @param Z World Z (cm).
	 * @param Yaw Control yaw (degrees).
	 * @param Pitch Control pitch (degrees, negative looks down).
	 * @return "OK: ..." or "ERROR: ...".
	 */
	UFUNCTION(meta = (AICallable))
	static FString TeleportPlayer(int32 PIEInstance, float X, float Y, float Z, float Yaw, float Pitch = 0.f);

	/*
	 * Returns the pawn's location, velocity, MaxWalkSpeed and spray-tank state as JSON
	 * (verification data).
	 * @param PIEInstance The PIE instance index (0-based).
	 * @return JSON: {"x":..,"y":..,"z":..,"speed":..,"maxWalkSpeed":..,"typeI":..,"typeIV":..,"fluid":".."}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString GetPawnInfo(int32 PIEInstance);

	/*
	 * Test hook: drives every surface section of the first aircraft to the given
	 * percentages (server-side, clamped). Used to set up departure/holdover scenarios
	 * without depending on spray timing.
	 * @param CleanPercent Target cleanliness 0-100.
	 * @param ProtectPercent Target protection 0-100.
	 * @return "OK: ..." with resulting section states.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ForceTreatment(float CleanPercent, float ProtectPercent);

	/*
	 * Returns aircraft/section state as JSON in the given PIE instance's world
	 * (instance 0 = authoritative server copy; other instances = replicated client view).
	 * @param PIEInstance The PIE instance index (0-based).
	 * @return JSON: {"holdover":"..","remaining":..,"aircraftZ":..,"sections":[{"name":"..","clean":..,"prot":..},...]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString GetSectionStates(int32 PIEInstance);

	/*
	 * Returns deicing-truck state as JSON in the given PIE instance's world.
	 * @param PIEInstance The PIE instance index (0-based).
	 * @return JSON: {"boomYaw":..,"boomExt":..,"occupied":true/false,"occupant":".."}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString GetTruckState(int32 PIEInstance);
};

// Copyright Epic Games, Inc. All Rights Reserved.


#include "HoldoverPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "HoldoverCameraManager.h"
#include "HoldoverDeicingTruck.h"
#include "Blueprint/UserWidget.h"
#include "Holdover.h"
#include "Widgets/Input/SVirtualJoystick.h"

AHoldoverPlayerController::AHoldoverPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AHoldoverCameraManager::StaticClass();
}

void AHoldoverPlayerController::PawnLeavingGame()
{
	// MP-3/M8: on disconnect the engine destroys the connection's pawn - when
	// the driver was in the deicing truck, that pawn is the LEVEL-PLACED truck.
	// Evict the occupant (destroying it, since its owner is leaving) and detach
	// so the default teardown cannot destroy the truck. This is the single
	// eviction path on disconnect - GameMode::Logout used to duplicate it.
	if (AHoldoverDeicingTruck* Truck = Cast<AHoldoverDeicingTruck>(GetPawn()))
	{
		UE_LOG(LogHoldover, Display, TEXT("PawnLeavingGame: saving truck %s (MP-3/M8)"), *GetNameSafe(Truck));
		Truck->EvictOccupant(/*bDestroyOrphan*/ true);
		UnPossess();
	}
	Super::PawnLeavingGame();
}

void AHoldoverPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogHoldover, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AHoldoverPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AHoldoverPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

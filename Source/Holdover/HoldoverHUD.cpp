// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUD.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UI/HoldoverHUDWidget.h"

void AHoldoverHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const TSubclassOf<UHoldoverHUDWidget> WidgetClass =
		HUDWidgetClass ? HUDWidgetClass : UHoldoverHUDWidget::StaticClass();

	MainWidget = CreateWidget<UHoldoverHUDWidget>(PC, WidgetClass);
	if (MainWidget)
	{
		MainWidget->AddToViewport();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HoldoverHUD: failed to create HUD widget class %s"), *GetNameSafe(WidgetClass.Get()));
	}
}

void AHoldoverHUD::ToggleDepartureBoard()
{
	if (MainWidget)
	{
		MainWidget->ToggleDepartureBoard();
	}
}

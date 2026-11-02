// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverHUDWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UHoldoverDepartureBoardWidget;
class UHoldoverDispatcherFeedWidget;
class UHoldoverShiftResultsWidget;
class UHoldoverInfoWidget;

/**
 * Root UMG HUD. Reads live C++ game state every tick (no duplicated state):
 * shift clock, treatment panel, departure board, dispatcher feed, shift results.
 * Also owns the D1-D3 field manual (tutorial / credits / legal / feedback).
 */
UCLASS()
class HOLDOVER_API UHoldoverHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** D1-D3: toggle the field manual overlay. Idempotent. */
	void ToggleInfoPanel();

protected:
	UPROPERTY(Transient) UCanvasPanel* Root;
	UPROPERTY(Transient) UTextBlock* ShiftLine;
	UPROPERTY(Transient) UTextBlock* TreatmentLine;
	UPROPERTY(Transient) UHoldoverDepartureBoardWidget* Board;
	UPROPERTY(Transient) UHoldoverDispatcherFeedWidget* Feed;
	UPROPERTY(Transient) UHoldoverShiftResultsWidget* Results;
	UPROPERTY(Transient) UHoldoverInfoWidget* Info;

	/** Auto-show the manual once on the first shift so a new player finds it. */
	bool bAutoShowedInfo = false;
};

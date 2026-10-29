// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverAircraft.h"
#include "HoldoverPanelWidgets.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;
class AHoldoverAircraft;

/** Shows the aircraft's treatment state and holdover clock. */
UCLASS()
class HOLDOVER_API UHoldoverDepartureBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Urgency colour ramp restored from the canvas HUD (P7-3):
	 *  running=green, under 30s=yellow, expired=red, otherwise white. */
	static FLinearColor HoldoverStateColor(EHoldoverState State, float RemainingSeconds);

protected:
	UPROPERTY(Transient) UTextBlock* StatusLine;
	UPROPERTY(Transient) UTextBlock* TreatmentLine;
	UPROPERTY(Transient) UTextBlock* SectionLine;

	/** Cached so NativeTick re-scans the world only when the pointer goes stale. */
	UPROPERTY(Transient) TWeakObjectPtr<AHoldoverAircraft> CachedAircraft;
};

/** Last dispatcher lines (radio feed). */
UCLASS()
class HOLDOVER_API UHoldoverDispatcherFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(Transient) UVerticalBox* LineBox;
	UPROPERTY(Transient) TArray<UTextBlock*> Lines;

	static constexpr int32 MaxLines = 5;
};

/** Shift-end results banner (pay, deductions, quota). Hidden until the shift ends. */
UCLASS()
class HOLDOVER_API UHoldoverShiftResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(Transient) UBorder* Panel;
	UPROPERTY(Transient) UTextBlock* VerdictLine;
	UPROPERTY(Transient) UTextBlock* BreakdownLine;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverShiftResultsWidget.generated.h"

class UTextBlock;

/**
 * End-of-shift report in the Vestergaard DTS-dashboard language:
 * KPI rows + a big PASSED / RESTRUCTURED stamp. Visible only when the
 * replicated shift state is ShiftEnded.
 */
UCLASS()
class HOLDOVER_API UHoldoverShiftResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Visibility is parent-driven (P7-1: a Collapsed UserWidget stops
	 * receiving NativeTick, so a self-showing results screen never shows).
	 * The owning HUD widget flips us visible at shift end, then calls this.
	 */
	void RefreshReport();

protected:
	void BuildTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OnTimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GrossText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeductionsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NetText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StampText;
};

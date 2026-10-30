// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverDispatcherFeedWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * Radio-style dispatcher panel (GND DISPATCH 121.9).
 * Reads the replicated GameState feed; no local gameplay state.
 */
UCLASS()
class HOLDOVER_API UHoldoverDispatcherFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Builds the widget tree if it is missing. Initialize() only fires
	 * NativeOnInitialized when PlayerContext is valid, so the parent calls
	 * this directly after Initialize() as a gate-independent guarantee.
	 */
	void EnsureTreeBuilt();

protected:
	void BuildTree();
	void RefreshLines();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LinesBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LineChips;

	int32 LastFeedCount = -1;
};

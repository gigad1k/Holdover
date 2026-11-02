// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverInfoWidget.generated.h"

class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * DEMO_CHECKLIST D2/D3 surface: credits, legal notices and the feedback route,
 * plus the D1 first-shift tutorial. Text-only (content lives in
 * UHoldoverInfoContent). Built in C++ like the other Holdover panel widgets.
 *
 * Visibility is toggled by the owning controller; the widget itself never
 * mutates game state.
 */
UCLASS()
class HOLDOVER_API UHoldoverInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Show every section. Safe to call every frame; cheap. */
	void RefreshContent();

	/** First-shift tutorial shown as its own section. */
	void SetTipsVisible(bool bVisible);

protected:
	UPROPERTY(Transient) UVerticalBox* Box;
	UPROPERTY(Transient) UTextBlock* Title;
	UPROPERTY(Transient) TArray<UTextBlock*> BodyLines;
};

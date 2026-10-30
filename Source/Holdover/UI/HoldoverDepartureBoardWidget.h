// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverDepartureBoardWidget.generated.h"

class AHoldoverAircraft;
class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * Fullscreen FIDS-style departure board (Tab to toggle).
 * One row per aircraft: flight, status, holdover clock, cleanliness, protection.
 */
UCLASS()
class HOLDOVER_API UHoldoverDepartureBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	struct FDepartureRow
	{
		TWeakObjectPtr<AHoldoverAircraft> Aircraft;
		TObjectPtr<UTextBlock> Flight;
		TObjectPtr<UTextBlock> Status;
		TObjectPtr<UTextBlock> Holdover;
		TObjectPtr<UTextBlock> Clean;
		TObjectPtr<UTextBlock> Prot;
	};

	void BuildTree();
	void RebuildRows();
	FString GetAircraftSignature() const;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowsBox;

	TArray<FDepartureRow> Rows;

	FString LastSignature;
	float RefreshTimer = 0.f;
};

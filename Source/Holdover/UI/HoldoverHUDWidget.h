// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldoverHUDWidget.generated.h"

class AHoldoverAircraft;
class AHoldoverSurfaceSection;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UOverlay;
class UTextBlock;
class UWidget;
class UWidgetTree;

/** One fluid gauge: size box + fill border + its canvas slot (fractional anchor span). */
struct FHoldoverGauge
{
	TObjectPtr<UWidget> Box;
	TObjectPtr<UBorder> Fill;
	TObjectPtr<UCanvasPanelSlot> FillSlot;
};

/** One aircraft surface cell in the OPTIM-ICE-style scan map. */
struct FHoldoverScanCell
{
	TWeakObjectPtr<AHoldoverSurfaceSection> Section;
	TObjectPtr<UBorder> Box;
	TObjectPtr<UTextBlock> Label;
	FVector2D PanelPos = FVector2D::ZeroVector;
};

/**
 * Main ramp-ops HUD (Vestergaard-style): holdover board, tank gauges,
 * fluid chips, surface scan map, shift clock, warning border.
 * Purely presentational — reads replicated C++ state only.
 */
UCLASS()
class HOLDOVER_API UHoldoverHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** See UHoldoverDispatcherFeedWidget::EnsureTreeBuilt — gate-independent tree build. */
	void EnsureTreeBuilt();

	/** Tab toggle for the fullscreen departure board. */
	void ToggleDepartureBoard();

protected:
	void BuildTree();
	void BuildCenterStack(UCanvasPanel* Root);
	void BuildHoldoverBoard(UCanvasPanel* Root);
	void BuildRampConsole(UCanvasPanel* Root);
	void BuildClockPanel(UCanvasPanel* Root);
	void BuildWarningBorder(UCanvasPanel* Root);

	FHoldoverGauge MakeGauge(float Width, float Height);
	void RefreshAircraftCache(float DeltaSeconds);
	void RebuildScanCells();
	void UpdateCenterStack();
	void UpdateHoldoverBoard();
	void UpdateConsole();
	void UpdateClock();
	void UpdateScan();
	void UpdateWarningBorder(float DeltaSeconds);

	// --- state cache (cosmetic only) ---
	TWeakObjectPtr<AHoldoverAircraft> PrimaryAircraft;
	TArray<FHoldoverScanCell> ScanCells;
	float AircraftRefreshTimer = 0.f;

	// --- reticle / centre stack ---
	TObjectPtr<UBorder> ReticleH;
	TObjectPtr<UBorder> ReticleV;
	TObjectPtr<UTextBlock> DistanceText;
	TObjectPtr<UBorder> PromptPlate;
	TObjectPtr<UTextBlock> PromptText;
	TObjectPtr<UTextBlock> HintText;
	TObjectPtr<UTextBlock> TargetText;

	// --- holdover board ---
	TObjectPtr<UTextBlock> HoldoverDigits;
	TObjectPtr<UTextBlock> HoldoverStateLabel;

	// --- ramp console ---
	TObjectPtr<UBorder> ChipI;
	TObjectPtr<UBorder> ChipIV;
	TObjectPtr<UTextBlock> ChipIText;
	TObjectPtr<UTextBlock> ChipIVText;
	FHoldoverGauge GaugeI;
	FHoldoverGauge GaugeIV;
	TObjectPtr<UTextBlock> WeatherText;

	// --- shift clock ---
	TObjectPtr<UTextBlock> ClockTime;
	TObjectPtr<UTextBlock> ClockDepartures;
	TObjectPtr<UTextBlock> ClockPay;

	// --- scan map ---
	TObjectPtr<UOverlay> ScanOverlay;
	TObjectPtr<UTextBlock> ScanAircraftLabel;

	// --- warnings / overlays ---
	TObjectPtr<UBorder> WarningBorder;
	float WarningPulsePhase = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> FeedWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BoardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultsWidget;
};

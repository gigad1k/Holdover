// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HoldoverHUDWidget.h"
#include "UI/HoldoverDepartureBoardWidget.h"
#include "UI/HoldoverDispatcherFeedWidget.h"
#include "UI/HoldoverShiftResultsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HoldoverAircraft.h"
#include "HoldoverCharacter.h"
#include "HoldoverGameState.h"
#include "HoldoverHUD.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"
#include "HoldoverUIStyle.h"

namespace
{
	constexpr float ScanPanelW = 236.f;
	constexpr float ScanPanelH = 110.f;

	/** Vestergaard traffic-light state colour for one surface section. */
	FLinearColor SectionStateColor(const AHoldoverSurfaceSection* Section)
	{
		if (!Section)
		{
			return HoldoverUI::StateGray();
		}
		if (Section->IsProtected())
		{
			return HoldoverUI::StateBlue();
		}
		if (Section->IsClean())
		{
			return HoldoverUI::StateGreen();
		}
		if (Section->CleanlinessPercent > 1.f)
		{
			return HoldoverUI::StateYellow();
		}
		return HoldoverUI::StateGray();
	}

	const TCHAR* HoldoverStateText(EHoldoverState State)
	{
		switch (State)
		{
		case EHoldoverState::HoldoverActive: return TEXT("HOLDOVER RUNNING");
		case EHoldoverState::HoldoverExpired: return TEXT("HOLDOVER EXPIRED - RETREAT");
		case EHoldoverState::Departed: return TEXT("DEPARTED");
		default: return TEXT("TREATMENT INCOMPLETE");
		}
	}
}

void UHoldoverHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
}

void UHoldoverHUDWidget::BuildTree()
{
	using namespace HoldoverUI;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());

	BuildCenterStack(Root);
	BuildHoldoverBoard(Root);
	BuildRampConsole(Root);
	BuildClockPanel(Root);

	// Radio feed, mid-left.
	FeedWidget = WidgetTree->ConstructWidget<UHoldoverDispatcherFeedWidget>(UHoldoverDispatcherFeedWidget::StaticClass());
	if (FeedWidget)
	{
		FeedWidget->Initialize(); // ConstructWidget children skip CreateWidget; init builds their trees now.
	}
	UCanvasPanelSlot* FeedSlot = Root->AddChildToCanvas(FeedWidget);
	FeedSlot->SetAnchors(FAnchors(0.f, 0.5f));
	FeedSlot->SetAlignment(FVector2D(0.f, 0.5f));
	FeedSlot->SetPosition(FVector2D(24.f, 0.f));
	FeedSlot->SetAutoSize(true);

	// Fullscreen departure board (Tab), hidden by default.
	BoardWidget = WidgetTree->ConstructWidget<UHoldoverDepartureBoardWidget>(UHoldoverDepartureBoardWidget::StaticClass());
	if (BoardWidget)
	{
		BoardWidget->Initialize();
	}
	UCanvasPanelSlot* BoardSlot = Root->AddChildToCanvas(BoardWidget);
	BoardSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BoardSlot->SetAlignment(FVector2D(0.f, 0.f));
	BoardSlot->SetPosition(FVector2D(0.f, 0.f));
	BoardSlot->SetAutoSize(false);
	BoardWidget->SetVisibility(ESlateVisibility::Collapsed);

	// Shift report (self-manages visibility from the shift state).
	ResultsWidget = WidgetTree->ConstructWidget<UHoldoverShiftResultsWidget>(UHoldoverShiftResultsWidget::StaticClass());
	if (ResultsWidget)
	{
		ResultsWidget->Initialize();
	}
	UCanvasPanelSlot* ResultsSlot = Root->AddChildToCanvas(ResultsWidget);
	ResultsSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ResultsSlot->SetAlignment(FVector2D(0.f, 0.f));
	ResultsSlot->SetPosition(FVector2D(0.f, 0.f));
	ResultsSlot->SetAutoSize(false);

	// Warning border on top of everything.
	BuildWarningBorder(Root);

	WidgetTree->RootWidget = Root;
}

void UHoldoverHUDWidget::BuildCenterStack(UCanvasPanel* Root)
{
	using namespace HoldoverUI;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// Reticle cross (20x20, arms centred; sizeboxes give the contentless bars a desired size).
	UOverlay* Cross = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ReticleH = MakeSquare(WidgetTree, TextPrimary());
	UOverlaySlot* HSlot = Cross->AddChildToOverlay(MakeFixed(WidgetTree, 18.f, 2.f, ReticleH));
	HSlot->SetHorizontalAlignment(HAlign_Center);
	HSlot->SetVerticalAlignment(VAlign_Center);
	ReticleV = MakeSquare(WidgetTree, TextPrimary());
	UOverlaySlot* VSlot = Cross->AddChildToOverlay(MakeFixed(WidgetTree, 2.f, 18.f, ReticleV));
	VSlot->SetHorizontalAlignment(HAlign_Center);
	VSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBoxSlot* CrossSlot = Stack->AddChildToVerticalBox(MakeFixed(WidgetTree, 20.f, 20.f, Cross));
	CrossSlot->SetHorizontalAlignment(HAlign_Center);

	DistanceText = MakeText(WidgetTree, TEXT(""), 11, TextPrimary(), true);
	UVerticalBoxSlot* DistSlot = Stack->AddChildToVerticalBox(DistanceText);
	DistSlot->SetHorizontalAlignment(HAlign_Center);

	// Gate-sign interaction prompt plate.
	PromptText = MakeText(WidgetTree, TEXT(""), 15, TextPrimary(), true);
	PromptPlate = MakePanel(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.72f), 10.f, 4.f);
	PromptPlate->AddChild(PromptText);
	UVerticalBoxSlot* PromptSlot = Stack->AddChildToVerticalBox(PromptPlate);
	PromptSlot->SetHorizontalAlignment(HAlign_Center);
	PromptSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	HintText = MakeText(WidgetTree, TEXT(""), 12, StateYellow(), true);
	UVerticalBoxSlot* HintSlot = Stack->AddChildToVerticalBox(HintText);
	HintSlot->SetHorizontalAlignment(HAlign_Center);

	TargetText = MakeText(WidgetTree, TEXT(""), 11, TextDim());
	UVerticalBoxSlot* TargetSlot = Stack->AddChildToVerticalBox(TargetText);
	TargetSlot->SetHorizontalAlignment(HAlign_Center);

	UCanvasPanelSlot* StackSlot = Root->AddChildToCanvas(Stack);
	StackSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	StackSlot->SetAlignment(FVector2D(0.5f, 0.f));
	StackSlot->SetPosition(FVector2D(0.f, -10.f));
	StackSlot->SetAutoSize(true);
}

void UHoldoverHUDWidget::BuildHoldoverBoard(UCanvasPanel* Root)
{
	using namespace HoldoverUI;

	UBorder* Board = MakePanel(WidgetTree, PanelBackground(), 16.f, 6.f);
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Board->AddChild(VBox);

	UTextBlock* Header = MakeText(WidgetTree, TEXT("HOLDOVER"), 10, TextDim(), true);
	UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(Header);
	HeaderSlot->SetHorizontalAlignment(HAlign_Center);

	HoldoverDigits = MakeText(WidgetTree, TEXT("--:--"), 40, AmberLED(), true);
	UVerticalBoxSlot* DigitsSlot = VBox->AddChildToVerticalBox(HoldoverDigits);
	DigitsSlot->SetHorizontalAlignment(HAlign_Center);

	HoldoverStateLabel = MakeText(WidgetTree, TEXT("NO AIRCRAFT ON PAD"), 12, TextDim(), true);
	UVerticalBoxSlot* LabelSlot = VBox->AddChildToVerticalBox(HoldoverStateLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);

	UCanvasPanelSlot* BoardSlot = Root->AddChildToCanvas(Board);
	BoardSlot->SetAnchors(FAnchors(0.5f, 0.f));
	BoardSlot->SetAlignment(FVector2D(0.5f, 0.f));
	BoardSlot->SetPosition(FVector2D(0.f, 16.f));
	BoardSlot->SetAutoSize(true);
}

void UHoldoverHUDWidget::BuildRampConsole(UCanvasPanel* Root)
{
	using namespace HoldoverUI;

	// Bottom-left stack: console panel above the scan panel.
	UVerticalBox* Corner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// --- ramp console ---
	UBorder* Console = MakePanel(WidgetTree, PanelBackground(), 10.f, 6.f);
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Console->AddChild(VBox);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VBox->AddChild(HeaderRow);
	UTextBlock* Header = MakeText(WidgetTree, TEXT("RAMP CONSOLE"), 11, TextDim(), true);
	UHorizontalBoxSlot* HeaderSlot = HeaderRow->AddChildToHorizontalBox(Header);
	HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HeaderSlot->SetVerticalAlignment(VAlign_Center);
	WeatherText = MakeText(WidgetTree, TEXT("WX SEV --"), 11, TextDim(), true);
	UHorizontalBoxSlot* WxSlot = HeaderRow->AddChildToHorizontalBox(WeatherText);
	WxSlot->SetVerticalAlignment(VAlign_Center);

	// Fluid chips.
	UHorizontalBox* ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VBox->AddChild(ChipRow);
	ChipIText = MakeText(WidgetTree, TEXT("TYPE I  CLEAN"), 12, StateGray(), true);
	ChipI = MakeChip(WidgetTree, ChipIText, FLinearColor(0.f, 0.f, 0.f, 0.5f), 8.f, 3.f);
	UHorizontalBoxSlot* ChipISlot = ChipRow->AddChildToHorizontalBox(ChipI);
	ChipISlot->SetPadding(FMargin(0.f, 6.f, 6.f, 0.f));
	ChipIVText = MakeText(WidgetTree, TEXT("TYPE IV  PROTECT"), 12, StateGray(), true);
	ChipIV = MakeChip(WidgetTree, ChipIVText, FLinearColor(0.f, 0.f, 0.f, 0.5f), 8.f, 3.f);
	UHorizontalBoxSlot* ChipIVSlot = ChipRow->AddChildToHorizontalBox(ChipIV);
	ChipIVSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	// Tank gauges.
	UHorizontalBox* GaugeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VBox->AddChild(GaugeRow);

	UVerticalBox* ColumnI = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* CapI = MakeText(WidgetTree, TEXT("TYPE I TANK"), 9, TextDim());
	UVerticalBoxSlot* CapISlot = ColumnI->AddChildToVerticalBox(CapI);
	CapISlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	GaugeI = MakeGauge(106.f, 12.f);
	ColumnI->AddChild(GaugeI.Box);
	UHorizontalBoxSlot* ColumnISlot = GaugeRow->AddChildToHorizontalBox(ColumnI);
	ColumnISlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));

	UVerticalBox* ColumnIV = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* CapIV = MakeText(WidgetTree, TEXT("TYPE IV TANK"), 9, TextDim());
	UVerticalBoxSlot* CapIVSlot = ColumnIV->AddChildToVerticalBox(CapIV);
	CapIVSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	GaugeIV = MakeGauge(106.f, 12.f);
	ColumnIV->AddChild(GaugeIV.Box);
	GaugeRow->AddChild(ColumnIV);

	UVerticalBoxSlot* ConsoleSlot = Corner->AddChildToVerticalBox(Console);

	// --- scan panel ---
	UBorder* Scan = MakePanel(WidgetTree, PanelBackground(), 10.f, 6.f);
	UVerticalBox* ScanVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scan->AddChild(ScanVBox);

	UHorizontalBox* ScanHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ScanVBox->AddChild(ScanHeader);
	UTextBlock* ScanTitle = MakeText(WidgetTree, TEXT("SURFACE SCAN"), 11, TextDim(), true);
	UHorizontalBoxSlot* ScanTitleSlot = ScanHeader->AddChildToHorizontalBox(ScanTitle);
	ScanTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScanTitleSlot->SetVerticalAlignment(VAlign_Center);
	ScanAircraftLabel = MakeText(WidgetTree, TEXT("NO AIRCRAFT"), 10, TextDim());
	UHorizontalBoxSlot* ScanNameSlot = ScanHeader->AddChildToHorizontalBox(ScanAircraftLabel);
	ScanNameSlot->SetVerticalAlignment(VAlign_Center);

	ScanOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ScanVBox->AddChild(MakeFixed(WidgetTree, ScanPanelW, ScanPanelH, ScanOverlay));

	UVerticalBoxSlot* ScanSlot = Corner->AddChildToVerticalBox(Scan);
	ScanSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	UCanvasPanelSlot* CornerSlot = Root->AddChildToCanvas(Corner);
	CornerSlot->SetAnchors(FAnchors(0.f, 1.f));
	CornerSlot->SetAlignment(FVector2D(0.f, 1.f));
	CornerSlot->SetPosition(FVector2D(24.f, -24.f));
	CornerSlot->SetAutoSize(true);
}

void UHoldoverHUDWidget::BuildClockPanel(UCanvasPanel* Root)
{
	using namespace HoldoverUI;

	UBorder* Clock = MakePanel(WidgetTree, PanelBackground(), 12.f, 6.f);
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Clock->AddChild(VBox);

	UTextBlock* Header = MakeText(WidgetTree, TEXT("SHIFT CLOCK"), 10, TextDim(), true);
	UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(Header);
	HeaderSlot->SetHorizontalAlignment(HAlign_Center);

	ClockTime = MakeText(WidgetTree, TEXT("--:--"), 24, TextPrimary(), true);
	UVerticalBoxSlot* TimeSlot = VBox->AddChildToVerticalBox(ClockTime);
	TimeSlot->SetHorizontalAlignment(HAlign_Center);

	ClockDepartures = MakeText(WidgetTree, TEXT("DEPARTURES 0 / 1"), 12, TextPrimary(), true);
	UVerticalBoxSlot* DepSlot = VBox->AddChildToVerticalBox(ClockDepartures);
	DepSlot->SetHorizontalAlignment(HAlign_Center);

	ClockPay = MakeText(WidgetTree, TEXT("PAY $0  DED -$0"), 11, TextDim());
	UVerticalBoxSlot* PaySlot = VBox->AddChildToVerticalBox(ClockPay);
	PaySlot->SetHorizontalAlignment(HAlign_Center);

	UCanvasPanelSlot* ClockSlot = Root->AddChildToCanvas(Clock);
	ClockSlot->SetAnchors(FAnchors(1.f, 1.f));
	ClockSlot->SetAlignment(FVector2D(1.f, 1.f));
	ClockSlot->SetPosition(FVector2D(-24.f, -24.f));
	ClockSlot->SetAutoSize(true);
}

FHoldoverGauge UHoldoverHUDWidget::MakeGauge(float Width, float Height)
{
	using namespace HoldoverUI;

	FHoldoverGauge Gauge;

	USizeBox* Box = MakeFixed(WidgetTree, Width, Height, nullptr);
	Gauge.Box = Box;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Box->AddChild(Canvas);

	UBorder* Bg = MakeSquare(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Bg);
	BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BgSlot->SetAlignment(FVector2D(0.f, 0.f));
	BgSlot->SetPosition(FVector2D(0.f, 0.f));
	BgSlot->SetAutoSize(false);

	Gauge.Fill = MakeSquare(WidgetTree, StateYellow());
	UCanvasPanelSlot* FillSlot = Canvas->AddChildToCanvas(Gauge.Fill);
	FillSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	FillSlot->SetAlignment(FVector2D(0.f, 0.f));
	FillSlot->SetPosition(FVector2D(0.f, 0.f));
	FillSlot->SetAutoSize(false);
	Gauge.FillSlot = FillSlot;

	return Gauge;
}

void UHoldoverHUDWidget::BuildWarningBorder(UCanvasPanel* Root)
{
	using namespace HoldoverUI;

	WarningBorder = MakeSquare(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f));
	UCanvasPanelSlot* Slot = Root->AddChildToCanvas(WarningBorder);
	Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	Slot->SetAlignment(FVector2D(0.f, 0.f));
	Slot->SetPosition(FVector2D(0.f, 0.f));
	Slot->SetAutoSize(false);
}

void UHoldoverHUDWidget::ToggleDepartureBoard()
{
	if (BoardWidget)
	{
		const bool bVisible = BoardWidget->GetVisibility() != ESlateVisibility::Collapsed;
		BoardWidget->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UHoldoverHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Tree must exist before any update dereferences members.
	if (!WidgetTree || !WidgetTree->RootWidget || !ReticleH)
	{
		return;
	}

	// P7-1: a Collapsed UserWidget stops receiving NativeTick, so the results
	// banner must never drive its own visibility — this root widget always
	// ticks, so it owns the show/hide (and refreshes content on the frame
	// the banner appears).
	if (ResultsWidget)
	{
		const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
		const bool bShowResults = GS && GS->ShiftState == EShiftState::ShiftEnded;
		const ESlateVisibility Desired = bShowResults ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (ResultsWidget->GetVisibility() != Desired)
		{
			ResultsWidget->SetVisibility(Desired);
			if (bShowResults)
			{
				if (UHoldoverShiftResultsWidget* Report = Cast<UHoldoverShiftResultsWidget>(ResultsWidget.Get()))
				{
					Report->RefreshReport();
				}
			}
		}
	}

	RefreshAircraftCache(InDeltaTime);
	UpdateCenterStack();
	UpdateHoldoverBoard();
	UpdateConsole();
	UpdateClock();
	UpdateScan();
	UpdateWarningBorder(InDeltaTime);
}

void UHoldoverHUDWidget::RefreshAircraftCache(float DeltaSeconds)
{
	AircraftRefreshTimer -= DeltaSeconds;
	if (AircraftRefreshTimer > 0.f)
	{
		return;
	}
	AircraftRefreshTimer = 0.5f;

	AHoldoverAircraft* Found = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHoldoverAircraft> It(World); It; ++It)
		{
			Found = *It;
			break;
		}
	}

	if (Found != PrimaryAircraft.Get())
	{
		PrimaryAircraft = Found;
		RebuildScanCells();
		return;
	}

	// Same aircraft: rebuild if sections registered late (BeginPlay ordering).
	const AHoldoverAircraft* Aircraft = PrimaryAircraft.Get();
	if ((Aircraft ? Aircraft->GetSections().Num() : 0) != ScanCells.Num())
	{
		RebuildScanCells();
	}
}

void UHoldoverHUDWidget::RebuildScanCells()
{
	if (!ScanOverlay)
	{
		return;
	}

	while (ScanOverlay->GetChildrenCount() > 0)
	{
		ScanOverlay->RemoveChildAt(0);
	}
	ScanCells.Reset();

	using namespace HoldoverUI;

	const AHoldoverAircraft* Aircraft = PrimaryAircraft.Get();
	if (ScanAircraftLabel)
	{
		FString Name = Aircraft ? Aircraft->GetName() : FString();
		int32 Underscore;
		if (Name.FindLastChar(TEXT('_'), Underscore))
		{
			Name = Name.Mid(Underscore + 1);
		}
		ScanAircraftLabel->SetText(Aircraft
			? FText::FromString(FString::Printf(TEXT("AIRCRAFT %s"), *Name))
			: FText::FromString(TEXT("NO AIRCRAFT")));
	}
	if (!Aircraft)
	{
		return;
	}

	const TArray<AHoldoverSurfaceSection*>& Sections = Aircraft->GetSections();
	FVector2D BoundsMin(0.f, 0.f);
	FVector2D BoundsMax(0.f, 0.f);
	int32 ValidCount = 0;
	for (const AHoldoverSurfaceSection* Section : Sections)
	{
		if (Section)
		{
			const FVector2D Loc2D(Section->GetActorLocation().X, Section->GetActorLocation().Y);
			if (ValidCount == 0)
			{
				BoundsMin = Loc2D;
				BoundsMax = Loc2D;
			}
			else
			{
				BoundsMin.X = FMath::Min(BoundsMin.X, Loc2D.X);
				BoundsMin.Y = FMath::Min(BoundsMin.Y, Loc2D.Y);
				BoundsMax.X = FMath::Max(BoundsMax.X, Loc2D.X);
				BoundsMax.Y = FMath::Max(BoundsMax.Y, Loc2D.Y);
			}
			++ValidCount;
		}
	}
	if (ValidCount == 0)
	{
		return;
	}

	// Map world XY into the scan area (centred, +Y up, margin for chip halves).
	const FVector2D Size = BoundsMax - BoundsMin;
	const float AreaW = ScanPanelW - 40.f;
	const float AreaH = ScanPanelH - 28.f;
	float Scale = 1.f;
	if (Size.X > 1.f && Size.Y > 1.f)
	{
		Scale = FMath::Min(AreaW / Size.X, AreaH / Size.Y);
	}
	const float OffsetX = 20.f + (AreaW - Size.X * Scale) * 0.5f;
	const float OffsetY = 14.f + (AreaH - Size.Y * Scale) * 0.5f;

	for (AHoldoverSurfaceSection* Section : Sections)
	{
		if (!Section)
		{
			continue;
		}

		const FVector Location = Section->GetActorLocation();
		FVector2D Rel((Location.X - BoundsMin.X) * Scale, (Location.Y - BoundsMin.Y) * Scale);
		Rel.Y = (Size.Y * Scale) - Rel.Y;
		const FVector2D Pos(OffsetX + Rel.X, OffsetY + Rel.Y);

		FHoldoverScanCell Cell;
		Cell.Section = Section;
		Cell.PanelPos = Pos;

		const FString LabelStr = Section->SectionName.ToString().Left(3).ToUpper();
		Cell.Label = MakeText(WidgetTree, LabelStr, 9, TextPrimary(), true);
		Cell.Box = MakeChip(WidgetTree, Cell.Label, StateGray());

		USizeBox* ChipBox = MakeFixed(WidgetTree, 40.f, 20.f, Cell.Box);
		UOverlaySlot* Slot = ScanOverlay->AddChildToOverlay(ChipBox);
		Slot->SetHorizontalAlignment(HAlign_Left);
		Slot->SetVerticalAlignment(VAlign_Top);
		Slot->SetPadding(FMargin(Pos.X - 20.f, Pos.Y - 10.f));

		ScanCells.Add(Cell);
	}
}

void UHoldoverHUDWidget::UpdateCenterStack()
{
	using namespace HoldoverUI;

	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr;

	FLinearColor ReticleColor = TextPrimary();
	FString DistanceStr;
	FString PromptStr;
	FString HintStr;
	FString TargetStr;
	FLinearColor TargetColor = TextDim();

	// The character writes the prompt from its own tick; while possessing the
	// truck that writer isn't ticking, so an unconditional read shows a stale
	// prompt (2026-10-29 review, lower-severity list). Only show it on foot.
	if (Cast<AHoldoverCharacter>(Pawn))
	{
		if (const AHoldoverHUD* HUD = Cast<AHoldoverHUD>(PC ? PC->GetHUD() : nullptr))
		{
			PromptStr = HUD->InteractionPrompt.ToString();
		}
	}

	if (Spray)
	{
		AHoldoverSurfaceSection* Target = Spray->CurrentTargetSection.Get();

		if (!Spray->TargetHint.IsEmpty())
		{
			HintStr = Spray->TargetHint.ToString();
			ReticleColor = StateYellow();
		}
		else if (Target)
		{
			ReticleColor = StateGreen();
		}

		if (Target)
		{
			if (PC)
			{
				FVector ViewLoc;
				FRotator ViewRot;
				PC->GetPlayerViewPoint(ViewLoc, ViewRot);
				const float Metres = FVector::Dist(ViewLoc, Target->GetActorLocation()) / 100.f;
				DistanceStr = FString::Printf(TEXT("%.1f M"), Metres);
			}
			TargetStr = FString::Printf(TEXT("TARGET %s   CLEAN %.0f%%   PROT %.0f%%"),
				*Target->SectionName.ToString().ToUpper(),
				Target->CleanlinessPercent, Target->ProtectionPercent);
			TargetColor = SectionStateColor(Target);
		}
		else if (Spray->IsSpraying())
		{
			TargetStr = TEXT("NO TARGET");
			TargetColor = StateRed();
		}
	}

	if (ReticleH)
	{
		ReticleH->SetBrush(MakeBoxBrush(ReticleColor));
	}
	if (ReticleV)
	{
		ReticleV->SetBrush(MakeBoxBrush(ReticleColor));
	}

	DistanceText->SetText(FText::FromString(DistanceStr));
	DistanceText->SetVisibility(DistanceStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	PromptText->SetText(FText::FromString(PromptStr));
	PromptPlate->SetVisibility(PromptStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	HintText->SetText(FText::FromString(HintStr));
	HintText->SetVisibility(HintStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	TargetText->SetText(FText::FromString(TargetStr));
	TargetText->SetColorAndOpacity(FSlateColor(TargetColor));
	TargetText->SetVisibility(TargetStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UHoldoverHUDWidget::UpdateHoldoverBoard()
{
	using namespace HoldoverUI;

	const AHoldoverAircraft* Aircraft = PrimaryAircraft.Get();
	if (!Aircraft)
	{
		HoldoverDigits->SetText(FText::FromString(TEXT("--:--")));
		HoldoverDigits->SetColorAndOpacity(FSlateColor(TextDim()));
		HoldoverStateLabel->SetText(FText::FromString(TEXT("NO AIRCRAFT ON PAD")));
		HoldoverStateLabel->SetColorAndOpacity(FSlateColor(TextDim()));
		return;
	}

	FString Digits;
	FLinearColor DigitsColor = AmberLED();
	FLinearColor LabelColor = TextDim();

	switch (Aircraft->HoldoverState)
	{
	case EHoldoverState::HoldoverActive:
		Digits = FormatHoldover(Aircraft->HoldoverRemaining);
		LabelColor = Aircraft->HoldoverRemaining < 30.f ? StateYellow() : StateGreen();
		break;
	case EHoldoverState::HoldoverExpired:
		Digits = FormatHoldover(0.f);
		DigitsColor = StateRed();
		LabelColor = StateRed();
		break;
	default:
		Digits = TEXT("--:--");
		DigitsColor = TextDim();
		LabelColor = Aircraft->HoldoverState == EHoldoverState::Departed ? TextDim() : StateGray();
		break;
	}

	HoldoverDigits->SetText(FText::FromString(Digits));
	HoldoverDigits->SetColorAndOpacity(FSlateColor(DigitsColor));
	HoldoverStateLabel->SetText(FText::FromString(HoldoverStateText(Aircraft->HoldoverState)));
	HoldoverStateLabel->SetColorAndOpacity(FSlateColor(LabelColor));
}

void UHoldoverHUDWidget::UpdateConsole()
{
	using namespace HoldoverUI;

	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr;

	// Fluid chips: active fluid is the "enabled" item (green), per the HMI semantics.
	const bool bTypeI = !Spray || Spray->ActiveFluidType == EHoldoverFluidType::TypeI;
	auto SetChip = [&](UBorder* Chip, UTextBlock* Text, bool bActive)
	{
		if (!Chip)
		{
			return;
		}
		const FLinearColor Fill = bActive
			? FLinearColor(StateGreen().R, StateGreen().G, StateGreen().B, 0.16f)
			: FLinearColor(0.f, 0.f, 0.f, 0.5f);
		Chip->SetBrush(MakeBoxBrush(Fill, bActive ? StateGreen() : PanelOutline(), bActive ? 2.f : 1.f));
		if (Text)
		{
			Text->SetColorAndOpacity(FSlateColor(bActive ? StateGreen() : TextDim()));
		}
	};
	SetChip(ChipI, ChipIText, bTypeI);
	SetChip(ChipIV, ChipIVText, !bTypeI);

	// Tank gauges: fractional anchor span, red under 10%.
	auto SetGauge = [&](FHoldoverGauge& Gauge, float Normalized, const FLinearColor& BaseColor)
	{
		const float Pct = FMath::Clamp(Normalized, 0.f, 1.f);
		if (Gauge.FillSlot)
		{
			Gauge.FillSlot->SetAnchors(FAnchors(0.f, 0.f, Pct, 1.f));
		}
		if (Gauge.Fill)
		{
			Gauge.Fill->SetBrush(MakeBoxBrush(Pct < 0.10f ? StateRed() : BaseColor));
		}
	};
	if (Spray)
	{
		const float Capacity = FMath::Max(1.f, Spray->FluidCapacity);
		SetGauge(GaugeI, Spray->TypeIFluid / Capacity, StateYellow());
		SetGauge(GaugeIV, Spray->TypeIVFluid / Capacity, StateBlue());
	}
	else
	{
		SetGauge(GaugeI, 0.f, StateGray());
		SetGauge(GaugeIV, 0.f, StateGray());
	}

	// Weather severity readout.
	if (const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr)
	{
		const float Severity = GS->WeatherSeverity;
		WeatherText->SetText(FText::FromString(FString::Printf(TEXT("WX SEV %.0f%%"), Severity * 100.f)));
		WeatherText->SetColorAndOpacity(FSlateColor(
			Severity >= 0.7f ? StateRed() : (Severity >= 0.35f ? StateYellow() : TextDim())));
	}
}

void UHoldoverHUDWidget::UpdateClock()
{
	using namespace HoldoverUI;

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	ClockTime->SetText(FText::FromString(FormatClock(GS->ShiftTimeRemaining)));
	if (GS->ShiftState == EShiftState::ShiftEnded)
	{
		ClockTime->SetColorAndOpacity(FSlateColor(GS->bShiftPassed ? StateGreen() : StateRed()));
	}
	else
	{
		ClockTime->SetColorAndOpacity(FSlateColor(GS->ShiftTimeRemaining < 30.f ? StateYellow() : TextPrimary()));
	}

	ClockDepartures->SetText(FText::FromString(
		FString::Printf(TEXT("DEPARTURES %d / %d"), GS->OnTimeDepartures, GS->Quota)));
	ClockDepartures->SetColorAndOpacity(FSlateColor(
		GS->OnTimeDepartures >= GS->Quota ? StateGreen() : TextPrimary()));

	ClockPay->SetText(FText::FromString(
		FString::Printf(TEXT("PAY $%d  DED -$%d"), GS->Money, GS->Deductions)));
}

void UHoldoverHUDWidget::UpdateScan()
{
	using namespace HoldoverUI;

	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr;
	const AHoldoverSurfaceSection* Target = Spray ? Spray->CurrentTargetSection.Get() : nullptr;

	for (FHoldoverScanCell& Cell : ScanCells)
	{
		const AHoldoverSurfaceSection* Section = Cell.Section.Get();
		if (!Section || !Cell.Box)
		{
			continue;
		}

		const FLinearColor State = SectionStateColor(Section);
		const bool bTarget = (Section == Target);
		Cell.Box->SetBrush(MakeBoxBrush(State,
			bTarget ? FLinearColor::White : FLinearColor(0.f, 0.f, 0.f, 0.75f),
			bTarget ? 2.f : 1.f));
	}
}

void UHoldoverHUDWidget::UpdateWarningBorder(float DeltaSeconds)
{
	using namespace HoldoverUI;

	const AHoldoverAircraft* Aircraft = PrimaryAircraft.Get();
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr;
	const bool bLowTank = Spray && (Spray->TypeIFluid <= 5.f || Spray->TypeIVFluid <= 5.f);

	FLinearColor Warning(0.f, 0.f, 0.f, 0.f);
	if (Aircraft && Aircraft->HoldoverState == EHoldoverState::HoldoverExpired)
	{
		// Pulsing red border while the holdover clock has run out.
		WarningPulsePhase += DeltaSeconds * 5.f;
		const float Alpha = 0.14f + 0.10f * FMath::Sin(WarningPulsePhase);
		Warning = FLinearColor(StateRed().R, StateRed().G, StateRed().B, Alpha);
	}
	else if (bLowTank)
	{
		// Steady dim yellow while a tank is nearly dry.
		Warning = FLinearColor(StateYellow().R, StateYellow().G, StateYellow().B, 0.08f);
	}

	if (WarningBorder)
	{
		WarningBorder->SetBrush(MakeBoxBrush(Warning));
	}
}

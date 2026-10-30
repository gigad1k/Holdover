// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HoldoverDepartureBoardWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverUIStyle.h"

namespace
{
	/** "HoldoverAircraft_2" -> "FLT 2". */
	FString FlightLabel(const AActor* Aircraft)
	{
		if (!Aircraft)
		{
			return TEXT("FLT ??");
		}
		FString Name = Aircraft->GetName();
		int32 Underscore;
		if (Name.FindLastChar(TEXT('_'), Underscore))
		{
			Name = Name.Mid(Underscore + 1);
		}
		return FString::Printf(TEXT("FLT %s"), *Name);
	}
}

void UHoldoverDepartureBoardWidget::NativeOnInitialized()
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

void UHoldoverDepartureBoardWidget::BuildTree()
{
	using namespace HoldoverUI;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());

	// Dim backdrop so the ramp stays visible behind the board.
	UBorder* Backdrop = MakeSquare(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.72f));
	UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackdropSlot->SetAlignment(FVector2D(0.f, 0.f));
	BackdropSlot->SetPosition(FVector2D(0.f, 0.f));
	BackdropSlot->SetSize(FVector2D(0.f, 0.f));
	BackdropSlot->SetAutoSize(false);

	// Centre board panel.
	UBorder* Board = MakePanel(WidgetTree, PanelBackground(), 18.f, 12.f);
	UCanvasPanelSlot* BoardSlot = Root->AddChildToCanvas(Board);
	BoardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	BoardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BoardSlot->SetPosition(FVector2D(0.f, 0.f));
	BoardSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Board->AddChild(VBox);

	UTextBlock* Title = MakeText(WidgetTree, TEXT("RAMP DEPARTURE BOARD"), 22, AmberLED(), true);
	VBox->AddChild(Title);

	// Column headers.
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VBox->AddChild(HeaderRow);

	auto AddColumnHeader = [&](const FString& Label, float Width)
	{
		USizeBox* Col = MakeFixed(WidgetTree, Width, 18.f, nullptr);
		UTextBlock* Text = MakeText(WidgetTree, Label, 11, TextDim(), true);
		Col->AddChild(Text);
		UHorizontalBoxSlot* Slot = HeaderRow->AddChildToHorizontalBox(Col);
		Slot->SetVerticalAlignment(VAlign_Center);
	};
	AddColumnHeader(TEXT("FLIGHT"), 120.f);
	AddColumnHeader(TEXT("TYPE"), 90.f);
	AddColumnHeader(TEXT("STATUS"), 170.f);
	AddColumnHeader(TEXT("HOLDOVER"), 110.f);
	AddColumnHeader(TEXT("CLEAN"), 80.f);
	AddColumnHeader(TEXT("PROT"), 80.f);

	// Separator (height override: contentless border would have no desired size).
	UBorder* Sep = MakeSquare(WidgetTree, PanelOutline());
	USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SepBox->SetHeightOverride(1.f);
	SepBox->AddChild(Sep);
	UVerticalBoxSlot* SepSlot = VBox->AddChildToVerticalBox(SepBox);
	SepSlot->SetPadding(FMargin(0.f, 4.f));

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	VBox->AddChild(RowsBox);

	UTextBlock* Footer = MakeText(WidgetTree, TEXT("[TAB] CLOSE"), 11, TextDim());
	UVerticalBoxSlot* FooterSlot = VBox->AddChildToVerticalBox(Footer);
	FooterSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	WidgetTree->RootWidget = Root;
}

FString UHoldoverDepartureBoardWidget::GetAircraftSignature() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	FString Sig;
	for (TActorIterator<AHoldoverAircraft> It(World); It; ++It)
	{
		Sig += It->GetName();
		Sig += TEXT("|");
	}
	return Sig;
}

void UHoldoverDepartureBoardWidget::RebuildRows()
{
	if (!RowsBox || !WidgetTree)
	{
		return;
	}

	RowsBox->ClearChildren();
	Rows.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	using namespace HoldoverUI;

	for (TActorIterator<AHoldoverAircraft> It(World); It; ++It)
	{
		AHoldoverAircraft* Aircraft = *It;

		FDepartureRow Row;
		Row.Aircraft = Aircraft;

		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		auto AddCell = [&](UTextBlock* Cell, float Width, bool bFixed)
		{
			UWidget* Content = Cell;
			if (bFixed)
			{
				Content = MakeFixed(WidgetTree, Width, 20.f, Cell);
			}
			UHorizontalBoxSlot* Slot = RowBox->AddChildToHorizontalBox(Content);
			Slot->SetVerticalAlignment(VAlign_Center);
			return Slot;
		};

		Row.Flight = MakeText(WidgetTree, FlightLabel(Aircraft), 14, TextPrimary(), true);
		AddCell(Row.Flight, 120.f, true);

		// Aircraft type placeholder — greybox aircraft are generic; show wing config later.
		UTextBlock* Type = MakeText(WidgetTree, TEXT("JET"), 12, TextDim());
		AddCell(Type, 90.f, true);

		Row.Status = MakeText(WidgetTree, TEXT("-"), 13, TextPrimary(), true);
		AddCell(Row.Status, 170.f, true);

		Row.Holdover = MakeText(WidgetTree, TEXT("-"), 14, AmberLED(), true);
		AddCell(Row.Holdover, 110.f, true);

		Row.Clean = MakeText(WidgetTree, TEXT("0%"), 13, TextPrimary());
		AddCell(Row.Clean, 80.f, true);

		Row.Prot = MakeText(WidgetTree, TEXT("0%"), 13, TextPrimary());
		AddCell(Row.Prot, 80.f, true);

		RowsBox->AddChild(RowBox);
		Rows.Add(Row);
	}
}

void UHoldoverDepartureBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	using namespace HoldoverUI;

	// Rebuild only when the aircraft set changes — throttled, because the
	// 2026-10-29 review flagged per-tick TActorIterator scans as a smell.
	RefreshTimer -= InDeltaTime;
	if (RefreshTimer <= 0.f)
	{
		RefreshTimer = 0.5f;
		const FString Signature = GetAircraftSignature();
		if (Signature != LastSignature)
		{
			LastSignature = Signature;
			RebuildRows();
		}
	}

	for (FDepartureRow& Row : Rows)
	{
		const AHoldoverAircraft* Aircraft = Row.Aircraft.Get();
		if (!Aircraft)
		{
			continue;
		}

		// Status + colour (Vestergaard traffic-light semantics).
		FString StatusText;
		FLinearColor StatusColor = StateGray();
		switch (Aircraft->HoldoverState)
		{
		case EHoldoverState::Departed:
			StatusText = TEXT("DEPARTED");
			StatusColor = TextDim();
			break;
		case EHoldoverState::HoldoverExpired:
			StatusText = TEXT("EXPIRED");
			StatusColor = StateRed();
			break;
		case EHoldoverState::HoldoverActive:
			if (Aircraft->AreAllSectionsCleanAndProtected())
			{
				StatusText = TEXT("CLEARED");
				StatusColor = StateGreen();
			}
			else
			{
				StatusText = TEXT("HOLDOVER RUN");
				StatusColor = Aircraft->HoldoverRemaining < 30.f ? StateYellow() : StateGreen();
			}
			break;
		default:
			StatusText = TEXT("TREATING");
			StatusColor = StateGray();
			break;
		}
		Row.Status->SetText(FText::FromString(StatusText));
		Row.Status->SetColorAndOpacity(FSlateColor(StatusColor));

		// Holdover clock.
		if (Aircraft->HoldoverState == EHoldoverState::HoldoverActive ||
			Aircraft->HoldoverState == EHoldoverState::HoldoverExpired)
		{
			Row.Holdover->SetText(FText::FromString(HoldoverUI::FormatHoldover(Aircraft->HoldoverRemaining)));
			Row.Holdover->SetColorAndOpacity(FSlateColor(
				Aircraft->HoldoverState == EHoldoverState::HoldoverExpired ? StateRed() : AmberLED()));
		}
		else
		{
			Row.Holdover->SetText(FText::FromString(TEXT("--:--")));
			Row.Holdover->SetColorAndOpacity(FSlateColor(TextDim()));
		}

		// Clean / protection with green-at-threshold colouring.
		const float Clean = Aircraft->GetOverallCleanliness();
		const float Prot = Aircraft->GetOverallProtection();
		Row.Clean->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Clean)));
		Row.Clean->SetColorAndOpacity(FSlateColor(
			Clean >= 95.f ? StateGreen() : (Clean > 1.f ? StateYellow() : TextDim())));
		Row.Prot->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Prot)));
		Row.Prot->SetColorAndOpacity(FSlateColor(
			Prot >= 95.f ? StateBlue() : (Prot > 1.f ? StateYellow() : TextDim())));
	}
}

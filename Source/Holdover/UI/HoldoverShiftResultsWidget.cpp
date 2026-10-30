// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HoldoverShiftResultsWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "HoldoverGameState.h"
#include "HoldoverUIStyle.h"

void UHoldoverShiftResultsWidget::NativeOnInitialized()
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

void UHoldoverShiftResultsWidget::BuildTree()
{
	using namespace HoldoverUI;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());

	UBorder* Backdrop = MakeSquare(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.65f));
	UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackdropSlot->SetAlignment(FVector2D(0.f, 0.f));
	BackdropSlot->SetPosition(FVector2D(0.f, 0.f));
	BackdropSlot->SetAutoSize(false);

	UBorder* Report = MakePanel(WidgetTree, PanelBackground(), 24.f, 16.f);
	UCanvasPanelSlot* ReportSlot = Root->AddChildToCanvas(Report);
	ReportSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ReportSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ReportSlot->SetPosition(FVector2D(0.f, 0.f));
	ReportSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Report->AddChild(VBox);

	// Hi-vis hazard header bar (height override: contentless borders have no desired size).
	UBorder* HazardBar = MakeSquare(WidgetTree, StateYellow());
	USizeBox* HazardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HazardBox->SetHeightOverride(6.f);
	HazardBox->AddChild(HazardBar);
	UVerticalBoxSlot* HazardSlot = VBox->AddChildToVerticalBox(HazardBox);
	HazardSlot->SetPadding(FMargin(-24.f, -16.f, -24.f, 10.f));

	UTextBlock* Title = MakeText(WidgetTree, TEXT("RAMP OPS - SHIFT REPORT"), 22, AmberLED(), true);
	VBox->AddChild(Title);

	// KPI rows (DTS-dashboard language): label left, value right.
	auto AddRow = [&](const FString& Label, const FLinearColor& ValueColor, TObjectPtr<UTextBlock>& OutValue)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 4.f));

		UTextBlock* Key = MakeText(WidgetTree, Label, 13, TextDim());
		UHorizontalBoxSlot* KeySlot = Row->AddChildToHorizontalBox(Key);
		KeySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		KeySlot->SetVerticalAlignment(VAlign_Center);

		OutValue = MakeText(WidgetTree, TEXT("-"), 14, ValueColor, true);
		OutValue->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(OutValue);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	};

	AddRow(TEXT("ON-TIME DEPARTURES"), StateGreen(), OnTimeText);
	AddRow(TEXT("LATE DEPARTURES"), StateRed(), LateText);
	AddRow(TEXT("GROSS PAY"), TextPrimary(), GrossText);
	AddRow(TEXT("DEDUCTIONS"), StateRed(), DeductionsText);
	AddRow(TEXT("NET PAY"), StateGreen(), NetText);

	// Stamp.
	StampText = MakeText(WidgetTree, TEXT("PASSED"), 44, StateGreen(), true);
	StampText->SetRenderTransformAngle(-10.f);
	UVerticalBoxSlot* StampSlot = VBox->AddChildToVerticalBox(StampText);
	StampSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
	StampSlot->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* Footer = MakeText(WidgetTree, TEXT("PRESS [E] ON THE START CUBE FOR ANOTHER SHIFT"), 10, TextDim());
	UVerticalBoxSlot* FooterSlot = VBox->AddChildToVerticalBox(Footer);
	FooterSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	FooterSlot->SetHorizontalAlignment(HAlign_Center);

	WidgetTree->RootWidget = Root;

	// Hidden until the shift actually ends.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHoldoverShiftResultsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Content refresh only — visibility is parent-driven (P7-1: a Collapsed
	// UserWidget stops ticking, so this widget must never hide itself).
	RefreshReport();
}

void UHoldoverShiftResultsWidget::RefreshReport()
{
	using namespace HoldoverUI;

	// Tree guard (Initialize normally runs via the parent; never deref before it).
	if (!OnTimeText)
	{
		return;
	}

	AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (!GS || GS->ShiftState != EShiftState::ShiftEnded)
	{
		return;
	}

	const int32 Net = GS->Money - GS->Deductions;

	OnTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), GS->OnTimeDepartures, GS->Quota)));
	LateText->SetText(FText::FromString(FString::Printf(TEXT("%d"), GS->LateDepartures)));
	GrossText->SetText(FText::FromString(FString::Printf(TEXT("$%d"), GS->Money)));
	DeductionsText->SetText(FText::FromString(FString::Printf(TEXT("-$%d"), GS->Deductions)));
	NetText->SetText(FText::FromString(FString::Printf(TEXT("$%d"), Net)));
	NetText->SetColorAndOpacity(FSlateColor(Net >= 0 ? StateGreen() : StateRed()));

	const bool bPassed = GS->bShiftPassed;
	StampText->SetText(FText::FromString(bPassed ? TEXT("PASSED") : TEXT("RESTRUCTURED")));
	StampText->SetColorAndOpacity(FSlateColor(bPassed ? StateGreen() : StateRed()));
}

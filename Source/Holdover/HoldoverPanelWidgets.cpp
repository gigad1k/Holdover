// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverPanelWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverGameState.h"
#include "HoldoverSurfaceSection.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeHoldoverText(UObject* Outer, const TCHAR* Name, int32 Size, FLinearColor Color)
	{
		UTextBlock* Text = NewObject<UTextBlock>(Outer, Name, RF_Transient);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UBorder* MakeHoldoverPanel(UObject* Outer, const TCHAR* Name)
	{
		UBorder* Border = NewObject<UBorder>(Outer, Name, RF_Transient);
		Border->SetBrushColor(FLinearColor(0.f, 0.05f, 0.12f, 0.55f));
		Border->SetPadding(FMargin(12.f, 8.f));
		return Border;
	}

	const TCHAR* BoardStateName(EHoldoverState State)
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

// --- Departure board ---

TSharedRef<SWidget> UHoldoverDepartureBoardWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UBorder* Panel = MakeHoldoverPanel(this, TEXT("BoardPanel"));
		UVerticalBox* Box = NewObject<UVerticalBox>(this, TEXT("BoardBox"), RF_Transient);
		Panel->SetContent(Box);

		StatusLine = MakeHoldoverText(this, TEXT("BoardStatus"), 16, FLinearColor::White);
		TreatmentLine = MakeHoldoverText(this, TEXT("BoardTreatment"), 13, FLinearColor(0.8f, 0.9f, 1.f));
		SectionLine = MakeHoldoverText(this, TEXT("BoardSections"), 13, FLinearColor(0.8f, 0.9f, 1.f));
		Box->AddChildToVerticalBox(StatusLine);
		Box->AddChildToVerticalBox(TreatmentLine);
		Box->AddChildToVerticalBox(SectionLine);

		WidgetTree->RootWidget = Panel;
	}
	return Super::RebuildWidget();
}

void UHoldoverDepartureBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHoldoverDepartureBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FString Status = TEXT("Aircraft: none");
	FString Treatment;
	FString Sections;

	AHoldoverAircraft* Aircraft = CachedAircraft.Get();
	if (!Aircraft)
	{
		for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
		{
			Aircraft = *It;
			break;
		}
		CachedAircraft = Aircraft;
	}

	if (Aircraft)
	{
		Status = FString::Printf(TEXT("AIRCRAFT: %s   holdover %.0fs"), BoardStateName(Aircraft->HoldoverState), Aircraft->HoldoverRemaining);
		Treatment = FString::Printf(TEXT("clean %.0f%%   protection %.0f%%"),
			Aircraft->GetOverallCleanliness(), Aircraft->GetOverallProtection());

		for (const AHoldoverSurfaceSection* Section : Aircraft->GetSections())
		{
			if (!Section)
			{
				continue;
			}
			if (!Sections.IsEmpty())
			{
				Sections += TEXT("  ");
			}
			Sections += FString::Printf(TEXT("%s %.0f/%.0f"),
				*Section->SectionName.ToString(), Section->CleanlinessPercent, Section->ProtectionPercent);
		}
	}
	if (StatusLine)
	{
		StatusLine->SetText(FText::FromString(Status));
		StatusLine->SetColorAndOpacity(FSlateColor(HoldoverStateColor(
			Aircraft ? Aircraft->HoldoverState : EHoldoverState::Incomplete,
			Aircraft ? Aircraft->HoldoverRemaining : 0.f)));
	}
	if (TreatmentLine) { TreatmentLine->SetText(FText::FromString(Treatment)); }
	if (SectionLine) { SectionLine->SetText(FText::FromString(Sections)); }
}

FLinearColor UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState State, float RemainingSeconds)
{
	switch (State)
	{
	case EHoldoverState::HoldoverActive:
		return RemainingSeconds < 30.f ? FLinearColor::Yellow : FLinearColor::Green;
	case EHoldoverState::HoldoverExpired:
		return FLinearColor::Red;
	default:
		return FLinearColor::White;
	}
}

// --- Dispatcher feed ---

TSharedRef<SWidget> UHoldoverDispatcherFeedWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UBorder* Panel = MakeHoldoverPanel(this, TEXT("FeedPanel"));
		LineBox = NewObject<UVerticalBox>(this, TEXT("FeedBox"), RF_Transient);
		Panel->SetContent(LineBox);

		for (int32 i = 0; i < MaxLines; ++i)
		{
			UTextBlock* Line = MakeHoldoverText(this, *FString::Printf(TEXT("FeedLine%d"), i), 11, FLinearColor(0.7f, 0.9f, 1.f));
			Lines.Add(Line);
			LineBox->AddChildToVerticalBox(Line);
		}

		WidgetTree->RootWidget = Panel;
	}
	return Super::RebuildWidget();
}

void UHoldoverDispatcherFeedWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHoldoverDispatcherFeedWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	static const TArray<FText> EmptyFeed;
	const TArray<FText>& Feed = GS ? GS->DispatchFeed : EmptyFeed;

	// Bottom-aligned: the newest line sits at the bottom slot.
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const int32 SourceIndex = Feed.Num() - (Lines.Num() - i);
		if (Lines[i])
		{
			Lines[i]->SetText(SourceIndex >= 0 ? Feed[SourceIndex] : FText::GetEmpty());
		}
	}
}

// --- Shift results ---

TSharedRef<SWidget> UHoldoverShiftResultsWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		Panel = MakeHoldoverPanel(this, TEXT("ResultsPanel"));
		UVerticalBox* Box = NewObject<UVerticalBox>(this, TEXT("ResultsBox"), RF_Transient);
		Panel->SetContent(Box);

		VerdictLine = MakeHoldoverText(this, TEXT("ResultsVerdict"), 28, FLinearColor::White);
		BreakdownLine = MakeHoldoverText(this, TEXT("ResultsBreakdown"), 14, FLinearColor(0.85f, 0.9f, 1.f));
		Box->AddChildToVerticalBox(VerdictLine);
		Box->AddChildToVerticalBox(BreakdownLine);

		WidgetTree->RootWidget = Panel;
	}
	return Super::RebuildWidget();
}

void UHoldoverShiftResultsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Start hidden; UHoldoverHUDWidget::NativeTick re-shows this at shift end.
	// (Never self-Collapse here — see P7-1.)
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHoldoverShiftResultsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (!GS || GS->ShiftState != EShiftState::ShiftEnded)
	{
		return;
	}

	if (VerdictLine)
	{
		VerdictLine->SetText(FText::FromString(GS->bShiftPassed ? TEXT("SHIFT PASSED!") : TEXT("RESTRUCTURED")));
		VerdictLine->SetColorAndOpacity(FSlateColor(GS->bShiftPassed ? FLinearColor::Green : FLinearColor::Red));
	}
	if (BreakdownLine)
	{
		BreakdownLine->SetText(FText::FromString(FString::Printf(TEXT("On-time %d/%d   pay $%d   deductions $%d   press E on the cube for a new shift"),
			GS->OnTimeDepartures, GS->Quota, GS->Money, GS->Deductions)));
	}
}

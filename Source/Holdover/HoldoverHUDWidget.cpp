// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "HoldoverGameState.h"
#include "HoldoverPanelWidgets.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UHoldoverHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		Root = NewObject<UCanvasPanel>(this, TEXT("Root"), RF_Transient);

		auto Place = [this](UWidget* Widget, float X, float Y, bool bTopRight = false, bool bBottomLeft = false, bool bCenter = false)
		{
			UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
			Slot->SetAutoSize(true);
			if (bTopRight)
			{
				Slot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
				Slot->SetAlignment(FVector2D(1.f, 0.f));
			}
			else if (bBottomLeft)
			{
				Slot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
				Slot->SetAlignment(FVector2D(0.f, 1.f));
			}
			else if (bCenter)
			{
				Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			}
			Slot->SetOffsets(FMargin(X, Y, 0.f, 0.f));
			return Slot;
		};

		auto MakeText = [this](const TCHAR* Name, int32 Size, FLinearColor Color)
		{
			UTextBlock* Text = NewObject<UTextBlock>(this, Name, RF_Transient);
			Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
			Text->SetColorAndOpacity(FSlateColor(Color));
			return Text;
		};

		ShiftLine = MakeText(TEXT("ShiftLine"), 15, FLinearColor::White);
		Place(ShiftLine, 24.f, 16.f);

		Feed = CreateWidget<UHoldoverDispatcherFeedWidget>(GetOwningPlayer());
		Place(Feed, 24.f, 48.f);

		Board = CreateWidget<UHoldoverDepartureBoardWidget>(GetOwningPlayer());
		Place(Board, -24.f, 16.f, true);

		TreatmentLine = MakeText(TEXT("TreatmentLine"), 14, FLinearColor(0.85f, 0.95f, 1.f));
		Place(TreatmentLine, 24.f, -64.f, false, true);

		Results = CreateWidget<UHoldoverShiftResultsWidget>(GetOwningPlayer());
		Place(Results, 0.f, -80.f, false, false, true);

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UHoldoverHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (GS && ShiftLine)
	{
		ShiftLine->SetText(FText::FromString(FString::Printf(TEXT("SHIFT %d/%d   time %.0fs   pay $%d   deductions $%d"),
			GS->OnTimeDepartures, GS->Quota, GS->ShiftTimeRemaining, GS->Money, GS->Deductions)));
	}

	// The results banner must never hide itself: a Collapsed UserWidget stops
	// receiving NativeTick, so the parent drives its visibility (P7-1).
	if (Results)
	{
		const bool bEnded = GS && GS->ShiftState == EShiftState::ShiftEnded;
		Results->SetVisibility(bEnded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Treatment panel: fluid, tanks, weather, current target (from the possessed pawn).
	if (TreatmentLine)
	{
		FString Line = TEXT("");
		const APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
		if (const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr)
		{
			const bool bTypeI = Spray->ActiveFluidType == EHoldoverFluidType::TypeI;
			Line = FString::Printf(TEXT("%s [Q]   Type I %.0f%%%s   Type IV %.0f%%%s   weather %.0f%%"),
				bTypeI ? TEXT("TYPE I (clean)") : TEXT("TYPE IV (protect)"),
				Spray->TypeIFluid, Spray->TypeIFluid <= 0.f ? TEXT(" [EMPTY]") : TEXT(""),
				Spray->TypeIVFluid, Spray->TypeIVFluid <= 0.f ? TEXT(" [EMPTY]") : TEXT(""),
				GS ? GS->WeatherSeverity * 100.f : 0.f);
			if (AHoldoverSurfaceSection* Section = Spray->CurrentTargetSection.Get())
			{
				Line += FString::Printf(TEXT("   target %s clean %.0f%% prot %.0f%%"),
					*Section->SectionName.ToString(), Section->CleanlinessPercent, Section->ProtectionPercent);
			}
			if (!Spray->TargetHint.IsEmpty())
			{
				Line += FString::Printf(TEXT("   [%s]"), *Spray->TargetHint.ToString());
			}
		}
		TreatmentLine->SetText(FText::FromString(Line));
	}
}

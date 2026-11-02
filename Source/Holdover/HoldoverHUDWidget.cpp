// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "HoldoverGameState.h"
#include "HoldoverInfoWidget.h"
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

		// D1-D3 field manual: fixed-size centred overlay so its ScrollBox scrolls
		// instead of growing off screen. Starts collapsed; F1 (or first shift)
		// opens it.
		Info = CreateWidget<UHoldoverInfoWidget>(GetOwningPlayer());
		if (Info)
		{
			if (UCanvasPanelSlot* InfoSlot = Root->AddChildToCanvas(Info))
			{
				InfoSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				InfoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				InfoSlot->SetSize(FVector2D(720.f, 520.f));
			}
			Info->SetVisibility(ESlateVisibility::Collapsed);
		}

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UHoldoverHUDWidget::ToggleInfoPanel()
{
	if (!Info)
	{
		return;
	}
	const bool bOpen = Info->GetVisibility() == ESlateVisibility::Visible;
	Info->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (!bOpen)
	{
		Info->RefreshContent();
	}
}

void UHoldoverHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (GS && ShiftLine)
	{
		ShiftLine->SetText(FText::FromString(FString::Printf(TEXT("SHIFT %d (%s)  %d/%d   time %.0fs   pay $%d   ded $%d   net $%d   ledger $%d"),
			GS->ShiftNumber, *GS->GetProfileName(),
			GS->OnTimeDepartures, GS->Quota, GS->ShiftTimeRemaining,
			GS->Money, GS->Deductions, GS->GetNetPay(), GS->CompanyLedger)));
	}

	// The results banner must never hide itself: a Collapsed UserWidget stops
	// receiving NativeTick, so the parent drives its visibility (P7-1).
	if (Results)
	{
		const bool bEnded = GS && GS->ShiftState == EShiftState::ShiftEnded;
		Results->SetVisibility(bEnded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// D1: open the field manual once when the first shift begins, so a player
	// with no manual sees the tutorial without hunting for a key.
	if (!bAutoShowedInfo && Info && GS &&
		GS->ShiftNumber <= 1 && GS->ShiftState == EShiftState::InProgress)
	{
		bAutoShowedInfo = true;
		Info->SetVisibility(ESlateVisibility::Visible);
		Info->RefreshContent();
	}

	// Treatment panel: fluid, tank level, weather, current target (from the possessed pawn).
	if (TreatmentLine)
	{
		FString Line = TEXT("");
		const APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
		if (const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr)
		{
			const bool bTypeI = Spray->CarriedFluidType == EHoldoverFluidType::TypeI;
			Line = FString::Printf(TEXT("%s   tank %.0f%%%s   weather %.0f%%"),
				bTypeI ? TEXT("TYPE I (clean)") : TEXT("TYPE IV (protect)"),
				Spray->FluidLevel, Spray->FluidLevel <= 0.f ? TEXT(" [EMPTY]") : TEXT(""),
				GS ? GS->WeatherSeverity * 100.f : 0.f);
			if (Spray->IsRefilling())
			{
				Line += FString::Printf(TEXT("   refilling %.0f%%"), Spray->GetRefillProgress() * 100.f);
			}
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

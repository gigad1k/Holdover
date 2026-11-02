// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverInfoWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HoldoverInfoContent.h"
#include "Holdover.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeInfoText(UObject* Outer, const TCHAR* Name, int32 Size, FLinearColor Color)
	{
		UTextBlock* Text = NewObject<UTextBlock>(Outer, Name, RF_Transient);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	}

	UTextBlock* AddSection(UObject* Outer, UVerticalBox* Box, const TCHAR* Name,
		const FText& Heading, const FText& Body)
	{
		UTextBlock* Head = MakeInfoText(Outer, Name, 15, FLinearColor(1.f, 0.78f, 0.35f));
		Head->SetText(Heading);
		UVerticalBoxSlot* HeadSlot = Box->AddChildToVerticalBox(Head);
		if (HeadSlot)
		{
			HeadSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
		}

		UTextBlock* BodyText = MakeInfoText(Outer, (FString(Name) + TEXT("Body")).GetCharArray().GetData(), 11,
			FLinearColor(0.86f, 0.9f, 0.95f));
		BodyText->SetText(Body);
		UVerticalBoxSlot* BodySlot = Box->AddChildToVerticalBox(BodyText);
		if (BodySlot)
		{
			BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		return BodyText;
	}
}

TSharedRef<SWidget> UHoldoverInfoWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UBorder* Panel = NewObject<UBorder>(this, TEXT("InfoPanel"), RF_Transient);
		Panel->SetBrushColor(FLinearColor(0.02f, 0.05f, 0.12f, 0.92f));
		Panel->SetPadding(FMargin(18.f, 14.f));

		UScrollBox* Scroll = NewObject<UScrollBox>(this, TEXT("InfoScroll"), RF_Transient);
		Panel->SetContent(Scroll);
		Box = NewObject<UVerticalBox>(this, TEXT("InfoBox"), RF_Transient);
		Scroll->AddChild(Box);

		Title = MakeInfoText(this, TEXT("InfoTitle"), 19, FLinearColor::White);
		Title->SetText(NSLOCTEXT("HoldoverInfo", "InfoTitleText", "HOLDOVER - FIELD MANUAL"));
		if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		WidgetTree->RootWidget = Panel;
		RefreshContent();
		// Evidence line for the packaged smoke test: proves the D1-D3 manual was
		// actually constructed in a cooked build, not just compiled in.
		UE_LOG(LogHoldover, Display, TEXT("Field manual built: %d tips + credits/legal/feedback"),
			UHoldoverInfoContent::FirstShiftTips().Num());
	}
	return Super::RebuildWidget();
}

void UHoldoverInfoWidget::RefreshContent()
{
	if (!Box)
	{
		return;
	}
	// Rebuilding from scratch keeps this idempotent and avoids diffing text.
	// Sonnet M11: headings were not tracked in BodyLines, so every refresh
	// appended them again - clear ALL children except the title, and garbage the
	// old widgets so their object names are free for the rebuild.
	TArray<UWidget*> OldChildren = Box->GetAllChildren();
	for (UWidget* Child : OldChildren)
	{
		if (Child && Child != Title)
		{
			Child->RemoveFromParent();
			Child->MarkAsGarbage();
		}
	}
	BodyLines.Reset();

	// D1 - tutorial, one block per tip.
	const TArray<UHoldoverInfoContent::FTip> Tips = UHoldoverInfoContent::FirstShiftTips();
	for (int32 i = 0; i < Tips.Num(); ++i)
	{
		UTextBlock* Line = AddSection(this, Box,
			*(FString::Printf(TEXT("InfoTip%d"), i)), Tips[i].Title, Tips[i].Body);
		BodyLines.Add(Line);
	}

	// D2/D3 - credits, legal, feedback.
	BodyLines.Add(AddSection(this, Box, TEXT("InfoCredits"),
		NSLOCTEXT("HoldoverInfo", "CreditsHead", "CREDITS"),
		UHoldoverInfoContent::CreditsText()));
	BodyLines.Add(AddSection(this, Box, TEXT("InfoLegal"),
		NSLOCTEXT("HoldoverInfo", "LegalHead", "LEGAL"),
		UHoldoverInfoContent::LegalText()));
	BodyLines.Add(AddSection(this, Box, TEXT("InfoFeedback"),
		NSLOCTEXT("HoldoverInfo", "FeedbackHead", "FEEDBACK"),
		UHoldoverInfoContent::FeedbackText()));
}

void UHoldoverInfoWidget::SetTipsVisible(bool bVisible)
{
	// Tips are the first FirstShiftTips().Num() body lines; hide just those.
	const int32 TipCount = UHoldoverInfoContent::FirstShiftTips().Num();
	for (int32 i = 0; i < BodyLines.Num() && i < TipCount; ++i)
	{
		if (BodyLines[i])
		{
			BodyLines[i]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

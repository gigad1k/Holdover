// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HoldoverDispatcherFeedWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HoldoverGameState.h"
#include "UI/HoldoverUIStyle.h"
#include "Blueprint/WidgetTree.h"

void UHoldoverDispatcherFeedWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureTreeBuilt();
}

void UHoldoverDispatcherFeedWidget::EnsureTreeBuilt()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
}

void UHoldoverDispatcherFeedWidget::BuildTree()
{
	using namespace HoldoverUI;

	UBorder* Panel = MakePanel(WidgetTree, PanelBackground(), 8.f, 5.f);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChild(VBox);

	HeaderText = MakeText(WidgetTree, TEXT("GND DISPATCH 121.9"), 11, StateGreen(), true);
	VBox->AddChild(HeaderText);

	LinesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	VBox->AddChild(LinesBox);

	WidgetTree->RootWidget = Panel;
}

void UHoldoverDispatcherFeedWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	using namespace HoldoverUI;

	if (const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr)
	{
		if (GS->DispatchFeed.Num() != LastFeedCount)
		{
			LastFeedCount = GS->DispatchFeed.Num();
			RefreshLines();
		}

		// Newest line highlighted (white on chip), older lines dimmed.
		for (int32 i = 0; i < LineBlocks.Num(); ++i)
		{
			const bool bNewest = (i == LineBlocks.Num() - 1);
			LineBlocks[i]->SetColorAndOpacity(FSlateColor(bNewest ? TextPrimary() : TextDim()));
			if (i < LineChips.Num())
			{
				const FLinearColor ChipBg = bNewest
					? FLinearColor(StateGreen().R, StateGreen().G, StateGreen().B, 0.10f)
					: FLinearColor(0.f, 0.f, 0.f, 0.f);
				LineChips[i]->SetBrush(MakeBoxBrush(ChipBg));
			}
		}
	}
}

void UHoldoverDispatcherFeedWidget::RefreshLines()
{
	if (!LinesBox)
	{
		return;
	}

	LinesBox->ClearChildren();
	LineBlocks.Reset();
	LineChips.Reset();

	const AHoldoverGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHoldoverGameState>() : nullptr;
	if (!GS || GS->DispatchFeed.Num() == 0)
	{
		// Keep the panel alive with an idle hint when the radio is quiet.
		UTextBlock* Idle = HoldoverUI::MakeText(WidgetTree, TEXT("... static ..."), 11, HoldoverUI::TextDim());
		LinesBox->AddChild(Idle);
		return;
	}

	const int32 First = FMath::Max(0, GS->DispatchFeed.Num() - 4);
	for (int32 i = First; i < GS->DispatchFeed.Num(); ++i)
	{
		UTextBlock* Line = HoldoverUI::MakeText(WidgetTree, GS->DispatchFeed[i].ToString(), 11, HoldoverUI::TextPrimary());
		UBorder* Chip = HoldoverUI::MakeChip(WidgetTree, Line, FLinearColor(0.f, 0.f, 0.f, 0.f), 4.f, 1.f);
		LinesBox->AddChild(Chip);
		LineBlocks.Add(Line);
		LineChips.Add(Chip);
	}
}

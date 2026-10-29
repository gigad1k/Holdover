// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * Shared palette + tiny factory helpers for the Vestergaard-style ramp-ops UI.
 * State colours follow the real vehicle HMI semantics:
 * gray = inactive/untreated, green = enabled/OK, red = error, yellow = warning.
 */
namespace HoldoverUI
{
	// --- panel chrome ---
	inline FLinearColor PanelBackground() { return FLinearColor(0.055f, 0.070f, 0.086f, 0.92f); }
	inline FLinearColor PanelOutline() { return FLinearColor(0.18f, 0.21f, 0.24f, 1.f); }
	inline FLinearColor TextPrimary() { return FLinearColor(0.88f, 0.91f, 0.94f, 1.f); }
	inline FLinearColor TextDim() { return FLinearColor(0.52f, 0.57f, 0.62f, 1.f); }

	// --- Vestergaard HMI state colours ---
	inline FLinearColor StateGray() { return FLinearColor(0.30f, 0.33f, 0.36f, 1.f); }
	inline FLinearColor StateGreen() { return FLinearColor(0.25f, 0.75f, 0.42f, 1.f); }
	inline FLinearColor StateRed() { return FLinearColor(0.89f, 0.23f, 0.20f, 1.f); }
	inline FLinearColor StateYellow() { return FLinearColor(1.00f, 0.83f, 0.25f, 1.f); }
	inline FLinearColor StateBlue() { return FLinearColor(0.25f, 0.66f, 0.96f, 1.f); }

	// --- FIDS amber: countdown digits only ---
	inline FLinearColor AmberLED() { return FLinearColor(1.00f, 0.69f, 0.00f, 1.f); }

	inline FSlateBrush MakeBoxBrush(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.Color = Outline;
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	/** Solid, sharp-cornered panel (equipment-HMI look). */
	inline UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Fill = PanelBackground(), float PadX = 10.f, float PadY = 6.f)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(MakeBoxBrush(Fill, PanelOutline(), 1.f));
		Border->SetPadding(FMargin(PadX, PadY));
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	}

	/** Plain solid square (no outline) — for bars, fills and chips. */
	inline UBorder* MakeSquare(UWidgetTree* Tree, const FLinearColor& Fill)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(MakeBoxBrush(Fill));
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	}

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		if (bBold)
		{
			Font.TypefaceFontName = TEXT("Bold");
		}
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Block;
	}

	inline USizeBox* MakeFixed(UWidgetTree* Tree, float Width, float Height, UWidget* Content)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);
		if (Content)
		{
			Box->AddChild(Content);
		}
		return Box;
	}

	/** Coloured chip (small labelled plate) used for fluid selectors and scan cells. */
	inline UBorder* MakeChip(UWidgetTree* Tree, UTextBlock* Label, const FLinearColor& Fill, float PadX = 6.f, float PadY = 2.f)
	{
		UBorder* Chip = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Chip->SetBrush(MakeBoxBrush(Fill, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f));
		Chip->SetPadding(FMargin(PadX, PadY));
		Chip->SetHorizontalAlignment(HAlign_Center);
		Chip->SetVerticalAlignment(VAlign_Center);
		if (Label)
		{
			Chip->AddChild(Label);
		}
		Chip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Chip;
	}

	/** M:SS clock formatting shared by every widget. */
	inline FString FormatClock(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	/** MM:SS for the holdover board (always two-digit minutes). */
	inline FString FormatHoldover(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"

void AHoldoverHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// Simple cross reticle
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float T = 2.f;      // thickness
	const float L = 8.f;      // arm length
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.85f), CX - L, CY - T * 0.5f, L * 2.f, T);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.85f), CX - T * 0.5f, CY - L, T, L * 2.f);

	// Interaction prompt, centred just below the reticle
	if (!InteractionPrompt.IsEmpty())
	{
		float TextW = 0.f;
		float TextH = 0.f;
		const FString Prompt = InteractionPrompt.ToString();
		GetTextSize(Prompt, TextW, TextH, UEngine::GetLargeFont(), 1.f);
		DrawText(Prompt, FLinearColor::Yellow, CX - TextW * 0.5f, CY + 24.f, UEngine::GetLargeFont(), 1.f);
	}
}

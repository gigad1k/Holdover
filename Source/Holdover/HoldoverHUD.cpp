// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HoldoverCharacter.h"
#include "HoldoverHUDWidget.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"

void AHoldoverHUD::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		HUDWidget = CreateWidget<UHoldoverHUDWidget>(PC);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
}

void AHoldoverHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// Centre reticle
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float T = 2.f;
	const float L = 8.f;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.85f), CX - L, CY - T * 0.5f, L * 2.f, T);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.85f), CX - T * 0.5f, CY - L, T, L * 2.f);

	// Interaction prompt just below the reticle
	if (!InteractionPrompt.IsEmpty())
	{
		float TextW = 0.f;
		float TextH = 0.f;
		const FString Prompt = InteractionPrompt.ToString();
		GetTextSize(Prompt, TextW, TextH, UEngine::GetLargeFont(), 1.f);
		DrawText(Prompt, FLinearColor::Yellow, CX - TextW * 0.5f, CY + 24.f, UEngine::GetLargeFont(), 1.f);
	}

	// Spray placeholder line from the reticle to the current target section
	const APawn* Pawn = GetOwningPawn();
	if (const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr)
	{
		if (Spray->IsSpraying())
		{
			if (AHoldoverSurfaceSection* Section = Spray->CurrentTargetSection.Get())
			{
				const FVector ScreenPos = Canvas->Project(Section->GetActorLocation());
				if (ScreenPos.Z > 0.f)
				{
					DrawLine(CX, CY, ScreenPos.X, ScreenPos.Y, FLinearColor(0.4f, 0.8f, 1.f, 0.6f), 2.f);
				}
			}
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverCharacter.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"

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

	// Treatment status (from the locally controlled character)
	const APawn* Pawn = GetOwningPawn();
	const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(Pawn);
	if (Character && Character->GetSprayComponent())
	{
		const UHoldoverSprayComponent* Spray = Character->GetSprayComponent();

		// Aircraft-wide total (single test aircraft expected in greybox)
		float Total = -1.f;
		for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
		{
			Total = It->GetOverallCleanliness();
			break;
		}

		FString Status = FString::Printf(TEXT("Type I: %.0f%%"), Spray->TypeIFluid);
		if (AHoldoverSurfaceSection* Section = Spray->CurrentTargetSection.Get())
		{
			Status += FString::Printf(TEXT("   Target: %s %.0f%%"),
				*Section->SectionName.ToString(), Section->CleanlinessPercent);
		}
		if (Total >= 0.f)
		{
			Status += FString::Printf(TEXT("   Aircraft: %.0f%%"), Total);
		}
		DrawText(Status, FLinearColor::White, 24.f, Canvas->SizeY - 48.f, UEngine::GetMediumFont(), 1.f);

		// Spray placeholder: line from reticle to the target section
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

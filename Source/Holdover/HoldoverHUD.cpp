// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "HoldoverAircraft.h"
#include "HoldoverCharacter.h"
#include "HoldoverGameState.h"
#include "HoldoverSprayComponent.h"
#include "HoldoverSurfaceSection.h"

namespace
{
	const TCHAR* HoldoverStateName(EHoldoverState State)
	{
		switch (State)
		{
		case EHoldoverState::HoldoverActive: return TEXT("HOLDOVER RUNNING");
		case EHoldoverState::HoldoverExpired: return TEXT("HOLDOVER EXPIRED - RETREAT");
		default: return TEXT("TREATMENT INCOMPLETE");
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

	// Treatment status (works for the character and the deicing truck alike)
	const APawn* Pawn = GetOwningPawn();
	const UHoldoverSprayComponent* Spray = Pawn ? Pawn->FindComponentByClass<UHoldoverSprayComponent>() : nullptr;
	if (Spray)
	{
		const bool bTypeI = Spray->ActiveFluidType == EHoldoverFluidType::TypeI;

		// Weather (replicated game state)
		float Weather = 0.f;
		if (const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			Weather = GS->WeatherSeverity;
		}

		FString Line1 = FString::Printf(TEXT("Fluid: %s   Type I: %.0f%%   Type IV: %.0f%%   Weather: %.0f%%"),
			bTypeI ? TEXT("TYPE I (clean)") : TEXT("TYPE IV (protect)"),
			Spray->TypeIFluid, Spray->TypeIVFluid, Weather * 100.f);

		if (AHoldoverSurfaceSection* Section = Spray->CurrentTargetSection.Get())
		{
			Line1 += FString::Printf(TEXT("   Target: %s clean %.0f%% prot %.0f%%"),
				*Section->SectionName.ToString(), Section->CleanlinessPercent, Section->ProtectionPercent);
		}
		DrawText(Line1, FLinearColor::White, 24.f, Canvas->SizeY - 72.f, UEngine::GetMediumFont(), 1.f);

		// Aircraft holdover line (colour-coded)
		for (TActorIterator<AHoldoverAircraft> It(GetWorld()); It; ++It)
		{
			const AHoldoverAircraft* Aircraft = *It;
			FLinearColor StateColor = FLinearColor::White;
			if (Aircraft->HoldoverState == EHoldoverState::HoldoverActive)
			{
				StateColor = Aircraft->HoldoverRemaining < 30.f ? FLinearColor::Yellow : FLinearColor::Green;
			}
			else if (Aircraft->HoldoverState == EHoldoverState::HoldoverExpired)
			{
				StateColor = FLinearColor::Red;
			}
			const FString Line2 = FString::Printf(TEXT("%s   clean %.0f%%   prot %.0f%%   holdover %.0fs"),
				HoldoverStateName(Aircraft->HoldoverState),
				Aircraft->GetOverallCleanliness(), Aircraft->GetOverallProtection(), Aircraft->HoldoverRemaining);
			DrawText(Line2, StateColor, 24.f, Canvas->SizeY - 48.f, UEngine::GetMediumFont(), 1.f);
			break;
		}

		// Shift status + dispatcher feed
		if (const AHoldoverGameState* GS = GetWorld()->GetGameState<AHoldoverGameState>())
		{
			FString ShiftLine = FString::Printf(TEXT("SHIFT %d/%d   time %.0fs   pay $%d   deductions $%d"),
				GS->OnTimeDepartures, GS->Quota, GS->ShiftTimeRemaining, GS->Money, GS->Deductions);
			DrawText(ShiftLine, FLinearColor::White, 24.f, 24.f, UEngine::GetMediumFont(), 1.f);

			if (GS->ShiftState == EShiftState::ShiftEnded)
			{
				const FString Banner = GS->bShiftPassed ? TEXT("SHIFT PASSED!") : TEXT("RESTRUCTURED - press E on the cube for a new shift");
				DrawText(Banner, GS->bShiftPassed ? FLinearColor::Green : FLinearColor::Red,
					24.f, 52.f, UEngine::GetLargeFont(), 1.2f);
			}

			// Last dispatch lines
			float Y = 84.f;
			for (int32 i = FMath::Max(0, GS->DispatchFeed.Num() - 3); i < GS->DispatchFeed.Num(); ++i)
			{
				DrawText(GS->DispatchFeed[i].ToString(), FLinearColor(0.7f, 0.9f, 1.f), 24.f, Y, UEngine::GetSmallFont(), 1.f);
				Y += 18.f;
			}
		}

		// Fluid-rule hint near the reticle
		if (!Spray->TargetHint.IsEmpty())
		{
			float TextW = 0.f;
			float TextH = 0.f;
			const FString Hint = Spray->TargetHint.ToString();
			GetTextSize(Hint, TextW, TextH, UEngine::GetMediumFont(), 1.f);
			DrawText(Hint, FLinearColor(1.f, 0.6f, 0.2f), CX - TextW * 0.5f, CY + 48.f, UEngine::GetMediumFont(), 1.f);
		}

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

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HoldoverInfoContent.generated.h"

/**
 * DEMO_CHECKLIST D1-D3: text-only first-shift tutorial, credits, legal notices
 * and feedback route. No art, no store page work.
 *
 * Content lives here (not in the widgets) so it is testable without rendering and
 * so a human can edit wording without touching UMG code. Anything that must be
 * supplied by a person is an explicit placeholder, never a guessed URL or name -
 * see Docs/AI/NEEDS_ALI.md.
 */
UCLASS()
class HOLDOVER_API UHoldoverInfoContent : public UObject
{
	GENERATED_BODY()

public:
	/** One first-shift tip: shown in order, one at a time. */
	struct FTip
	{
		FText Title;
		FText Body;
	};

	/** D1: the first-shift tutorial, in the order a new player needs it. */
	static TArray<FTip> FirstShiftTips();

	/** D1: a single rotating line for the HUD (short enough to read while playing). */
	static FText QuickTip(int32 Index);

	/** D2: credits. Placeholders are marked so Ali can fill them in. */
	static FText CreditsText();

	/** D2: legal notices / copyright. Placeholders marked. */
	static FText LegalText();

	/** D3: where players report bugs. Placeholder until Ali supplies the route. */
	static FText FeedbackText();

	/** True when the given text still contains an unfilled placeholder. */
	static bool HasPlaceholder(const FText& Text);

	/** Placeholder marker used across this content (testable). */
	static const TCHAR* PlaceholderTag() { return TEXT("[[TODO:ALI]]"); }
};

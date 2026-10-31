// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverPanelWidgets.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverBoardStateColorTest,
	"Holdover.UI.BoardStateColor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverBoardStateColorTest::RunTest(const FString& Parameters)
{
	// P7-3: the urgency colour ramp restored from the canvas HUD.
	TestEqual(TEXT("Running holdover is green"),
		UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState::HoldoverActive, 120.f),
		FLinearColor::Green);

	TestEqual(TEXT("Holdover under 30s is yellow"),
		UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState::HoldoverActive, 29.9f),
		FLinearColor::Yellow);

	TestEqual(TEXT("Expired holdover is red"),
		UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState::HoldoverExpired, 0.f),
		FLinearColor::Red);

	TestEqual(TEXT("Incomplete is white"),
		UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState::Incomplete, 0.f),
		FLinearColor::White);

	TestEqual(TEXT("Departed is white"),
		UHoldoverDepartureBoardWidget::HoldoverStateColor(EHoldoverState::Departed, 0.f),
		FLinearColor::White);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

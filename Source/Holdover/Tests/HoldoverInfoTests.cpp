// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HoldoverInfoContent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// DEMO_CHECKLIST D1-D3: the field-manual text is content, not UI, so it is
// testable without rendering. These assert the copy exists, covers the mechanics
// a new player needs, and still flags the placeholders Ali must fill in.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverInfoTutorialTest,
	"Holdover.Info.TutorialTips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverInfoTutorialTest::RunTest(const FString& Parameters)
{
	const TArray<UHoldoverInfoContent::FTip> Tips = UHoldoverInfoContent::FirstShiftTips();
	TestTrue(TEXT("has tutorial tips"), Tips.Num() >= 5);

	// Every tip needs a title and real body copy.
	for (int32 i = 0; i < Tips.Num(); ++i)
	{
		TestFalse(FString::Printf(TEXT("tip %d has a title"), i), Tips[i].Title.IsEmpty());
		TestFalse(FString::Printf(TEXT("tip %d has a body"), i), Tips[i].Body.IsEmpty());
		TestTrue(FString::Printf(TEXT("tip %d body is more than a stub"), i),
			Tips[i].Body.ToString().Len() > 40);
	}

	// The copy must actually teach the core loop: clean, protect, one tank, clock.
	const FString All = FString::JoinBy(Tips, TEXT("\n"), [](const UHoldoverInfoContent::FTip& T)
	{
		return T.Title.ToString() + TEXT(" ") + T.Body.ToString();
	});
	TestTrue(TEXT("teaches cleaning (Type I)"), All.Contains(TEXT("Type I")));
	TestTrue(TEXT("teaches protecting (Type IV)"), All.Contains(TEXT("Type IV")));
	TestTrue(TEXT("teaches the one-tank relay"), All.Contains(TEXT("ONE TANK")) || All.Contains(TEXT("one tank")));
	TestTrue(TEXT("teaches the holdover clock"), All.Contains(TEXT("holdover")) || All.Contains(TEXT("HOLDOVER")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverInfoQuickTipTest,
	"Holdover.Info.QuickTip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverInfoQuickTipTest::RunTest(const FString& Parameters)
{
	// QuickTip must cycle safely across any index (positive and negative).
	for (int32 i = -3; i < 24; ++i)
	{
		const FText Tip = UHoldoverInfoContent::QuickTip(i);
		TestFalse(FString::Printf(TEXT("quick tip %d is non-empty"), i), Tip.IsEmpty());
	}
	// And it must actually wrap rather than growing without bound.
	TestEqual(TEXT("index wraps"), UHoldoverInfoContent::QuickTip(0).ToString(),
		UHoldoverInfoContent::QuickTip(7).ToString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldoverInfoPlaceholdersTest,
	"Holdover.Info.CreditsLegalFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoldoverInfoPlaceholdersTest::RunTest(const FString& Parameters)
{
	const FText Credits = UHoldoverInfoContent::CreditsText();
	const FText Legal = UHoldoverInfoContent::LegalText();
	const FText Feedback = UHoldoverInfoContent::FeedbackText();

	TestFalse(TEXT("credits non-empty"), Credits.IsEmpty());
	TestFalse(TEXT("legal non-empty"), Legal.IsEmpty());
	TestFalse(TEXT("feedback non-empty"), Feedback.IsEmpty());

	// Legal must mention copyright; feedback must say where to report.
	TestTrue(TEXT("legal mentions copyright"), Legal.ToString().Contains(TEXT("Copyright")));
	TestTrue(TEXT("feedback mentions bugs"), Feedback.ToString().Contains(TEXT("BUG")) || Feedback.ToString().Contains(TEXT("bug")));

	// The legal block is a DRAFT only - it must stay labelled as such until
	// Ali/lawyer review it, and must never be presented as final.
	TestTrue(TEXT("legal is labelled DRAFT for Ali/lawyer review"),
		Legal.ToString().Contains(TEXT("DRAFT - not legal advice, needs Ali/lawyer review")));

	// Ali has not supplied studio name / licence / feedback route yet, so these
	// MUST still carry the placeholder marker. If this test fails, someone filled
	// a placeholder in - update NEEDS_ALI.md to match.
	TestTrue(TEXT("credits still has a placeholder"), UHoldoverInfoContent::HasPlaceholder(Credits));
	TestTrue(TEXT("legal still has a placeholder"), UHoldoverInfoContent::HasPlaceholder(Legal));
	TestTrue(TEXT("feedback still has a placeholder"), UHoldoverInfoContent::HasPlaceholder(Feedback));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

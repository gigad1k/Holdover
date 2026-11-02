// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverInfoContent.h"

// ---------------------------------------------------------------------------
// NEEDS_ALI #7 - EDIT THESE SIX VALUES.
//
// Everything the AI must not invent (names, copyright holder, URLs) lives here,
// one constant each, so supplying them is a six-line change with no other code
// touched. Until then each is the [[TODO:ALI]] marker and
// Holdover.Info.CreditsLegalFeedback asserts the markers are still present.
// After filling them in, flip that test to assert the markers are GONE.
//
// Keep the text as *literal* values, not URLs the AI guessed. If an item is
// unwanted (e.g. no EULA), set it to an empty string AND delete the line that
// references it below.
// ---------------------------------------------------------------------------
namespace
{
	const TCHAR* GStudioName    = UHoldoverInfoContent::PlaceholderTag(); // e.g. "Snowbound Games"
	const TCHAR* GCodeCredit    = UHoldoverInfoContent::PlaceholderTag(); // e.g. "the Holdover team" or a name
	const TCHAR* GArtCredit     = UHoldoverInfoContent::PlaceholderTag(); // e.g. "the Holdover team" or a name
	const TCHAR* GCopyrightLine = UHoldoverInfoContent::PlaceholderTag(); // e.g. "2026 Snowbound Games"
	const TCHAR* GEulaRef       = UHoldoverInfoContent::PlaceholderTag(); // e.g. "see EULA.txt in the install folder"
	const TCHAR* GFeedbackRoute = UHoldoverInfoContent::PlaceholderTag(); // e.g. an email, Discord invite, or issues URL
}

TArray<UHoldoverInfoContent::FTip> UHoldoverInfoContent::FirstShiftTips()
{
	TArray<FTip> Tips;
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T1T", "WELCOME TO THE RAMP"),
		NSLOCTEXT("HoldoverInfo", "T1B",
			"You are ground crew on a freezing night shift. Move with WASD, look with the mouse. "
			"Planes arrive, get de-iced, and leave before their holdover timer runs out. "
			"Up to 4 crew, zero qualifications.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T2T", "STEP 1 - CLEAN (TYPE I)"),
		NSLOCTEXT("HoldoverInfo", "T2B",
			"Aim at the aircraft and hold Left Mouse to spray. Type I fluid CLEANS ice off a section. "
			"A section is done when its cleanliness is full. Different sections (nose, wings, tail) "
			"each need their own pass.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T3T", "STEP 2 - PROTECT (TYPE IV)"),
		NSLOCTEXT("HoldoverInfo", "T3B",
			"Type IV fluid PROTECTS an already-clean section and starts its holdover clock. "
			"You cannot protect a dirty section. Clean first, protect second - that order is the job.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T4T", "ONE TANK EACH - SHARE THE WORK"),
		NSLOCTEXT("HoldoverInfo", "T4B",
			"Each crew member carries ONE fluid at a time. Pick your fluid at the refill station, "
			"then run back to the plane. Two crew - one cleans, one protects. That relay is the game.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T5T", "THE HOLDOVER CLOCK IS THE ENEMY"),
		NSLOCTEXT("HoldoverInfo", "T5B",
			"Once a section is protected it is on a timer. If protection decays below the threshold "
			"the clock stops and you must retreat. Keep the treatment topped up until departure.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T6T", "DEPART BEFORE IT EXPIRES"),
		NSLOCTEXT("HoldoverInfo", "T6B",
			"When every section is clean AND protected the plane is cleared. Get it away before "
			"the holdover expires - an expiry is a retreat and a deduction. Meet the shift quota to pass.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T7T", "WATCH THE ICE AND THE JETS"),
		NSLOCTEXT("HoldoverInfo", "T7B",
			"Icy patches will drop you. Jet blast from a running engine will launch you and cost the "
			"shift money. Funny for everyone except the person paying for it.") });
	Tips.Add({ NSLOCTEXT("HoldoverInfo", "T8T", "MIND THE DISPATCHER"),
		NSLOCTEXT("HoldoverInfo", "T8B",
			"The dispatcher is on the radio with new planes, weather warnings and bad news. "
			"Watch the departure board for what is cleared and what is running out of time.") });
	return Tips;
}

FText UHoldoverInfoContent::QuickTip(int32 Index)
{
	// Short enough to read mid-spray. Cycles.
	static const TCHAR* Lines[] = {
		TEXT("Type I cleans, Type IV protects. Clean first, always."),
		TEXT("One tank per crew member - split the work, not the fluid."),
		TEXT("Protect only clean sections. A dirty section refuses Type IV."),
		TEXT("Holdover clock only runs on protected sections. Keep them topped up."),
		TEXT("Refill at the station to switch fluid type."),
		TEXT("Depart before the holdover expires or the shift pays for it."),
		TEXT("Ice patches trip you. Jet blast launches you. Both cost money."),
	};
	const int32 Count = (int32)(sizeof(Lines) / sizeof(Lines[0]));
	const int32 Safe = (Index % Count + Count) % Count;
	return FText::FromString(Lines[Safe]);
}

FText UHoldoverInfoContent::CreditsText()
{
	return FText::FromString(FString::Printf(TEXT(
		"HOLDOVER\n"
		"A co-op comedy about de-icing aircraft with no qualifications.\n"
		"\n"
		"Studio: %s\n"
		"Design and code: %s\n"
		"Art and audio: %s\n"
		"\n"
		"Made with Unreal Engine 5.8.\n"
		"Unreal Engine is a trademark of Epic Games, Inc.\n"
		"\n"
		"Placeholder audio generated by Tools/build_placeholder_audio.py."),
		GStudioName, GCodeCredit, GArtCredit));
}

FText UHoldoverInfoContent::LegalText()
{
	return FText::FromString(FString::Printf(TEXT(
		"DRAFT - not legal advice, needs Ali/lawyer review\n"
		"=================================================\n"
		"\n"
		"Copyright (c) %s. All rights reserved.\n"
		"\n"
		"HOLDOVER is a work of fiction. Any resemblance to actual airports, "
		"airlines, or qualified ground crew is entirely coincidental.\n"
		"\n"
		"This software is provided for evaluation and playtesting purposes. "
		"Distribution, resale, or reverse engineering is prohibited except as "
		"permitted by applicable law.\n"
		"\n"
		"Third-party components and their licences are listed in the NOTICES.txt "
		"file shipped with this build.\n"
		"\n"
		"End User Licence Agreement: %s"),
		GCopyrightLine, GEulaRef));
}

FText UHoldoverInfoContent::FeedbackText()
{
	return FText::FromString(FString::Printf(TEXT(
		"FOUND A BUG? TELL US.\n"
		"\n"
		"Report anything broken, unfair, or unexpectedly funny.\n"
		"\n"
		"Feedback route: %s\n"
		"\n"
		"Please include:\n"
		"  - what you were doing when it happened\n"
		"  - how many players, host or client\n"
		"  - the step number from the demo checklist if you know it\n"
		"\n"
		"The build log is in Saved/Logs/ next to the game."),
		GFeedbackRoute));
}

bool UHoldoverInfoContent::HasPlaceholder(const FText& Text)
{
	return Text.ToString().Contains(PlaceholderTag());
}

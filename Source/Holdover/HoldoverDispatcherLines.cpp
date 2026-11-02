// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoverDispatcherLines.h"

namespace
{
	// Each event has tiers of lines. Tier 0 fires first; later tiers add variety.
	// Lines containing "{player}" get the blamed player's name injected.
	struct FDispatchBank
	{
		TArray<TArray<FString>> Tiers;
	};

	const TMap<EDispatchEvent, FDispatchBank>& GetBanks()
	{
		static const TMap<EDispatchEvent, FDispatchBank> Banks = {
			{ EDispatchEvent::ShiftStart, { {
				{ TEXT("Welcome to the ramp. Try not to break anything."), TEXT("Another night, another shift. The planes won't deice themselves. Probably."), TEXT("Shift start. Everyone look busy."), TEXT("Alright, listen up. We've got planes and we've got fluids. What could go wrong?"), TEXT("The tower says we're professionals. Let's not ruin the illusion.") } } } },

			{ EDispatchEvent::AircraftArrival, { {
				{ TEXT("New tin can on the stand. Go make it shiny."), TEXT("Aircraft arriving. That's your cue to look busy."), TEXT("Fresh plane just pulled in. It's already judging you."), TEXT("We've got a customer. Everyone pretend we know what we're doing."), TEXT("Aircraft on the stand. Clock's ticking before it turns into a popsicle.") } } } },

			{ EDispatchEvent::TreatmentComplete, { {
				{ TEXT("Not bad. For once."), TEXT("Someone actually did their job. Mark the calendar."), TEXT("Treatment complete. I'm almost impressed."), TEXT("That plane looks... acceptable. Let's go with that."), TEXT("Done! Now don't touch anything else.") } } } },

			{ EDispatchEvent::HoldoverStarted, { {
				{ TEXT("Clock's ticking. Don't get comfortable."), TEXT("Holdover running. You've got time. Not much."), TEXT("Protection's live. Get it in the air before it melts."), TEXT("Holdover started. The clock doesn't care about your feelings."), TEXT("Timer's running. Move like you mean it.") } } } },

			{ EDispatchEvent::HoldoverWarning, { {
				{ TEXT("Thirty seconds! Move it!"), TEXT("Clock's almost up! Push!"), TEXT("Almost out of time! Don't stop now!"), TEXT("Last call on that holdover! Go go go!") } } } },

			{ EDispatchEvent::HoldoverExpired, { {
				{ TEXT("Time's up! That plane's a popsicle again."), TEXT("Expired! Back to work, people."), TEXT("Holdover's gone. You know what that means. Start over."), TEXT("And that's why we don't dawdle. Retreat and re-treat!"), TEXT("Expired! The clock always wins eventually.") } } } },

			{ EDispatchEvent::DepartureOnTime, { {
				{ TEXT("Clean getaway. Almost professional."), TEXT("On time! Don't let it go to your head."), TEXT("That's how it's done. Mostly."), TEXT("Departure cleared. The airline is... tolerably pleased."), TEXT("And it's gone. Next!") } } } },

			{ EDispatchEvent::DepartureLate, { {
				{ TEXT("Late! That's coming out of someone's paycheck."), TEXT("Late departure. The airline is thrilled. Not."), TEXT("Missed the window. That's a paddlin'. -$50."), TEXT("Late. You had ONE job."), TEXT("And that's how we lose money, folks. -$50.") } } } },

			{ EDispatchEvent::IceStumble, { {
				{ TEXT("Ice on the ramp! Try walking, not sliding."), TEXT("Who ordered the ice rink?"), TEXT("Careful! That's not a slip-n-slide."), TEXT("Ice patch! Someone call maintenance. Or don't."), TEXT("And down goes another one. Shake it off!") } } } },

			{ EDispatchEvent::JetBlast, { {
				{ TEXT("Jet blast! Maybe stand somewhere else?"), TEXT("That's gonna leave a mark. -$25."), TEXT("Whoo! Free haircut from the jet blast. And a fine."), TEXT("Did you not see the big glowing hole? Keep clear!"), TEXT("Jet blast incident. Again. -$25.") } } } },

			{ EDispatchEvent::RefillComplete, { {
				{ TEXT("Tank filled. Try to make it last."), TEXT("Refilled. That stuff isn't free."), TEXT("All topped up. Now use it wisely. Hah."), TEXT("Tank's full. Go forth and spray.") } } } },

			{ EDispatchEvent::TankEmpty, { {
				{ TEXT("Running dry! Get to a station!"), TEXT("Tank's empty. That's a you problem."), TEXT("Click click. That's the sound of an empty tank."), TEXT("Empty! The station isn't gonna come to you.") } } } },

			{ EDispatchEvent::ShiftPassed, { {
				{ TEXT("Shift complete! You didn't completely fail."), TEXT("Against all odds, you passed. Barely."), TEXT("We did it! Nobody tell the auditors."), TEXT("Passed! Drinks are on... somebody."), TEXT("Look at that. A successful shift. Screenshot it.") } } } },

			{ EDispatchEvent::ShiftFailed, { {
				{ TEXT("RESTRUCTURED. Maybe next shift."), TEXT("Well, that happened. Back to training."), TEXT("Failed! But hey, we learned something. Probably."), TEXT("RESTRUCTURED. The beatings will continue until morale improves."), TEXT("That's a fail. At least the overtime is... not paid.") } } } },

			{ EDispatchEvent::NewShift, { {
				{ TEXT("New shift, new chance to disappoint."), TEXT("Round two! Or three. Who's counting?"), TEXT("Fresh shift. Everyone pretend this one matters."), TEXT("And we're back. Lower your expectations.") } } } },

			{ EDispatchEvent::PlayerConnected, { {
				{ TEXT("New hire just clocked in. Good luck with that."), TEXT("Someone else joined the chaos. Welcome!"), TEXT("We've got reinforcements! Or at least more witnesses."), TEXT("Another warm body on the ramp.") } } } },

			{ EDispatchEvent::PlayerDisconnected, { {
				{ TEXT("And someone just quit. Can't blame them."), TEXT("We lost one. Probably for the best."), TEXT("That's one fewer person to blame."), TEXT("And there they go. Probably going to a real job.") } } } }
		};
		return Banks;
	}
}

FString HoldoverDispatch::GetLine(EDispatchEvent Event, int32 RepetitionCount, const FString& PlayerName)
{
	const FDispatchBank* Bank = GetBanks().Find(Event);
	if (!Bank || Bank->Tiers.Num() == 0)
	{
		return TEXT("...");
	}

	// Pick the tier: index by repetition, clamp to last tier once exhausted.
	const TArray<TArray<FString>>& Tiers = Bank->Tiers;
	const int32 TierIndex = FMath::Clamp(RepetitionCount, 0, Tiers.Num() - 1);
	const TArray<FString>& Tier = Tiers[TierIndex];

	if (Tier.Num() == 0)
	{
		return TEXT("...");
	}

	// Pick a line within the tier. Deterministic on RepetitionCount so the same
	// event + count always yields the same line in tests.
	const FString& Line = Tier[RepetitionCount % Tier.Num()];

	// Inject player name if the line has a slot.
	FString Result = Line;
	if (!PlayerName.IsEmpty())
	{
		Result = Result.Replace(TEXT("{player}"), *PlayerName);
	}
	return Result;
}

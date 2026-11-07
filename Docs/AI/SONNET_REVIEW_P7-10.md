# Code review - Phases 7.6-10 (a930819..HEAD)

Reviewer: Sonnet 5.5, read-only. Static review only: nothing was built, run or played, and no engine source was read.
Every finding below cites code I read. Where a conclusion depends on engine behaviour I could not check, it says "verify".

**Read in full:** HoldoverCharacter, HoldoverSprayComponent, HoldoverDeicingTruck, HoldoverGameState, HoldoverAircraft, HoldoverSurfaceSection, HoldoverHazardVolume, HoldoverPlayerState, HoldoverGameMode, HoldoverPlayerController, HoldoverTestInteractable, HoldoverAudioLibrary, HoldoverHUD, HoldoverHUDWidget, HoldoverPanelWidgets, HoldoverInfoWidget, HoldoverShiftProfiles; `Tools/package_win64.ps1`, `Tools/run_tests.ps1`, the `Config/*.ini` diff, `Holdover.uproject`, both Build.cs files and both Target.cs files; `HoldoverWorldTests.cpp` (header, replication test, spray test) and the test-name inventory.
**Not reviewed:** HoldoverInfoContent, HoldoverDispatcherLines, HoldoverFluidData/ShiftData bodies, the HoldoverEditor module (HoldoverInputToolset), the Python map builders (only grepped), and the other test files beyond their names.

**No Critical findings.** Nothing I verified crashes or corrupts state on a normal single-host path. The two worst problems are co-op bugs (High #1 and #2).

---

## Critical
None.

---

## High

### H1. Four replicated GameState properties are never registered, so clients never receive them
- `Source/Holdover/HoldoverGameState.h:56,64,67,70` declare `ShiftNumber`, `TotalCompanyPay`, `TotalCompanyDeductions` and `CompanyLedger` as `UPROPERTY(ReplicatedUsing = OnRep_Shift)`.
- `HoldoverGameState.cpp:277-294` has no `DOREPLIFETIME` for any of them.
- **Effect:** on a client these stay at their defaults (`ShiftNumber = 1`, ledger 0).
  - `HoldoverHUDWidget.cpp:171-174` shows "SHIFT 1 ... ledger $0" forever on clients.
  - `GetProfileName()` (`GameState.cpp:88`) is wrong on clients after shift 1.
  - `MaybeAutoShowInfo()` (`HoldoverHUDWidget.cpp:140`) tests `GS->ShiftNumber > 1`, which is always false on clients.
  - Verify: the engine may also log or ensure on a `CPF_Net` property missing from `GetLifetimeReplicatedProps`.
- **Test gap:** `Holdover.MP.ReplicationContract` (`Tests/HoldoverWorldTests.cpp:122-203`) only checks the `CPF_Net` flag, which the specifier sets. It does not check registration, so it passes, and it does not list these four properties at all. The comment at lines 96-103 says registration is covered by code audit; this is a hole in that audit.
- **Fix:**
  - Add `DOREPLIFETIME(AHoldoverGameState, ShiftNumber/TotalCompanyPay/TotalCompanyDeductions/CompanyLedger)` in `GetLifetimeReplicatedProps`.
  - Add the four names to the replication-contract test.
  - Make the audit mechanical: add a test that greps or enumerates `CPF_Net` properties against a hand-kept list.
  - Consider replicating `ShiftNumber` only; the ledger can be derived on the client.

### H2. Aircraft `BeginPlay` overwrites replicated `bArrived`, so late joiners never see arrived aircraft
- `HoldoverAircraft.cpp:131-139`: `BeginPlay` runs on clients too and has no authority guard. It sets `bArrived = ArrivalDelay <= 0.f` and, when false, calls `SetActorHiddenInGame(true)` and `SetActorEnableCollision(false)`.
- On a client joining after the aircraft's arrival, the initial replicated `bArrived == true` is applied before `BeginPlay`. `BeginPlay` then flips it back to false and hides the aircraft.
- The server's value never changes, so `OnRep_Arrived` (line 398) never fires again. The aircraft stays hidden and collision-less on that client for the rest of the shift (until the next `ResetForNewShift` toggles it).
- This breaks the DES-1 staggered-arrival feature for anyone who joins mid-shift.
- **Fix:** guard the block with `if (HasAuthority())`. On clients, call `OnRep_Arrived()` from `BeginPlay`, or drive visibility only from the replicated value. Keep `SpawnLocation` capture server-side.
- **Tests:** add a world test that sets `bArrived = true` and then runs the client-side `BeginPlay` path.

### H3. `ServerSpray` is `Reliable` and called at 10 Hz per spraying client
- `HoldoverSprayComponent.h:148` is `UFUNCTION(Server, Reliable)`. It is sent every `SprayRequestInterval` (0.1 s, `Component.cpp:133-139`) while LMB is held.
- Under packet loss the reliable buffer backs up. The standard result is a hitch or a connection drop at the reliable-buffer limit.
- Spray is idempotent per request (dt is clamped), so a lost update costs nothing.
- **Fix:** make it `Unreliable`, as `ServerSetBoom` already is (`HoldoverDeicingTruck.h:120`).
- **Note:** also add a server-side rate limit (see M1), because unreliable plus unbounded rate is the cheat-friendly combination.

### H4. Shift-end state is not guarded, so scoring and ledger numbers move after the verdict
- `EndShift` (`GameState.cpp:171-181`) freezes `bShiftPassed`, but later paths still mutate `Money` and `Deductions`:
  - `Aircraft::Interact_Implementation` (`HoldoverAircraft.cpp:40-83`) allows departure after shift end. `bOnTime` is false, so it calls `RegisterDeparture(false)`, adding `LateDepartures++` and `Deductions += ...` and `PS->AddPenalty`.
  - `SimulateTreatment` keeps running after `EndShift` (the Tick at lines 157-206 never checks `ShiftState`). A holdover abort then calls `RegisterPenalty` (line 349).
  - `HazardVolume` `RegisterPenalty` (line 339) is also unguarded.
  - `Tick` also runs the arrival path when not in progress: `Elapsed = GetShiftDuration() - ShiftTimeRemaining`. After `EndShift`, `ShiftTimeRemaining = 0`, so `Elapsed == duration` and every un-arrived aircraft "arrives" at shift end, firing an `AircraftArrival` dispatch (lines 174-189).
- **Effect:** the results banner can show net pay inconsistent with the PASSED/RESTRUCTURED verdict. The first `RestartShift` rolls those late penalties into `TotalCompanyDeductions` (line 188).
- **Fix:**
  - Early-return from `Aircraft::Tick` (after the departure climb) and from `Interact` unless `GS->ShiftState == InProgress`.
  - Add the same guard in `RegisterDeparture` and `RegisterPenalty` (`if (ShiftState != InProgress) return;`).
  - Add a test: end the shift, then depart or penalise, and assert `Money` and `Deductions` are unchanged.

---

## Medium

### M1. No server rate limit on spray; the boom has one
- `UHoldoverSprayComponent::ServerApplySprayRequest` (`SprayComponent.cpp:206-255`) clamps dt per call (`ClampSprayDt`) but accepts unlimited calls per second. A modified client sending 1000 RPC/s of dt = 0.2 s each gets about 200 s of spray per real second, which fluid capacity alone does not bound (refills are cheap).
- **Fix:** reuse the token-bucket pattern from `AHoldoverDeicingTruck::ApplyBoomRequest` (`DeicingTruck.cpp:250-280`): bank `SprayRequestInterval * k` seconds, credit by `world time - last time`, and cap dt by the allowance. Add a test mirroring `BoomValidation`.

### M2. Spray and interact line-of-sight trace to the actor origin and only ignore the target
- `SprayComponent.cpp:243-252` and `HoldoverCharacter.cpp:397-409` trace from origin to `Target->GetActorLocation()`, ignoring only `Target`.
- Surface sections are separate `BlockAll` actors, one per aircraft zone. A section whose centre sits behind a neighbouring section (tail behind fuselage, far wing) is rejected even though the client's own trace hit it directly. This is a false rejection on a legitimate spray.
- `Tests/HoldoverWorldTests.cpp:519-575` (`LineOfSight`) only tests a lone box blocker, so this case is untested.
- **Fix:**
  - Have the client send the trace hit location (a `FVector_NetQuantize` or an `FHitResult`-style param). Validate that it is on or near `Target`'s bounds and within range.
  - Trace to that point, or to the nearest point on `Target->GetComponentsBoundingBox()`.
  - Alternatively ignore all sections of the same `OwningAircraft`.
  - Add an adjacent-sections test.

### M3. `RefillTimeRemaining` replicates every server tick to every client
- `SprayComponent.cpp:102` decrements it each tick, and line 321 replicates it unconditionally (`DOREPLIFETIME`), so one float per spray component per frame goes out.
- **Fix:** replicate `RefillEndServerTime` (via `GameState::GetServerWorldTimeSeconds()`) or a start time plus duration, once, and compute progress locally in `GetRefillProgress`. Or use `COND_OwnerOnly`.

### M4. Client sprint prediction ignores ice slowdown
- `IceScale` is only set on the server (`HazardVolume.cpp:297` under `HasAuthority()`, via `AHoldoverCharacter::SetIceSlowed`). It is not replicated.
- Sprint is applied locally first (`Character.cpp:273-289`, `SetSprintLocal` then `ServerSetSprint`). `RecomputeSpeedScale` on the client uses its own `IceScale = 1`, so `MaxWalkSpeed` jumps to `Base * SprintMult` while the server clamps to `Base * ice * sprint`. The result is CMC corrections or rubber-banding until `OnRep_SpeedScale` lands.
- The client also writes the replicated `SpeedScale` locally (line 310), so the replicated value is only restored when the server's value changes. Verify in PIE.
- **Fix:** replicate `IceScale` too (or derive it from an overlap count), or make `SpeedScale` server-only and have clients apply only the replicated value. A proper fix is a custom saved-move flag.

### M5. Ice slow-down is not overlap-counted
- `HazardVolume.cpp:344-357`: `OnOverlapEnd` always calls `SetIceSlowed(false, ...)`. A player leaving one ice patch while still inside another (or overlapping two volumes) loses the slow-down.
- **Fix:** keep a per-character overlap count (e.g. `TMap<TWeakObjectPtr<ACharacter>, int32>` in the volume, or a counter on the character) and clear only at zero.

### M6. Hazard volume client prediction is divergent and doubles up on the listen host
- `HazardVolume.cpp:273-285`: the locally-controlled client launches with its own `FRandRange` direction. The server launches again with a different random direction at lines 302-304 and 320-328.
- On a client this leaves the predicted position diverging from the server's, so you get a correction. On the listen host both launches run on the same character in the same frame (the `IsLocallyControlled` block then the authority block), and the second overrides the first.
- Also, the jet-blast cooldown early return (lines 315-324) still launches, and the local cue fires every time.
- **Fix:** do prediction only on `!HasAuthority()`. Make the launch direction deterministic (derived from the character's forward and the hazard location), or skip prediction and let the server launch drive the correction. Keep the sound local.

### M7. `JetBlastCooldownMap` is never pruned
- `HoldoverHazardVolume.h:64` is `TMap<TWeakObjectPtr<AActor>, float>`. Entries for destroyed or respawned characters are never removed.
- It is bounded in a normal session, but each respawn adds a dead-weak entry. `OnOverlapEnd` returns early for non-ice (line 348), so there is no cleanup hook.
- **Fix:** prune stale entries on insert (`RemoveAll` of invalid keys), or add `Now - Last > Cooldown` expiry.

### M8. A disconnecting truck occupant leaves an orphan Character
- `GameMode::Logout` (`HoldoverGameMode.cpp:35-48`) and `PawnLeavingGame` (`PlayerController.cpp:237-249`) call `Truck->EvictOccupant()` and `UnPossess()`, then `Super`. `EvictOccupant` (`DeicingTruck.cpp:193-206`) only unhides the character and resets `OccupantCharacter`.
- The character is no longer the controller's pawn, so `Super::Logout` cannot destroy it. It remains in the world uncontrolled and visible, and the shift state keeps it at that spot until the level ends.
- **Fix:** destroy or possess-null the occupant when the controller is leaving. For example, `Pawn->Destroy()` in `EvictOccupant` when called from `Logout`. Alternatively have `Logout` capture `OccupantCharacter` and call `Possess(Occupant)` before `Super::Logout`. Add a test.
- **Cleanup:** the logic is duplicated in two places (GameMode and PlayerController). Keep one.

### M9. Results screen shows only the local player on clients
- `HoldoverPanelWidgets.cpp:247-263` iterates `GetPlayerControllerIterator()`. A client only has its own controller, so a co-op client's results screen lists one row instead of the whole crew.
- **Fix:** iterate `GS->PlayerArray` and cast to `AHoldoverPlayerState`.

### M10. Results screen shows the wrong fluid for every player
- `AHoldoverPlayerState::SetCarriedFluid` (`HoldoverPlayerState.cpp:34-39`) has no callers (the only references to it are its definition and declaration). Fluid is tracked on `UHoldoverSprayComponent` (`SprayComponent.cpp:304`).
- `PanelWidgets.cpp:259` reads `PS->CarriedFluidType`, which stays `TypeI`, so the "[Type I]" tag is permanently wrong.
- **Fix:** either call `SetCarriedFluid` from `ValidateAndBeginRefill` for the owning controller's PlayerState, or read the spray component's fluid. Remove the duplicate replicated property if it is not needed.

### M11. Field-manual widget accumulates headings and reuses object names
- `HoldoverInfoWidget.cpp:299-319` (`AddSection`) adds a heading `UTextBlock` and a body `UTextBlock`, but only the body is tracked in `BodyLines`. `RefreshContent` (lines 352-387) removes only `BodyLines`, so every call appends all headings again. The comment "keeps this idempotent" is not true.
- `RebuildWidget` already calls `RefreshContent()` (line 343), and `MaybeAutoShowInfo` and the F1 toggle call it again. So the headings are duplicated on first open and on every open after that.
- Names are reused for each rebuild (`InfoTip%d`, `InfoCredits`, ...) via `NewObject(this, Name)` while the removed widgets are not yet garbage-collected. Verify in PIE whether the engine reuses or asserts.
- **Fix:** track the heading widgets in the same array, or clear `Box` children except `Title` before rebuilding. Use `NAME_None` (or unique names) for rebuilt widgets. Add a test: refresh twice and assert the child count is stable.

### M12. Departure board only shows one aircraft
- `PanelWidgets.cpp:86-91` caches the first `TActorIterator<AHoldoverAircraft>` result and `break`s. With the Phase 7.6 two-plane shift, the board shows an arbitrary aircraft, which may be un-arrived, hidden or already departed.
- **Fix:** show all arrived, non-departed aircraft (one row each), or pick the one nearest the local pawn.

### M13. Shift profile pay values are dead data
- `HoldoverShiftProfiles.cpp` sets per-profile `PayPerOnTimeDeparture` and `LateDepartureDeduction` (e.g. night 4 late = 75, night 5 abort = 40, with pay always 100).
- `GameState::StartShift` (lines 101-154) copies only `ShiftDuration` and `ShiftQuota`. `RegisterDeparture` and `Aircraft::Interact` use `GS->PayPerOnTimeDeparture` and `LateDepartureDeduction` (GameState members, EditDefaultsOnly), so the profile values are ignored. Only `HoldoverAbortPenalty` is actually consumed via `GetHoldoverAbortPenalty`.
- The `Holdover.Treatment.ShiftProfiles` test cannot catch this, because it checks the data, not the effect.
- **Fix:** in `StartShift`, set `PayPerOnTimeDeparture` and `LateDepartureDeduction` from the profile (both are already replicated), and add a test that shift 4 produces a 75 deduction.

### M14. Audio is synchronously loaded on every play; looping wind is never stopped
- `HoldoverAudioLibrary.cpp:11-12` does `LoadObject` by string path on every `Play2D`: spray start, each dispatch beep, each hazard cue.
  - Cost: a disk or asset-registry hitch on first use, plus a warning log on every call if the asset is missing.
  - The sound is not held by any `UPROPERTY`, so it can be GC'd between plays.
  - **Fix:** cache the loaded `USoundWave`s in a `TMap<FName, TObjectPtr<USoundWave>>` UPROPERTY on a `UGameInstanceSubsystem`, or use a data asset with soft references the cooker can see. Then `DirectoriesToAlwaysCook` is a safety net, not the mechanism.
- `HoldoverCharacter.cpp:75-78`: `Play2D(wind_loop, looping)` runs at `BeginPlay` and the returned `UAudioComponent` is discarded. Each Character `BeginPlay` (respawn, PIE restart, a travelling pawn) starts another loop with no stop at `EndPlay`.
  - **Fix:** keep the component in a `UPROPERTY(Transient)` and `Stop()` it in `EndPlay`, or start the loop once from the PlayerController.
- `GameState.cpp:242-245` and `HazardVolume.cpp:277` call `Play2D` on a dedicated server, which only wastes a `LoadObject` and may log the "MISSING" warning. Guard with `GetNetMode() != NM_DedicatedServer`.
- `GameState::OnRep_Dispatch` (lines 301-308) also beeps on the initial join replication. Skip it when the feed length did not grow.

### M15. `run_tests.ps1` expected list is stale, so a missing test can pass silently
- `Tools/run_tests.ps1:79-87` lists 7 tests. The suite now has 26 (`Holdover.Info.*`, `Holdover.MP.*`, `Holdover.Treatment.*`, `Holdover.Spray.DtClamp`, `Holdover.Movement.SpeedScale`, ...).
- Anything outside the list is reported as "extras". Failures are counted, but a test that disappears (deleted, no longer compiled, renamed) never makes the run exit 2, because only the 7 are checked as "missing".
- **Fix:** generate the expected list at start (e.g. `Automation List` output) or add the 19 names, and add a minimum-count assertion (`Queue Empty N tests performed` must be at least the expected count).
- **Also:** the default `$UEPath` (`D:\Gamez\EPIC\UE_5.8\...`, line 67) differs from the default `$UERoot` in `package_win64.ps1` (`C:\Program Files\Epic Games\UE_5.8`, line 48), and `Resolve-ProjectPath` has a hardcoded `D:\Projects\HOLDOVER` fallback (line 110). Resolve the engine path from one env var or one shared config.

### M16. Packaging smoke test is a weak gate
- `Tools/package_win64.ps1:130-160`: the game is started headless for a fixed 20 s and killed. The only failure signal is the regex "Fatal error|Assertion failed|Critical error|appError called".
  - It never checks that the map actually loaded (e.g. that the log contains `LogHoldover` startup lines).
  - It would pass if the game hung before reaching the map.
  - It does not flag the failure the cook fix was meant to prevent (`PLACEHOLDER AUDIO MISSING`, log lines from `HoldoverAudioLibrary.cpp:15`), nor `LogLinker: Error`.
- `Get-ChildItem ... -Recurse | Select-Object -First 1` (lines 111-112) picks whichever `Holdover.exe` it finds first. A packaged tree normally has both a root launcher stub and `Holdover\Binaries\Win64\Holdover.exe`. If the first match is the stub, `Stop-Process -Id $Proc.Id` (line 140) may not kill the child game process (verify). The order is also filesystem dependent.
- **Fix:**
  - Locate the exe explicitly (`...\Binaries\Win64\Holdover.exe`) and kill the process tree (`taskkill /T`) or find the child by name.
  - Require the presence of an expected log line such as `Field manual built` or the existing shift-start `LogHoldover` line (the manual line only appears when the widget is built, per `HoldoverInfoWidget.cpp:346`), and fail on `PLACEHOLDER AUDIO MISSING` and `Error:` from `LogLinker`/`LogHoldover`.
  - Also fail if the process exits early with a non-zero code.

### M17. Aircraft sections are separate actors; verify they are hidden with the aircraft
- `Aircraft::BeginPlay` and `ResetForNewShift` (`HoldoverAircraft.cpp:96-114,127-139`) call `SetActorHiddenInGame` and `SetActorEnableCollision` on the aircraft actor only.
- `AHoldoverSurfaceSection` is its own actor (`SurfaceSection.cpp:11-25`, own root mesh, `BlockAll`). I found no attach call in the aircraft code or in `Tools/build_ramp_greybox.py` or `mapbuild_common.py` (grep for "Attach" returned nothing).
- If sections are not attached to the aircraft, a not-yet-arrived or departed aircraft's sections stay visible and sprayable. `ServerApplySprayRequest` does not check the owning aircraft's `bArrived` or `bDeparted`.
- **Fix:** in `SetArrived`/depart, also hide the sections (`for Section : Sections { Section->SetActorHiddenInGame(...); SetActorEnableCollision(...) }`), or attach them with `AttachToActor` so they follow. In `ServerApplySprayRequest`, reject targets whose `OwningAircraft` is not arrived or has departed.
- This needs checking against the actual level, since I could not read the .umap.

---

## Nit

- **N1.** `AHoldoverDeicingTruck::Tick` (`DeicingTruck.cpp:240-243`) is empty but `PrimaryActorTick.bCanEverTick = true` (line 25). Remove the tick, or set `bCanEverTick = false`.
- **N2.** `HoldoverHUDWidget.cpp:171-175, 197-215` and `PanelWidgets.cpp:96-122` rebuild and `SetText` strings every frame (plus `FindComponentByClass` each frame). Cache the last string or values and call `SetText` only on change. This matters most for `ShiftLine` (printf of 9 values per frame).
- **N3.** `YawBoom` / `ExtendBoom` (`DeicingTruck.cpp:287-305`) send an unreliable `ServerSetBoom` every frame, 60-144 per second, while a key is held. 10-20 Hz is enough. The owner is also excluded from replication (`COND_SkipOwner`, line 452), so if the server's token bucket clamps a move, the owner's boom and nozzle drift from the server's with no reconciliation.
- **N4.** `ServerExit_Implementation` (`DeicingTruck.cpp:159-182`) re-possesses the character where it stood at entry and re-enables collision there (`SetVehicleHidden(false)`). It is fine while `bDrivingEnabled = false`, but teleport the character to a safe exit point beside the truck before enabling driving.
- **N5.** `HoldoverTestInteractable.cpp:405-415` calls `ResetForNewShift` on every aircraft and then `RestartShift` (which does so again, `GameState.cpp:194-201`). Drop the first loop. `static bool bToggle` (line 423) is shared across all instances. Make it a member. Consider guarding `RestartShift` with `ShiftState == ShiftEnded` and `HasAuthority()`.
- **N6.** The public shift API (`StartShift`, `EndShift`, `RestartShift`, `RegisterDeparture`, `RegisterPenalty`) has no `HasAuthority()` guard (`GameState.cpp:101-231`), unlike `SetWeatherSeverity`. All current callers are server-side, but a client call would silently desync. Add a guard or a `check(HasAuthority())`.
- **N7.** `ValidateAndBeginRefill` (`SprayComponent.cpp:286-293`) runs a `TActorIterator` over all stations per RPC. Cache the stations on `BeginPlay`, or cap the RPC rate, since the RPC is client-callable.
- **N8.** `Aircraft::Tick` (`HoldoverAircraft.cpp:198-204`) copies tuning from the GameState into `EditAnywhere` UPROPERTYs each tick. Read through the GameState getters in `SimulateTreatment` instead, so designer-edited values are not overwritten.
- **N9.** `Holdover.Build.cs:11-24` lists `AIModule`, `StateTreeModule` and `GameplayStateTreeModule`, and the uproject lists `AIModule` and `UMG` as module additional dependencies. I did not see any use of AI or StateTree in the code I read. If they are unused, drop them to shorten link time and the packaged dependency surface.
- **N10.** `PawnLeavingGame` in the PlayerController duplicates the GameMode `Logout` eviction (see M8). Keep one path.
- **N11.** `GetCurrentProfile()` is a `const` method returning a mutable pointer, and `ShiftProfiles` is `EditDefaultsOnly` but filled at runtime with `NewObject(this)` (`ShiftProfiles.cpp:415`, `GameState.cpp:74`). It is GC-safe because the array is a UPROPERTY, but clients never have it (not replicated), so any client call to the profile getters silently falls back to `ShiftData` or the hardcoded defaults. This ties into H1: derive client-side display values from replicated fields only.

---

## Test and config coverage gaps (summary)

| Area | Status |
|---|---|
| Replication registration (`DOREPLIFETIME`) | Not tested. The contract test checks `CPF_Net` only; it missed H1. |
| Client-side `BeginPlay` for replicated state (H2) | No test. |
| Shift-ended guards (H4) | No test. |
| Spray rate limit (M1) | No test (boom has one). |
| LOS with adjacent sections (M2) | No test. |
| `HazardVolume` (ice overlap count, jet-blast cooldown, launch) | No test. |
| `GameMode::Logout` / truck occupant eviction | No test. |
| Truck `TryEnter` / `ServerExit` | No test. |
| Profile pay values taking effect (M13) | Data is tested, effect is not. |
| `Holdover.Info.*` widget refresh idempotence (M11) | Content tested, rebuild not. |

**Config:** `Config/*.ini` changes look sound. The Android File Server plugin and its committed token are disabled, `ServerDefaultMap` points at the ramp map, and `DirectoriesToAlwaysCook` covers `/Game/Holdover/Audio`.
- I could not verify that `GlobalDefaultGameMode` (`BP_FirstPersonGameMode`) derives from `AHoldoverGameMode`; check that in the editor.
- I found nothing wrong in the Build.cs or Target.cs files. The editor-only module and plugins are correctly gated by `"Type": "Editor"` and `TargetAllowList`.

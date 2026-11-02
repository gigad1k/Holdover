# CHANGELOG

## 2026-11-02 - Phase 8a: atmosphere pass (cold blue night, warm orange ramps)
- `Tools/build_ramp_atmosphere.py` (new): snowy-night atmosphere for `L_RampGreybox`
  with no purchased assets. Cold steel-blue moonlight (intensity 3.0) + cool sky
  ambient (0.8) + cold blue-grey volumetric height fog (density 0.03) + four warm
  sodium-orange spot pools (9000cd) under the ramp light poles + mild post-process
  (bloom 0.75, vignette 0.28, AO 0.55, auto-exposure 0.55-1.85, slight cool
  desaturation). All tagged `builder:atmosphere`; idempotent.
- Every run ends with a READBACK block proving each value persisted (headless, so
  the render cannot be seen - see Docs/AI/NEEDS_ALI.md for the eyeball pass).
- UE 5.8 API notes (found via `Tools/probe_atmo_names.py` / `probe_pp_names.py`):
  PostProcessVolume keeps `Settings`/`Unbound`/`BlendRadius` on the actor (no
  `PostProcessComponent`); override flags are `override_<name>`; `ColorSaturation`
  and `ColorContrast` are `FVector4` not `FLinearColor`; fog volumetric toggle is
  `enable_volumetric_fog` and fog colours go through `set_*_inscattering_color()`.
- Tests: `run_tests.ps1` 7/7 pass (C++ unchanged).

## 2026-11-02 - Phase 8a: PROC-5 builder actor-ownership model
- `Tools/mapbuild_common.py` (new): shared ownership helpers. Builders tag their
  actors `builder:<name>` and clear **only** their own, so hand-placed / Phase 8
  art work survives any builder run. Fixes CODE_REVIEW PROC-5 ("the greybox
  builder deletes hand-placed work"), which blocked Phase 8.
- `Tools/build_ramp_greybox.py`: no longer clears every non-system actor. Clears
  only `builder:greybox` actors plus untagged legacy actors matching its own
  labels (one-time migration of the pre-ownership map). Lighting is tagged
  `builder:atmosphere` and created only as a fallback, so the atmosphere pass can
  retune it without duplicates.
- `Tools/verify_proc5.py` (new): plants an untagged hand-placed probe actor,
  re-runs the greybox builder, and reports survival. Verified: probe SURVIVED,
  `should_clear_by_greybox=False`, owners = 4 atmosphere / 30 greybox / 1 untagged.
- Run order is now `build_ramp_greybox.py` first, then `build_ramp_atmosphere.py`.
- Tests: `run_tests.ps1` 7/7 pass (C++ unchanged).

## 2026-11-02 - Phase 7.6: build + test fixes (compile clean, 16/16 pass)
- `HoldoverInputToolset.h/cpp`: `GetPawnInfo` JSON updated to the DES-2 one-tank model
  (`fluidLevel`/`fluidType`) - was still reading removed `TypeIFluid`/`TypeIVFluid`/
  `ActiveFluidType` fields.
- `HoldoverSprayComponent.cpp`: `TickComponent` refill countdown used `HasAuthority()`,
  which `UActorComponent` does not have - now `GetOwner()->HasAuthority()`.
- `HoldoverGameState.cpp`: `StartShift` and `RestartShift` dereferenced `GetWorld()`
  without a null check (crashed `Holdover.Shift.ScoringAndRestart`, which builds a
  world-less GameState). Added guards; also added the missing `#include "Holdover.h"`
  for `LogHoldover` that unity builds had been masking.
- Tests updated to match intended DES-4 / SIM-2 behaviour (not regressions):
  `Holdover.Shift.ScoringAndRestart` now ticks past `ShiftTimeRemaining` since DES-4
  profiles own the duration; `Holdover.Treatment.HoldoverStateMachine` now asserts
  protection is held during active holdover (SIM-2 pauses degradation).

## 2026-10-31 — Phase 7.6: DES-6 playtest session script
- `Docs/DES6_PLAYTEST_SESSION.md`: complete 2-player playtest script — session setup, pre-
  session brief, 22-item observation checklist across 3 shifts, DES-6 gate questions
  (laughter + coordination), post-session interview, results template, evidence capture plan.
  Ready to run. Gate must pass before Phase 8 art pass.

## 2026-10-31 — Phase 7.6: DES-4 escalating shift profiles + running ledger
- `HoldoverShiftProfiles.h/cpp`: 5 built-in escalating profiles (Night 1 Orientation ->
  Night 5 Blizzard). Weather severity, holdover duration, deadlines, contamination and pay
  all tighten across profiles. `CreateDefaultProfiles()` returns them; designers can override
  with `UHoldoverShiftData` assets.
- `AHoldoverGameState`: `ShiftNumber` (1-based, advances on restart), `ShiftProfiles` array,
  `GetCurrentProfile()` reads tuning from the active profile. `CompanyLedger` /
  `TotalCompanyPay` / `TotalCompanyDeductions` track cumulative results across shifts.
- HUD shows shift number, profile name and running ledger.
- Test: `Holdover.Treatment.ShiftProfiles` — 5 profiles escalate in difficulty. Not run.

## 2026-10-31 — Phase 7.6: SIM-1..SIM-4 + DES-5 data assets
- **DES-5**: `UHoldoverFluidData` (spray/tank tuning) and `AHoldoverShiftData` (shift/weather/
  holdover/pay tuning) data assets. Spray component and GameState read from the assets with
  local-default fallbacks. Aircraft syncs tuning from GameState each tick.
- **SIM-1**: Weather reset/randomisation per shift — base severity randomised per shift,
  squalls/lulls via sine oscillation (`WeatherAmplitude`, `WeatherPeriodSeconds`) instead of a
  monotonic ramp to max. `WeatherTime`/`WeatherBaseSeverity`/`WeatherPhase` tracked per shift.
- **SIM-2**: Degradation paused during active holdover (`HoldoverDegradationScale`, default 0)
  so expiry is the clock players fight. Holdover abort charges a penalty (`HoldoverAbortPenalty`).
- **SIM-3**: Precipitation re-contaminates unprotected sections
  (`ReContaminationRatePerSecond` * weather). Sections above `ReContaminationProtectionThreshold`
  are immune. `ApplyContamination()` on surface sections.
- **SIM-4**: Net pay = money − deductions; `bShiftPassed` requires both quota met AND positive
  net pay. Per-aircraft deadlines (`PerAircraftDeadlineSeconds`): late if past deadline even
  during an active shift. HUD shows net pay.
- Tests: `Holdover.Treatment.Contamination`, `Holdover.Treatment.NetPay`. Not run.

## 2026-10-31 — Phase 7.6: DES-3 dispatcher line bank + ice stumble + jet blast cooldown
- `HoldoverDispatcherLines.h/cpp`: 79 reactive dispatcher lines across 17 events, keyed by
  event + repetition tier + blamed player. RESTRUCTURED voice (underqualified crew comedy).
  `HoldoverDispatch::GetLine(Event, Repetition, PlayerName)` is deterministic for tests.
- `AHoldoverGameState::DispatchEvent(Event, PlayerName)` tracks per-event repetition and fires
  a bank line. All 10 hardcoded `AddDispatch` call sites replaced with `DispatchEvent`.
- Ice stumble: entering an ice volume launches the character (small stumble, `IceStumbleStrength`)
  so the hazard is physical, not just a speed number. Client-predicted launch for instant feedback.
- Jet blast: per-player cooldown (`JetBlastCooldown`, 5s) so the same player isn't fined
  repeatedly. Launch always fires; penalty respects the cooldown. Penalty attributed to the
  player's PlayerState.
- Test: `Holdover.Treatment.DispatcherBank` — all 17 events return lines, repetition varies
  output, exhausted tiers cycle safely. Not run — manual build required.

## 2026-10-31 — Phase 7.6: DES-2 PlayerState + one-tank rule
- `AHoldoverPlayerState`: per-player earnings/penalties/departures attribution, replicated;
  `CarriedFluidType` (one-tank rule). Credited on departure request and shown on the results
  screen ("Dan [Type I] +$100 -$25 departures 2").
- One-tank-per-player: spray component carries a single `FluidLevel` + `CarriedFluidType`
  (replaces `TypeIFluid`/`TypeIVFluid`/`ActiveFluidType`). Q toggle removed — fluid is chosen
  at the refill station.
- Timed refills: `BeginRefill(Type)` at a refill station starts a `RefillDuration` (3s) countdown;
  `RefillTanks()` removed. `HoldoverTestInteractable` is now a refill station with a configurable
  `StationFluidType` (place one per type to force the clean→protect relay).
- Dispatcher lines now name the responsible player ("Dan departure cleared. +$100.").
- Results screen shows per-player attribution lines.
- Tests: `Holdover.Treatment.PlayerStateAttribution`, `Holdover.Treatment.OneTankRefill` added;
  `Holdover.Treatment.FluidRules` updated for single-tank API. Not run — manual build required.

## 2026-10-31 — Phase 7.6: DES-1 staggered arrivals + SIM-5 section ownership
- `ArrivalDelay` + `bArrived` (replicated): aircraft are hidden/off-collision until the shift
  clock passes their delay. Dispatch announces arrival. `ResetForNewShift` restores the hidden state.
- `AHoldoverSurfaceSection::OwningAircraft` explicit ownership: radius-gather no longer steals
  sections owned by another aircraft (SIM-5). `RegisterSection` claims unowned sections only.
- Shift spec updated to the plan's first shift: 2 aircraft, 8 min, quota 2 (`HoldoverGameMode.h`).
- Test: `Holdover.Treatment.SectionOwnership` added. Not run — manual build required.

## 2026-10-29 — Phase 7: UMG HUD + dispatcher feed + placeholder audio
- UMG HUD: `AHoldoverHUD` keeps the canvas centre reticle + interaction prompt and creates
  `UHoldoverHUDWidget` on BeginPlay; the widget builds its tree in C++ (`RebuildWidget`) and reads
  live game state every tick — shift line (SHIFT n/quota, time, pay, deductions), treatment line
  (active fluid, Type I/Type IV tank %, weather %, current target section clean/prot, fluid-rule
  hint), `UHoldoverDepartureBoardWidget` (aircraft holdover state + countdown + overall
  clean/protection), `UHoldoverDispatcherFeedWidget` (last 5 dispatch lines),
  `UHoldoverShiftResultsWidget` (SHIFT PASSED!/RESTRUCTURED + breakdown, visible only when the
  shift ended).
- Treatment line is pawn-agnostic (works from the truck too).
- `UHoldoverAudioLibrary` (LoadSound/Play2D) loads `/Game/Holdover/Audio/<Name>` and logs a LOUD
  'PLACEHOLDER AUDIO MISSING' warning instead of failing silently.
  `Tools/build_placeholder_audio.py` generates 4 deterministic stdlib WAVs and imports them as
  USoundWave assets: radio_beep, spray_hiss (loop), wind_loop (loop), collision_cue; loop flags
  set on the two loops (looping comes from `USoundWave::bLooping` — `SpawnSound2D` has no loop
  parameter).
- Audio wiring: wind_loop on character BeginPlay (locally controlled), radio_beep on server
  `AddDispatch` + client `OnRep_Dispatch`, spray_hiss while spraying (owning client), collision_cue
  when the local player enters a hazard volume.
- Fixed: `HoldoverPanelWidgets.h` was missing `RebuildWidget` declarations (compile error) — added;
  `SlateCore` added to `Holdover.Build.cs` (`FCoreStyle` link error).
- Code-review fixes (`Docs/CODE_REVIEW_2026-10-29.md` P7-1..P7-4): **P7-1** the shift-results
  banner is parent-driven — a self-`Collapsed` UserWidget stops receiving `NativeTick`, so the
  banner could never re-show; `UHoldoverHUDWidget::NativeTick` now owns its visibility. **P7-2**
  the spray-hiss stops on every possession-lifetime edge (`UnPossessed`/`OnRep_Controller`/
  `EndPlay` + owner-side exit; the `SetSpraying(false)` stop branch is unconditional) and hiss
  start/stop is logged. **P7-3** the departure board restores the holdover urgency colour ramp
  (running=green, <30s=yellow, expired=red) as pure `HoldoverStateColor` with an automation
  test. **P7-4** `build_placeholder_audio.py` `detrend()` algebra fixed (pins both loop
  endpoints to zero — the old form left the first sample nonzero and clicked at the seam);
  the generator now asserts the seam.
- Multiplayer fluid bug fixed in this tree (MP-1): active fluid is server-authoritative
  (`ServerSetFluid` RPC + replicated `ActiveFluidType`), so remote clients spray what their HUD
  says. Client-seat verification is Phase 7.5 step 2.
- Verification (2026-10-29): `HoldoverEditor` Win64 Development build clean; **7/7** automation
  tests PASS (incl. new `Holdover.UI.BoardStateColor`); placeholder audio regenerated with seam
  self-check (`spray_hiss`/`wind_loop` endpoints 0.000000), no missing-audio warnings; 2-player
  listen-server PIE clean (join, shift start dispatch) and the shift-end banner renders red
  "RESTRUCTURED" at expiry — evidence `Docs/evidence/phase75_p7_results_banner.png` (client
  window; the host window's banner area is occluded by the editor Message Log in that capture).
  P7-2 hiss lifecycle and P7-3 colour states in live play still need manual playtest (no input
  injection automation yet).

## 2026-10-29 — Phase 6: deicing truck & boom
- `AHoldoverDeicingTruck`: parked truck with rotating/extending boom + basket nozzle (replicated
  `BoomYaw`/`BoomExtension`, clamped `SetBoom`), enter/exit via interact (server-authoritative
  possession; occupant character hidden/parked), spray from the nozzle through the shared
  `UHoldoverSprayComponent` (`SprayOrigin` override). Arcade driving is modular behind
  `bDrivingEnabled` (default **false** — parked-truck demo per the playbook).
- `UHoldoverSprayComponent`: optional `SprayOrigin` so any pawn (truck nozzle) can spray.
- HUD is pawn-agnostic (treatment panel works from the truck).
- Controls in truck: A/D boom yaw, W/S extend/retract, LMB spray, E exit; arrows drive (flagged).
- Test `Holdover.Truck.BoomClampAndApply` — **all 6 tests PASS** via MCP. PIE with the truck runs
  clean (shift start dispatch, no errors).

## 2026-10-28 — Phase 5: hazards, scoring, shift loop
- `AHoldoverGameState`: shift lifecycle (start/tick/end/restart), quota, on-time vs late
  departures, pay/deductions, pass/fail evaluation, replicated dispatcher feed (8 lines).
- `AHoldoverAircraft`: departure via the interact pattern ("Press E - Request Clearance") —
  `TryDepart` requires a running holdover and succeeds **exactly once** (`bDeparted`);
  departed aircraft climbs away; `ResetForNewShift` makes the loop repeatable.
- `AHoldoverHazardVolume`: ice (traction halved while overlapped, fair recovery) and jet blast
  (launches the player + penalty + dispatch line). Spawned on the ramp's marked locations.
- HUD: shift clock/quota/pay/deductions, pass/fail banner, dispatcher feed.
- Shift restart without app restart: E on the test cube after shift end resets aircraft + counters.
- Tests: `Holdover.Shift.DepartureOnlyOnceAndValid` + `Holdover.Shift.ScoringAndRestart` —
  **all 5 Holdover tests PASS** via MCP. PIE: shift starts, dispatch feed live.

## 2026-10-28 — Phase 4: Type IV, tanks, weather, holdover
- `UHoldoverSprayComponent`: two tanks (Type I / Type IV) with separate rates/capacities, **Q** to
  toggle active fluid; `ApplySpray` core rules — Type I cleans, Type IV protects **clean sections
  only** (wrong-fluid rejected), empty tanks stop treatment. Refill covers both tanks.
- `AHoldoverGameState`: replicated server-authoritative `WeatherSeverity` (auto-ramping in greybox).
- `AHoldoverAircraft`: deterministic `SimulateTreatment(dt, weather)` state machine —
  weather degrades protection, holdover starts only when all sections are clean+protected,
  burns faster with weather, expiry resets protection (retreatment), valid retreatment restarts
  the clock. `EHoldoverState` + `HoldoverRemaining` replicated.
- HUD: active fluid, both tanks, weather %, holdover state/timer colour-coded
  (green/yellow/red), wrong-fluid hints.
- Tests: `FluidRules` + `HoldoverStateMachine` + `SectionIndependenceAndClamp` — **all PASS** via
  MCP. Fixed a same-tick burn on holdover start (clock now starts at full duration).

## 2026-10-28 — Phase 3: spray & per-section treatment
- `AHoldoverSurfaceSection`: replicated `CleanlinessPercent`/`ProtectionPercent`, clean threshold,
  clamped `ApplyCleaning`/`ApplyProtection` (server-side).
- `AHoldoverAircraft`: gathers nearby sections (radius), exposes overall cleanliness / all-clean
  state. Test aircraft in `L_RampGreybox` (Nose/Fuselage/LeftWing/RightWing/Tail, labelled `AC_*`).
- `UHoldoverSprayComponent` on the character: hold **Left Mouse** to spray; owning client traces,
  `ServerSpray` validates range + fluid and applies Type I cleaning at a throttled rate; Type I
  tank with consumption + refill (E on the test cube).
- HUD: target section progress, aircraft-wide total, tank level, spray line placeholder.
- Automation test `Holdover.Treatment.SectionIndependenceAndClamp` — **PASS** via MCP
  (AutomationTestToolset). PIE: "Aircraft registered 5 surface sections" on server + client.
- Evidence: `Docs/evidence/phase3_aircraft.png`.
- Fixed: `AHoldoverAircraft` had no root component so it spawned at the origin (0 sections
  gathered); now has a scene root.

## 2026-10-28 — Phase 2: greybox ramp + interaction
- `L_RampGreybox` (`Content/Holdover/Maps/`): greybox apron built from engine basic shapes —
  deicing pad, aircraft-parking / ice-patch / jet-blast markers, refill station, light poles,
  safety cones, player start, lighting. Generated by `Tools/build_ramp_greybox.py`
  (idempotent headless Python; the MCP toolsets expose no actor-spawn tool).
- Interaction system: `IHoldoverInteractableInterface` (prompt + Interact) shared by all
  interactables; `AHoldoverTestInteractable` greybox test object; camera interaction trace on **E**
  with `ServerInteract` range validation; `AHoldoverHUD` reticle + prompt.
- Sprint on **Left Shift** (transient input actions + mapping context — UE 5.8 removed `BindKey`).
- Startup maps point at `L_RampGreybox`; evidence screenshot `Docs/evidence/phase2_ramp_pie.png`.
- Debugged a black-render root cause: `unreal.Rotator(*rot)` positional args are (roll, pitch, yaw),
  so the spawned sun pointed away from the scene (night sky + unlit geometry). Builder now maps
  rotation explicitly; see KNOWN_ISSUES.
- New: `Tools/build_ramp_greybox.py`, `Source/Holdover/HoldoverInteractableInterface.*`,
  `HoldoverTestInteractable.*`, `HoldoverHUD.*`.

## 2026-10-28 — Phase 1: network spike
- `AHoldoverCharacter`: explicit `bReplicates` + `SetReplicatingMovement(true)` (standard replicated
  character movement for listen-server co-op).
- PIE defaults set to **2-player listen server** (`Config/DefaultEditorPerProjectUserSettings.ini`).
- Verified via MCP + logs: "Server logged in" / "Client logged in" / "Welcomed by server",
  worlds `UEDPIE_0` (host) and `UEDPIE_1` (client) up for play. Movement-see-each-other remains a
  manual test (input injection not automatable yet).
- Added `Docs/TESTING.md`, `Docs/TEST_CHECKLIST.md`, `Docs/KNOWN_ISSUES.md`, `Docs/CHANGELOG.md`.

## 2026-10-28 — Phase 0: clean project (`00-clean-project`, commit af02efe)
- UE 5.8 C++ First Person scaffold as `Holdover`: module/targets/classes renamed from `TP_FirstPerson`
  with CoreRedirects keeping template Blueprints alive; Horror/Shooter variants pruned; shared packs
  (Characters/Input/LevelPrototyping) added.
- Unreal MCP (`ModelContextProtocol` + `ToolsetRegistry` + `AllToolsets`) enabled with auto-start on
  `http://127.0.0.1:8000/mcp`; verified end-to-end (toolsets, actor reads, PIE round-trip).
- Team README (setup, branch+PR workflow, MCP wiring); collaborators dandog25 & HreaperH78 invited.

## 2026-10-27 — Repo reset
- Unity 6 URP template purged (zero game content); UE `.gitignore`/`.gitattributes` (Git LFS) added.
- `Docs/` seeded with the UE 5.8 MCP build plan and master agent prompt (playbook).

## 2026-10-30 - Phase 7.5 step 2: client-truth pass (MP-1..MP-9)

Every fix verified from the CLIENT seat in PIE as client with net emulation (PktLag=120, PktLoss=5),
driven by the new `HoldoverEditor` MCP toolset (`Source/HoldoverEditor/` - InjectKey/InjectMouseDelta/
ExecConsole/TeleportPlayer/GetPawnInfo/ForceTreatment/GetSectionStates/GetTruckState).

- **MP-1 fixed** - active fluid is server-authoritative (`ServerSetFluid` RPC + replicated
  `ActiveFluidType`). Evidence: client Q-toggled to Type IV and Tail protection rose 0 -> 52
  (tanks drained 100->40.8/65.6 on the client's own component).
- **MP-2 fixed** - replicated `SpeedScale` state replaces raw MaxWalkSpeed writes; sprint and ice
  multiply. Evidence: 600 walk / 1050 sprint / 300 ice / 525 ice+sprint / 600 after the full
  enter-ice -> sprint -> leave-ice -> release sequence (the old permanent-2x bug is gone).
  Test `Holdover.Movement.SpeedScale`.
- **MP-3 fixed** - `AHoldoverPlayerController::PawnLeavingGame` saves a level-placed truck from
  connection cleanup (evict + unpossess); `GameMode::Logout` evicts as well. Evidence:
  `PawnLeavingGame: saving truck` -> truck alive `occupied=false` -> host enters
  (`entered by C_0`) after the client disconnected while inside.
- **MP-4 fixed** - truck spray origin set on every instance (constructor) so remote occupants'
  traces come from the nozzle. Evidence: client spray from the truck consumed the truck's tanks
  and hit sections.
- **MP-5 fixed** - `SetReplicatingMovement(true)` on the aircraft. Evidence: client observed
  `holdover=Departed` and `aircraftZ 0 -> 800` (replicated climb-out) after requesting clearance.
- **MP-6 fixed** - occupancy replicated (`OccupantCharacter`) + replicated `bVehicleHidden`
  collision state. Evidence: both instances report `occupied=true` after entry.
- **MP-7 fixed** - `ServerSetBoom` Unreliable + yaw clamp + server rate validation +
  `COND_SkipOwner`. Evidence: boom moved to `-120` (clamped at MaxAbsYaw) on both sides via the
  RPC path under net emulation, no disconnects.
- **MP-8 fixed** - `ClampSprayDt` (test `Holdover.Spray.DtClamp`) + origin-aware range check.
  Evidence: no false range rejections across the treatment runs; the clamp held under the
  3Hz-throttled secondary PIE world.
- **MP-9 fixed** - input contexts created/bound once, removed on unpossess (cached subsystem),
  truck priority 200 vs character 100. Evidence: repeated enter/exit cycles work from the client.
- Departure interact unblocked: `AHoldoverSurfaceSection` forwards interact to its nearest
  aircraft (the aircraft has no traceable collision - E-on-a-section is the departure path).
- **P7-2 evidence closed**: `spray hiss stop` fires on exit-while-spraying from the client seat
  (`TryExit` -> `ServerExit arrived: occupied=1` -> `exited back to`).
- Engine findings (documented): `AController::Possess` calls the NEW pawn's `UnPossessed` mid-entry
  (eviction must not live there); `AActor::Destroy` is non-virtual in 5.8 (use `PawnLeavingGame`).
- Verification: 9/9 automation tests PASS via `Tools/run_tests.ps1` (headless, PROC-4).

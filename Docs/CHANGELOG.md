# CHANGELOG

## 2026-11-02 - Sonnet review H4: shift ledger freezes at the verdict
- **H4 (verified real):** after `EndShift` froze `bShiftPassed`, the ledger kept
  moving — post-verdict departures registered as late (deductions),
  SimulateTreatment kept running (holdover aborts → RegisterPenalty), and every
  un-arrived aircraft "arrived" at EndShift (`Elapsed == duration`), firing
  dispatch events. The results banner could show net pay inconsistent with the
  PASSED/FAILED verdict.
- Fixes: `RegisterDeparture`/`RegisterPenalty` no-op unless `ShiftState ==
  InProgress` (covers hazard penalties and aborts too); `Aircraft::Tick` stops
  arrivals and treatment at the verdict (departure climb-out still runs);
  `Aircraft::Interact` refuses departures after the verdict.
- New `Holdover.MP.ShiftEndFreeze`: departs once legitimately, ends the shift,
  then attempts every post-verdict scoring path (direct register, interact,
  holdover abort, late arrival) and asserts the ledger is byte-identical.
  Suite **29/29 pass**.

## 2026-11-02 - Sonnet review H3/M1: spray RPC unreliable + spray rate limit
- **H3 (verified real):** `ServerSpray` was `UFUNCTION(Server, Reliable)` sent at
  10 Hz per spraying client — a reliable stream that backs up the reliable
  buffer under packet loss (hitches/disconnects). Now `Unreliable` like
  `ServerSetBoom`; the request is idempotent (dt clamped server-side), so a
  lost update costs nothing.
- **M1 (cheap, rides with H3):** `ServerApplySprayRequest` now runs the same
  token-bucket pattern as the boom — spray time accrues at 1x real time, banked
  up to 2x the request interval. dt was already clamped per request; this
  bounds REQUESTS per second, so a 1000 RPC/s flood can no longer apply ~200s
  of spray per real second. `NowSeconds` injectable for tests (boom pattern).
- `Holdover.MP.SprayValidation` extended: injected times + same-time flood
  (moves nothing) + idle-refill (one burst only). Suite **28/28 pass**.

## 2026-11-02 - Sonnet review H1/H2 + M17: client-truth fixes
- **H1 (verified real):** `ShiftNumber`, `TotalCompanyPay`,
  `TotalCompanyDeductions`, `CompanyLedger` were `UPROPERTY(ReplicatedUsing)`
  but had **no DOREPLIFETIME** — clients kept defaults (SHIFT 1 / ledger $0
  forever, wrong profile name, field-manual auto-open never triggering).
  Registered in `GetLifetimeReplicatedProps`; added to the replication contract.
- **H2 (verified real):** `AHoldoverAircraft::BeginPlay` wrote replicated
  `bArrived` and hid the actor with no authority guard — a client whose initial
  replication lands before BeginPlay stayed hidden all shift. Arrival init is
  now authority-only; clients derive visuals through the new public
  `SyncArrivalVisuals()` (also called from `OnRep_Arrived`).
- **M17 (cheap half):** `SyncArrivalVisuals` also drives the unattached surface
  sections' hidden/collision from `bArrived`/`bDeparted` (departed sections no
  longer stay visible and sprayable; the departed plane stays visible for its
  climb-out cue).
- **Test gap closed mechanically:** `Holdover.MP.ReplicationLint` reads each
  class's .cpp and asserts every contract property has a real
  `DOREPLIFETIME[_CONDITION](<Class>, <Prop>` line — this is the H1-class hole
  the CPF_Net-only contract test could not see. Plus
  `Holdover.MP.ArrivalVisuals` for the H2 client path.
- Tests: `run_tests.ps1` **28/28 pass**.

## 2026-11-02 - READY FOR ALI: consolidated NEEDS_ALI + status
- `Docs/AI/NEEDS_ALI.md` rewritten as ONE prioritised list: 9 items (packaged
  full shift, DES-6 gate, 3-4p MP, field-manual placeholders, legal review,
  atmosphere, snow Niagara, art assets, Steamworks) each with exact steps,
  estimated minutes and the AI's follow-up. ~5 h total; 1-5 are the
  demo-critical path.
- `Docs/AI/STATUS.md` now opens **READY FOR ALI** with a 10-line summary.
- Legal DRAFT label landed in the previous commit (test-enforced).

## 2026-11-02 - Phase 10: legal text labelled DRAFT (not final)
- `HoldoverInfoContent.cpp` `LegalText()` now leads with
  **"DRAFT - not legal advice, needs Ali/lawyer review"** — the legal block is a
  draft only and must never be presented as final until reviewed. The
  `Holdover.Info.CreditsLegalFeedback` test asserts the label stays present
  (same flip-after-review rule as the `[[TODO:ALI]]` placeholders).
- Tests: `run_tests.ps1` 26/26 pass. Repackaged + smoke: PASS (0 fatals,
  `L_RampGreybox` loads).

## 2026-11-02 - Phase 10: NEEDS_ALI #7 prep - field-manual values isolated
- `HoldoverInfoContent.cpp`: pulled the six values the AI must not invent into a
  single marked block at the top of the file (`GStudioName`, `GCodeCredit`,
  `GArtCredit`, `GCopyrightLine`, `GEulaRef`, `GFeedbackRoute`), each defaulting
  to `[[TODO:ALI]]`. Supplying NEEDS_ALI #7 is now a six-line edit with no other
  code touched, and the header comment documents flipping the
  `Holdover.Info.CreditsLegalFeedback` test afterwards.
- No behaviour change - all six still default to the placeholder marker, and the
  test that asserts the markers are present still passes.
- Tests: `run_tests.ps1` 26/26 pass.

## 2026-11-02 - Phase 10: PROC-6 .vscode template residue removed
- `.vscode/settings.json` and `launch.json` were Unity-era leftovers (PROC-6):
  `.unityproj`/`.prefab`/`.booproj`/`.pidb` file excludes, Unity file
  associations, and an `"Attach to Unity"` (`vstuc`) launch config that cannot
  work in a UE project. Replaced with UE-appropriate excludes
  (`Binaries/`/`Intermediate/`/`Saved/`), `.Build.cs`/`.Target.cs`/`.uproject`
  associations, and a build task for `Build.bat HoldoverEditor Win64 Development`.
- Tests: no C++ change; suite 26/26 (from the previous run).

## 2026-11-02 - Phase 10: field-manual evidence in the packaged build
- Added `UE_LOG` evidence lines so the D1-D3 manual is verifiable in a cooked
  build rather than assumed. Packaged smoke run now records
  `Field manual built: 8 tips + credits/legal/feedback`.
- **Finding (headless limitation, not a bug):** `UUserWidget::NativeTick` never
  runs under the `-nullrhi` smoke test — widgets tick only via `SObjectWidget`'s
  Slate paint pass, which headless has none of. Verified with a temporary
  diagnostic: `HUD NativeConstruct gs=ok shift=1 state=0` (shift still
  `WaitingToStart` when the HUD is built) and zero `NativeTick` lines.
  Consequence: the D1 auto-open cannot be observed headless. It is driven from
  both `NativeConstruct` (in case the shift is already running) and `NativeTick`
  (the normal case, where `StartShift` runs after the HUD is built), so it covers
  both orderings in a real windowed run. **Auto-open is unverified and needs the
  human windowed play-through (DEMO_CHECKLIST B7/D1); `F1` is the reliable
  access path.**
- Tests: `run_tests.ps1` 26/26 pass (C++ behaviour unchanged).

## 2026-11-02 - Phase 10: packaged-build audio fix + PROC-6 config hygiene
- **Placeholder audio missing from the packaged build** (found by the smoke test):
  `UHoldoverAudioLibrary::LoadSound` loads cues by name via a soft path, so the
  cooker saw no hard reference and dropped `radio_beep` / `wind_loop`. Added
  `+DirectoriesToAlwaysCook=(Path="/Game/Holdover/Audio")`. Verified: all 4 cues
  (`wind_loop`, `spray_hiss`, `radio_beep`, `collision_cue`) now in
  `Manifest_UFSFiles_Win64.txt`, and the packaged smoke run logs **0**
  `PLACEHOLDER AUDIO MISSING` warnings (previously 2) while the dispatcher still
  fires.
- **PROC-6 security**: `Config/DefaultEngine.ini` shipped an AndroidFileServer
  `SecurityToken` with `bAllowNetworkConnection=True` on a PC-only project — a
  usable network credential in source control. Plugin disabled, connection
  refused, token cleared.
- `ServerDefaultMap` pointed at `/Engine/Maps/Entry` (engine template map); now
  `L_RampGreybox` like the game default map.
- Repackaged and smoke-tested: `RESULT: PASS`, 0 errors / 3 warnings at cook,
  no fatals, `LoadMap: L_RampGreybox`.
- Tests: `run_tests.ps1` 26/26 pass (C++ unchanged).

## 2026-11-02 - Phase 10: D1-D3 field manual (tutorial / credits / legal / feedback)
- `HoldoverInfoContent.h/cpp` (new): all the text. `FirstShiftTips()` (8 tips
  covering clean/protect order, one-tank relay, holdover clock, hazards,
  dispatcher), `QuickTip()` (rotating HUD line), `CreditsText()`, `LegalText()`,
  `FeedbackText()`. Content is separate from UMG so it is testable without
  rendering and editable without touching widget code.
- `HoldoverInfoWidget.h/cpp` (new): scrollable C++-built UMG panel following the
  existing `RebuildWidget()` pattern in `HoldoverPanelWidgets.cpp`.
- Wired into `UHoldoverHUDWidget` as a fixed-size centred overlay (720x520) that
  auto-opens once on the first shift so a player with no manual sees the tutorial.
  `F1` toggles it via a transient input action (same MP-9-safe pattern as the
  existing sprint/interact/spray bindings).
- **Placeholders deliberately left in** (`[[TODO:ALI]]`): studio name, credits
  names, copyright holder, EULA link, feedback route. A test asserts they are
  still present so nothing ships with invented names or URLs - see
  `Docs/AI/NEEDS_ALI.md` #7.
- Tests: `Holdover.Info.TutorialTips` / `QuickTip` / `CreditsLegalFeedback`.
  Suite **26/26 PASS**.

## 2026-11-02 - Phase 10a: packaged Win64 build + demo checklist
- `Tools/package_win64.ps1` (new): scripted `RunUAT BuildCookRun`
  (-build -cook -stage -pak -archive) for Win64 Development, then a headless
  smoke test (launches `Builds\Windows\Holdover.exe` with `-nullrhi`, scans its
  log for fatals). Verified end-to-end: BUILD SUCCESSFUL → smoke PASS (game
  loads `L_RampGreybox`, comes up for play, no fatal errors). ~1 min incremental.
- `Config/DefaultGame.ini`: added `GameFeatureData` PrimaryAssetTypesToScan
  rule — without it the cook fails ("Asset manager settings do not include a
  rule for assets of type GameFeatureData").
- `Holdover.uproject`: editor-only plugins (ModelContextProtocol, ToolsetRegistry,
  AllToolsets, PythonScriptPlugin) restricted with `TargetAllowList: ["Editor"]`
  so they stay out of game builds (the cook still runs in editor context and
  loads them there).
- `Docs/DEMO_CHECKLIST.md` (new): release-candidate checklist — AUTO items
  (build/tests/package/smoke) are green; B/C sections are the human play-through
  (full shift, display/audio/quit/restart, host+client, 3–4p, net emulation,
  disconnect, late join); D covers pending text (tutorial/credits/legal/
  feedback) and the Ali-gated items; E lists remaining blockers.
- Known non-blocking cook warnings: stale `/Script/TP_FirstPerson` import in
  `BP_FirstPersonPlayerController.uasset` (template leftover) and the map's
  GameMode is `BP_FirstPersonGameMode_C` (verify it derives from
  `AHoldoverGameMode` at checklist B2). Editor suite still 23/23.

## 2026-11-02 - Phase 9 G-5: late-join shift state fully replicated
- `AHoldoverGameState`: `LastShiftDuration`, `PayPerOnTimeDeparture` and
  `LateDepartureDeduction` are now replicated. A late joiner previously read the
  local 120s default for elapsed-time math and C++-default pay rules for the HUD.
- ReplicationContract extended to cover the three properties. Suite **23/23 PASS**.
- **Phase 9 hardening complete**: audit gaps G-1..G-5 all fixed, each with
  automation coverage (`Docs/MP_AUDIT_2026-11-02.md` backlog closed).

## 2026-11-02 - Phase 9 G-3/G-4: server line-of-sight re-trace on spray + interact
- `ServerApplySprayRequest` (G-3) and `ServerInteract` (G-4) now re-trace
  `ECC_Visibility` from the validated origin to the target on the server and
  reject blocked paths. The client targeting trace uses the same channel, so any
  target the client can see still passes; a modified client can no longer
  spray/interact through walls within range.
- `AHoldoverCharacter::ValidateInteractTarget` (static) holds the range + LOS
  validation so tests can drive it without spawning the abstract character.
- New test `Holdover.MP.LineOfSight` (spray-through-wall rejected, clear path
  accepted, interact range + LOS). Suite **23/23 PASS**.

## 2026-11-02 - Phase 9 G-2: boom rate limit is per elapsed time, not per RPC
- `ApplyBoomRequest` (behind `ServerSetBoom`) now runs a server-side **token
  bucket**: the boom moves at most `YawSpeed`/`ExtensionSpeed` per second of
  elapsed server time, banking up to `BoomBurstSeconds` (0.25s) for packet
  bursts. The old per-RPC clamp allowed ~60x overspeed under a 60Hz request
  flood; teleports and floods now drain the bucket and stop.
- `ApplyBoomRequest` takes an injectable `NowSeconds` (default = world time) so
  the rate logic is deterministically testable.
- `Holdover.MP.BoomValidation` extended: burst cap, same-frame 100-request flood
  moves nothing, refill-then-reverse symmetric clamp. Suite **22/22 PASS**.

## 2026-11-02 - Phase 9 G-1: refill RPC bound to a real refill station
- `UHoldoverSprayComponent::ServerBeginRefill` was client-callable with no
  validation: any connection could start a timed refill anywhere, restart a
  refill mid-progress, or push a garbage fluid enum into replicated state.
- New `ValidateAndBeginRefill` (the RPC delegates to it): rejects invalid
  `EHoldoverFluidType` values, rejects while a refill is in progress (no timer
  refresh), and requires the owner to be within `RefillStationRange` (1000uu,
  same slack as `ServerInteract`) of an `AHoldoverTestInteractable` refill
  station. The legitimate station-interact path still passes unchanged.
- New test `Holdover.MP.RefillGuard` (4 cases). Suite **22/22 PASS**.
- Test-harness gotcha: a bare spawned `AActor` has no root component, so
  `GetActorLocation()` silently returns (0,0,0) — world-test owners now get a
  `USceneComponent` root.

## 2026-11-02 - Phase 9: PROC-1 world-based MP automation tests
- `Source/Holdover/Tests/HoldoverWorldTests.cpp` (new): five world-based MP tests
  against real spawned actors in a real `UWorld`:
  - `Holdover.MP.ReplicationContract` — every MP-1..MP-9 fix property is
    UPROPERTY(Replicated*) (reflection `CPF_Net` check on all 7 classes).
  - `Holdover.MP.SprayValidation` — ServerSpray handler rejects dt=1000 (clamped
    to 0.2s), rejects out-of-range targets, rejects Type IV on dirty sections.
  - `Holdover.MP.DepartOnce` — two clearance requests on one aircraft register
    exactly one departure and one payment (double-award guard).
  - `Holdover.MP.CoverageAuthority` — interact never writes treatment values;
    only the server spray path does (self-set-coverage impossibility).
  - `Holdover.MP.BoomValidation` — boom requests cannot teleport past
    YawSpeed/ExtensionSpeed or the absolute clamps.
- Test seams: `UHoldoverSprayComponent::ServerApplySprayRequest` and
  `AHoldoverDeicingTruck::ApplyBoomRequest` now hold the server-side validation
  bodies (the ServerSpray / ServerSetBoom RPCs delegate to them), so tests drive
  the exact handler a remote client hits.
- Suite: **21/21 tests pass** (`run_tests.ps1` RESULT: PASS, exit 0).
- UE 5.8 testing gotchas (hard-won):
  - `AHoldoverCharacter`/`AHoldoverGameMode` are `UCLASS(abstract)` — SpawnActor
    refuses them ("is abstract"); tests use CDOs (`StaticClass()->GetDefaultObject()`)
    and plain AActor spray owners.
  - `UWorld::GameState` is a private UPROPERTY — wired in tests via
    `FObjectPropertyBase::SetObjectPropertyValue_InContainer` reflection.
  - Enumerating `GetLifetimeReplicatedProps` from automation on `AHoldoverCharacter`
    hits `check(bIsPushBased == Other.bIsPushBased)` in `FLifetimeProperty::operator==`
    (CoreNet.h:342) and kills the process; the DOREPLIFETIME-parity half of the
    contract test was dropped (flag checks kept).
  - `IHoldoverInteractableInterface::Execute_Interact` does NOT reach the native
    `Interact_Implementation` in this automation context — call the `_Implementation`
    directly in tests. (`Execute_Interact` works in the real PIE game path.)

## 2026-11-02 - Phase 9: MP-1..MP-9 client-truth audit (docs only)
- `Docs/MP_AUDIT_2026-11-02.md` (new): source-verified audit that the MP-1..MP-9
  fixes landed, with file:line evidence per issue, plus the Phase 9 server-owns
  authority checklist (treatment / fluid / weather / timers / aircraft / shift /
  penalties / departures), double-clear + double-award + self-set-coverage
  verification, and late-join surface review.
- Verdict: all 9 fixes LANDED. Audit found 5 hardening gaps (G-1..G-5):
  refill RPC not station-bound, boom rate clamp per-RPC not per-time,
  no server LOS re-trace on spray/interact, late-join shift-duration + pay
  tuning not replicated. These become the Phase 9 hardening backlog.
- No C++ changed; tests 7/7 (16 cases) unchanged.

## 2026-11-02 - Phase 8a: snow-sky placeholder + Niagara snow blocker recorded
- `Tools/build_ramp_atmosphere.py`: added `ATMO_Cloud` (VolumetricCloud) as a heavy
  overcast snow-sky. This is the best scriptable "it is snowing hard" read; real
  *falling* snow is blocked (below).
- **Falling snow (Niagara) BLOCKED** - UE 5.8 Python cannot author a Niagara
  emitter here: `NiagaraEditorLibrary` is absent, `NiagaraSystem` exposes no
  emitter handles, and no snow emitter ships with the engine to copy. We can
  create an empty `NiagaraSystem` but nothing to put in it. Full diagnosis and
  unblock paths in `Docs/AI/BLOCKERS.md`; the ~10-minute in-editor step for Ali is
  `Docs/AI/NEEDS_ALI.md` #3 (author `NS_SnowFlurry`). Placeholder + NEEDS_ALI
  chosen over inventing a binary asset, per the build plan.
- `Tools/mapbuild_common.py`: `VolumetricCloud` moved out of the never-destroy
  system list so the atmosphere builder owns it like other atmosphere actors.
- Tests: `run_tests.ps1` 7/7 pass (C++ unchanged).

## 2026-11-02 - Phase 8a: wet/icy ground materials + painted taxi lines
- `Tools/build_ramp_surface.py` (new): generated materials + decals, no purchased
  textures. `M_ApronWet` (dark cold blue-grey, roughness 0.22 = wet apron) on the
  ramp floor; `M_Ice` (bright cold white-blue, roughness 0.06 = glassy) on the
  deicing pad and ice marker; `M_TaxiLine` (warm orange-yellow, faint emissive) as
  5 deferred decals (centerline, stop line, pad edges, pad stop). All tagged
  `builder:surface`; idempotent; READBACK verifies domain/blend/size/material.
- UE 5.8 API notes (via `Tools/probe_mat_api.py`, `probe_mat_enums.py`,
  `probe_decal_api.py`): material inputs need `connect_material_property()` with
  `MaterialProperty.MP_BASE_COLOR` etc. (NOT `connect_material_expressions()`);
  the decal domain is `MaterialDomain.MD_DEFERRED_DECAL` (not `MD_DECAL`);
  `DecalComponent` uses `set_decal_material()`/`decal_material` (there is no
  `set_material()` on it).
- Tests: `run_tests.ps1` 7/7 pass (C++ unchanged).

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

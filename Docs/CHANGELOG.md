# CHANGELOG

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

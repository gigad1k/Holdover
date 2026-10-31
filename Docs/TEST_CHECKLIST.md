# TEST CHECKLIST
_(Source: build plan §8. Re-run after significant changes. Mark results with date + build.)_

## Treatment rules — Type I + Type IV + holdover implemented (Phase 4)
- [x] Spraying section A does not change section B — automation test PASS (2026-10-28).
- [x] Type I increases cleanliness and consumes only Type I fluid — test PASS (2026-10-28).
- [x] Type IV cannot fully protect a dirty section — test PASS (2026-10-28).
- [x] Type IV increases protection and consumes only Type IV fluid — test PASS (2026-10-28).
- [x] Empty tank stops treatment — test PASS (2026-10-28); **manual**: HUD shows tank at 0 + hint.
- [x] Holdover does not start while required sections remain incomplete — test PASS (2026-10-28).
- [x] Worse weather changes the timer/degradation according to configured values — test PASS
      (weather burn multiplier + protection degradation verified deterministically).
- [x] Expired holdover blocks clearance and requires valid retreatment — test PASS (2026-10-28).
- [x] Server validates spray range and dt in `ServerSpray` — code since Phase 3; dt clamp +
      origin-aware range since MP-8 (2026-10-29). Automation covers the dt clamp only
      (`Holdover.Spray.DtClamp`); no test drives `ServerSpray` end-to-end (PROC-3 honesty).
- [ ] **Manual**: full loop in PIE — clean all 5 sections (Type I), switch to **Q** (Type IV),
      protect all 5, watch "HOLDOVER RUNNING" + timer on HUD; let weather degrade it; re-protect;
      let it expire (red) and confirm retreatment restarts the clock.
- [ ] A hit on scenery does not change aircraft values. _(trace only hits `AHoldoverSurfaceSection`;
      verify manually)_

## Shift and hazards — implemented (Phase 5)
- [x] On-time departure rewards match configured values — test PASS (2026-10-28).
- [x] Late/invalid departure does not award on-time credit — test PASS (late = deduction).
- [x] The same plane cannot depart twice or pay twice — test PASS (`bDeparted` gate).
- [ ] Penalties shown in results (jet blast penalty + late deduction) — logic exists but
      **NOT applied once**: no cooldown gate on jet-blast overlap, and no automation test touches
      `RegisterPenalty` (PROC-3 honesty). Needs a once/cooldown gate + a test.
- [x] Shift pass and fail paths both work — test PASS (quota met vs missed).
- [x] Restarting a shift resets the right values — test PASS + runtime (E on cube after end).
- [ ] **Manual**: full loop in PIE — treat the aircraft, depart with E, then let the shift timer
      expire; confirm RESTRUCTURED banner; press E on the cube; confirm counters reset.
- [ ] Ice and jet blast are obvious, fair and recoverable — **manual**: walk over the ice disc
      (slower movement, recovers on exit); enter the jet blast zone (pushed away + -$25).

## Multiplayer
- [x] Two instances connect via listen server (2026-10-28, `af02efe`+): PIE 2-player shows
      "Server logged in", "Client logged in", "Welcomed by server". See Docs/TESTING.md.
- [x] Owner-only input and camera — manual: each PIE window controls only its own character (2026-10-28).
- [ ] Replicated character movement — **manual test required**: move one player, confirm the other
      window shows the movement. (Automation cannot inject input yet; see KNOWN_ISSUES.)
- [ ] Server rejects out-of-range/invalid spray requests. _(range check in `ServerSpray` since
      Phase 3; manual fuzz pending)_
- [ ] All clients see the same surface values and aircraft state. _(replicated since Phase 3/4;
      manual 2p check pending)_
- [x] The same plane cannot depart twice or pay twice. _(Phase 5, test PASS)_
- [x] Weather and holdover are not independently advanced by every client. _(server-only ticks)_
- [ ] Host and client agree on final score and result. _(Phase 5; manual 2p check pending)_

## Packaged build — _not yet packaged (Phase 10)_
- [ ] Launches outside Unreal Editor.
- [ ] Correct default map and GameMode load.
- [ ] No missing asset references or blocking logs.
- [ ] Keyboard/mouse controls work, HUD remains legible at target resolutions.
- [ ] Full shift is playable without editor-only assets or MCP running.

## Project / toolchain (Phase 0)
- [x] `HoldoverEditor` Win64 Development builds clean (2026-10-28).
- [x] Unreal MCP smoke test: initialize → list_toolsets (20 toolsets) → GetVisibleActors →
      StartPIE/StopPIE round-trip (2026-10-28).
- [x] Template map `Lvl_FirstPerson` loads after module rename (CoreRedirects working).

## Greybox ramp + interaction (Phase 2)
- [x] `L_RampGreybox` builds/saves via `Tools/build_ramp_greybox.py` and is the startup map (2026-10-28).
- [x] PIE on `L_RampGreybox` runs with 2-player listen server; clean shutdown, no errors (2026-10-28).
- [x] Evidence screenshot captured (`Docs/evidence/phase2_ramp_pie.png`).
- [ ] **Manual**: walk the ramp (WASD, Left Shift sprint), verify scale/collision feels right,
      look at the test cube → prompt "Press E - Test Interact" appears → press E → cube flips
      green/orange and log line `TestInteractable ... interacted by` appears in Output Log.

## Truck & boom (Phase 6)
- [x] Boom rotation/extension clamped and applied — test PASS (2026-10-29).
- [x] Enter/exit occupancy is server-authoritative (possession) — build + tests PASS.
- [x] Spray from the nozzle goes through the shared spray rules (2026-10-29).
- [ ] **Manual**: E on the truck to enter ? A/D/W/S to aim the boom at a wing ? LMB sprays the
      section from the nozzle ? E exits back to the character. Confirm the HUD works in the truck.
- [ ] **Manual (two clients)**: boom movement visible on the second client.
- [ ] Driving (behind bDrivingEnabled): arrows move the truck — tested separately.

## HUD and feedback (Phase 7)
- [x] UMG HUD shows shift line, fluid/tanks, weather and target section — PIE capture PASS (2026-10-29).
- [x] Departure board shows aircraft holdover state, countdown and totals — capture PASS (2026-10-29).
- [x] Dispatcher feed shows the latest radio lines — visible in PIE capture (2026-10-29).
- [x] Placeholder audio assets import with loud missing-asset warnings — 4 cues via
      `Tools/build_placeholder_audio.py` (2026-10-29).
- [x] Radio beep (dispatch), spray hiss (spraying), wind loop (ambience), collision cue (hazards)
      wired in code (2026-10-29).
- [ ] **Manual listening test**: hear radio beep on dispatch, hiss while spraying, wind loop on
      spawn, collision cue on ice/jet blast.
- [ ] **Manual**: HUD legible in BOTH PIE windows (shift line + feed were occluded in the automated
      capture by the overlapping second window); empty-tank hint; collision penalty shown in
      pay/deductions.
- [x] P7-1 shift-results banner renders at shift end — PIE 2p capture PASS (2026-10-29,
      `Docs/evidence/phase75_p7_results_banner.png`, red "RESTRUCTURED" at expiry).
- [x] P7-3 urgency colour ramp logic (green running / yellow <30s / red expired) — automation
      PASS (`Holdover.UI.BoardStateColor`, 2026-10-29). **Manual**: see the colours live on the
      board during an active/expired holdover.
- [x] P7-4 audio loop seams pin to zero — generator self-check PASS (spray_hiss + wind_loop
      endpoints 0.000000, 2026-10-29). **Manual listening test**: confirm no seam click.
- [ ] **Manual (client seat)**: P7-2 spray-hiss lifecycle — enter truck, spray, exit; confirm no
      lingering hiss (start/stop is logged as `spray hiss start/stop`).
- [x] Full automation suite re-run after UMG/audio/P7 changes — all 7 tests PASS (2026-10-29).

## Client truth - multiplayer fixes (Phase 7.5 step 2)

All items below verified from the CLIENT seat (PIE as client, net emulation PktLag=120 PktLoss=5)
via the `HoldoverEditor` MCP toolset. See Docs/CHANGELOG.md for the per-issue evidence values.

- [x] MP-1 client Type IV spray raises protection - Tail 0 -> 52 with tank drain (2026-10-30).
- [x] MP-2 sprint/ice speed state replicates and never compounds - 600/1050/300/525/600 (2026-10-30).
- [x] MP-3 disconnect while in the truck leaves the truck usable - saved + host enters (2026-10-30).
- [x] MP-4 nozzle spray origin valid for remote occupants (2026-10-30).
- [x] MP-5 client sees the plane depart - Departed + aircraftZ 0 -> 800 replicated (2026-10-30).
- [x] MP-6 occupancy/collision state replicated to all clients (2026-10-30).
- [x] MP-7 boom end-to-end from the basket under unreliable RPCs - clamped -120 both sides (2026-10-30).
- [x] MP-8 ServerSpray dt clamp + origin-aware range - `Holdover.Spray.DtClamp` PASS (2026-10-30).
- [x] MP-9 input contexts do not accumulate across enter/exit cycles (2026-10-30).
- [x] P7-2 spray-hiss stops on exit-while-spraying (client seat, logged) (2026-10-30).
- [x] PROC-4 headless runner `Tools/run_tests.ps1` - 9/9 PASS exit 0 (2026-10-30).
- [ ] **Manual listening/feel pass**: boom movement feel, hiss/spatialisation (P7-5), 2-4 player
      chaos check - Phase 7.6 playtests own these.

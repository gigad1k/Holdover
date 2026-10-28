# TEST CHECKLIST
_(Source: build plan §8. Re-run after significant changes. Mark results with date + build.)_

## Treatment rules — _not yet implemented (Phase 3–4)_
- [ ] A hit on scenery does not change aircraft values.
- [ ] Spraying section A does not change section B.
- [ ] Type I increases cleanliness and consumes only Type I fluid.
- [ ] Type IV cannot fully protect a dirty section.
- [ ] Type IV increases protection and consumes only Type IV fluid.
- [ ] Empty tank stops treatment and shows a clear warning.
- [ ] Holdover does not start while required sections remain incomplete.
- [ ] Worse weather changes the timer/degradation according to configured values.
- [ ] Expired holdover blocks clearance and requires valid retreatment.
- [ ] A fully treated aircraft within holdover can be cleared exactly once.

## Shift and hazards — _not yet implemented (Phase 5)_
- [ ] On-time departure rewards match the configured values.
- [ ] Late/invalid departure does not award on-time credit.
- [ ] Penalties are applied once and shown in the results panel.
- [ ] Ice and jet blast are obvious, fair and recoverable.
- [ ] Shift pass and fail paths both work.
- [ ] Restarting a shift resets the right values and does not retain old timers or rewards.

## Multiplayer
- [x] Two instances connect via listen server (2026-10-28, `af02efe`+): PIE 2-player shows
      "Server logged in", "Client logged in", "Welcomed by server". See Docs/TESTING.md.
- [x] Owner-only input and camera — manual: each PIE window controls only its own character (2026-10-28).
- [x] Replicated character movement — **manual test required**: move one player, confirm the other
      window shows the movement. (Automation cannot inject input yet; see KNOWN_ISSUES.)
- [ ] Server rejects out-of-range/invalid spray requests. _(Phase 3)_
- [ ] All clients see the same surface values and aircraft state. _(Phase 3–4)_
- [ ] The same plane cannot depart twice or pay twice. _(Phase 5)_
- [ ] Weather and holdover are not independently advanced by every client. _(Phase 4)_
- [ ] Host and client agree on final score and result. _(Phase 5)_

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

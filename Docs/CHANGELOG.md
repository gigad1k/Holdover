# CHANGELOG

## 2026-10-29 — GI pass 1: night lighting rig + scalability tiers (UNVERIFIED — engine pass pending)
- `Tools/build_ramp_greybox.py` night grade: moonlight sun (intensity 1.2, cold blue-white, negative
  pitch kept), dim cold skylight (0.4, real-time capture), volumetric height fog (density 0.02, cold
  in-scatter), 4 warm sodium pole point lights (5000 cd, radius 2600) on the existing corner poles,
  new emissive material `M_PoleHeadGlow` (HDR warm constant → MP_EMISSIVE_COLOR, Lumen GI source)
  assigned to the pole heads, unbound `PPV_Night` (fixed exposure EV −1.5, 8500 K white temp).
  All risky property writes are try/except with `log_warning` (Python API names vary by engine
  build — a "… not set" warning means that name drifted in 5.8, fix in place).
- `Config/DefaultScalability.ini` (new): GI/reflection quality tiers Low→Ultra. **`r.Lumen.HardwareRayTracing=0`
  at every default tier** — ray tracing stays OFF by default for wide-audience system requirements;
  the Ultra tier (@3) is the single opt-in path for RT-capable GPUs (settings menu toggle comes later).
- **Status: authored on a machine WITHOUT UE 5.8 — NOT yet run.** Execute the headless rebuild and
  the new "Rendering" checklist in `Docs/TEST_CHECKLIST.md` on an engine machine before trusting it.
- Palette rule (locked in planning): orange = Type I, green = Type IV (fluids only); red = holdover
  clock only; everything else cold monochrome. Warm lamp light is lighting, not UI — unaffected.

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

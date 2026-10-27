# HOLDOVER — Unreal Engine 5.8 + MCP Build Plan

**Edition:** No AngelScript · Native C++ + Blueprints · AI-assisted editor workflow  
**Plan date:** 27 October 2026  
**Target:** A polished playable Windows vertical slice, then a 2–4 player co-op demo suitable for Steam playtesting.

---

## 1. The decision

We are rebuilding HOLDOVER as a normal Unreal Engine 5.8 project. **Do not use AngelScript, the Hazelight fork, or the previous `.as` kit.** Keep the old kit archived for reference only; do not copy its source into the new project.

Use three tools for three different jobs:

- **C++:** gameplay rules that must be reliable, testable and authoritative in multiplayer.
- **Blueprints:** child classes, asset assignment, animation, effects, level assembly and simple presentation wiring.
- **MCP:** lets an AI coding agent inspect and operate the running Unreal Editor—placing actors, configuring assets, building supported graphs, playing tests and reporting results. MCP is an editor-control channel, not the game's runtime scripting language.

Python may be used for **editor automation only** if the MCP workflow needs it. It will not replace C++/Blueprint gameplay code.

### Why this approach

Unreal Engine 5.8 is released and includes Epic's experimental, first-party Unreal MCP plugin. Epic's documentation says it can connect an MCP-compatible agent to the editor, but also warns that the feature is experimental and incomplete in places. We'll use it, test each capability early, and never assume an operation succeeded without checking the result in the editor or build log.

### Game premise

You and your friends—Dan, Michael and Ali—are dangerously underqualified ground crew at a freezing, behind-schedule airport. Deice aircraft before their holdover clock expires. The weather fights back, the equipment is battered, and the crew are often the biggest hazard.

**Store hook:** “Up to 4 players. Zero qualifications.”

**Style:** chunky, cartoonish, cold and grimy; snow and icy tarmac under orange ramp lights at 3 a.m. The comedy comes from workplace chaos, the dispatcher, equipment mishaps and narrowly missed departures—not from punishing the player.

---

## 2. Scope: what “complete” means

The first goal is not a giant airport or a content catalogue. The first goal is one complete, repeatable shift that is fun enough to replay.

### Vertical-slice acceptance criteria

A player can start a shift, enter the ramp, treat an aircraft section-by-section, choose the right fluid, manage finite tanks, cope with worsening weather and hazards, clear the aircraft while it is still protected, earn or lose money, and see a shift result. The game has sound, readable HUD, clear feedback, a start/end flow, and can be packaged as a Windows build.

### Co-op acceptance criteria

Two to four players can join the same session. The host/server owns aircraft condition, fluid treatment outcomes, weather, holdover timers, shift rules, departures and shared vehicle state. All players see the same treatment progress and results. Each client controls only its own character and camera.

### Release acceptance criteria

The build has a stable start-to-finish loop, no known blocking crashes, understandable controls, a tested graphics/performance preset, Steam friend joining/invites verified on target hardware, a store page and a short gameplay trailer. More aircraft and maps come after the first ramp is fun.

**Reality check:** a 30-day sprint is a target for a strong vertical slice, not a promise that a commercial co-op game will be launch-ready. Multiplayer stability, art production and Steam integration may require a further 2–4 weeks or more. Never label a feature complete simply because code was generated.

---

## 3. Technical design

### Engine and project

- Unreal Engine **5.8**, installed from the Epic Games Launcher.
- Windows 11, Win64 target.
- Start from the **C++ First Person** template; use its working input/camera as the starting point, then replace template logic deliberately.
- Visual Studio 2022 with the **Game development with C++** workload and a supported Windows SDK.
- Git repository before substantial project changes. Use GitHub Desktop if preferred. Commit before each milestone and before broad MCP edits.
- Use Enhanced Input for player and vehicle controls.

### Gameplay architecture

| Class / asset | Responsibility | Authority / notes |
|---|---|---|
| `AHoldoverGameMode` | Start/end a shift, spawn/queue aircraft, enforce quota and end conditions | Server only |
| `AHoldoverGameState` | Replicated weather severity, shift time, shift number, global status and shared messages | Server writes; all clients read |
| `AHoldoverPlayerState` | Player pay, penalties, damage count, contribution and shift stats | Replicated per player |
| `AHoldoverCharacter` | First-person camera, movement, interaction and input | Own client sends input; server validates gameplay requests |
| `UHoldoverSprayComponent` | Spray trace, active fluid, rate/resource checks and spray effects | Server validates hits and changes gameplay values |
| `AHoldoverAircraft` | Aircraft queue/state, required sections, treatment validity and departure state | Server authoritative; replicated to clients |
| `AHoldoverSurfaceSection` | One independently trackable surface panel: cleanliness, ice contamination, Type IV protection | Replicated; simple and easy to debug |
| `AHoldoverDeicingTruckPawn` | Arcade driving, boom rotation/extension, basket station and vehicle occupancy | Server-owned movement/state where applicable |
| `AHoldoverHazardVolume` | Ice and jet-blast trigger behaviour | Server validates force/state; clients display effects |
| `AHoldoverShiftData` | Tunable shift quota, timer, initial weather, aircraft mix and starting resources | Data asset / configuration |
| `UHoldoverFluidData` | Type I and Type IV rates, consumption and treatment properties | Data asset; no hard-coded scattered constants |
| `WBP_HoldoverHUD` | Reticle, active fluid, tank levels, interaction hints, weather and clock | UMG presentation |
| `WBP_DepartureBoard` | Aircraft status, holdover and queue | UMG presentation |
| `WBP_ShiftResults` | Quota result, money, deductions and next-shift button | UMG presentation |
| `WBP_DispatcherFeed` | Radio-style messages and dispatch audio cues | UMG presentation |

**Rule:** core gameplay state changes live in C++; Blueprint children supply meshes, materials, sounds, animation assets and exposed tuning values. Do not create a single enormous Blueprint graph. Keep classes small and responsibilities separate.

### Recommended content structure

```text
Source/Holdover/
  Characters/
  Aircraft/
  Vehicles/
  Gameplay/
  Hazards/
  UI/
  Tests/
Content/Holdover/
  Maps/
  Blueprints/Characters/
  Blueprints/Aircraft/
  Blueprints/Vehicles/
  Blueprints/Hazards/
  UI/
  Data/
  Materials/
  Meshes/
  Animations/
  Audio/
  FX/
  Decals/
Docs/
  BUILD_PLAN.md
  TEST_CHECKLIST.md
  MCP_NOTES.md
```

---

## 4. MCP setup on UE 5.8

### Primary choice: Epic's built-in Unreal MCP

Use the official plugin first. It avoids committing to a community bridge before we've tested the editor tools shipped with the engine.

1. Create and open the `Holdover` C++ project in Unreal Engine 5.8.
2. Open **Edit → Plugins**.
3. Enable **Unreal MCP** (the plugin identifier is `ModelContextProtocol`) and **All Toolsets**. The Toolset Registry dependency should enable automatically.
4. Restart the editor when prompted.
5. Open **Edit → Editor Preferences → General → Model Context Protocol**.
6. Enable **Auto Start Server**. The default local endpoint is `http://127.0.0.1:8000/mcp`.
7. Keep it bound to the local machine. The official server has no authentication layer and must not be exposed to the internet or a public network.
8. Keep Unreal Editor open, then open the AI coding agent from the project root.

### If using OpenCode

Run this from the `Holdover` project directory after Unreal Editor is open:

```powershell
opencode mcp add unreal --url http://127.0.0.1:8000/mcp
opencode mcp list
```

If the installed OpenCode version prompts for authentication or reports that the endpoint cannot be reached, check the editor Output Log, confirm Auto Start Server is enabled, and check that the URL is exactly the local UE 5.8 endpoint. OpenCode versions can differ in their config format, so prefer the CLI command above to hand-editing a config file.

### If using Claude Code instead

In the Unreal Editor console, run:

```text
ModelContextProtocol.GenerateClientConfig ClaudeCode
```

This generates the client configuration at the project/workspace root. Start Claude Code from that same root so it can find the configuration.

### First MCP smoke test

Ask the agent to do only the following, one action at a time:

> 1. Confirm the active project and engine version. 2. List the tools/toolsets available. 3. Inspect the current level and selected actors. 4. Add three labelled test cubes and one light to an empty test level. 5. Save the level, enter Play-In-Editor, capture or describe the result, and stop PIE. 6. Remove the test actors and confirm the level is clean.

**Pass condition:** the agent can inspect, change, save, run and verify a harmless test without corrupting the project. Do not start game production until this passes.

### MCP operating rules

- Do not issue overlapping editor mutations. Epic's MCP dispatches tool calls serially on the game thread.
- Save before and after batches of edits. Use Git commits as the durable undo point; Ctrl+Z is not a replacement for source control.
- Ask the agent to inspect the available tool schema before attempting an operation. Never invent a tool name.
- Every milestone must include evidence: build result, output-log errors, test steps, and a PIE check when relevant.
- If a task is not supported by the official toolset, let the agent edit source files using the normal filesystem/IDE workflow and compile with Unreal Build Tool. Use MCP for editor actions that it actually exposes.

### Optional fallback, only if a real blocker appears

Do not install multiple editor MCP bridges on day one. If Epic's MCP cannot perform a specific required task, record the missing operation first. Then evaluate one alternative:

- **Funplay MCP for Unreal:** community Python-based editor MCP advertising UE 5.3–5.8 support, Blueprint/editor actions, PIE and screenshot automation. It uses a per-project token and includes execution safety checks. Consider it if the main need is editor automation and visual test capture.
- **qiqt/unreal-mcp:** community bridge focused on Blueprint graph inspection/editing, with public status notes claiming UE 5.8 verification. Consider it if the blocker is reading or wiring large Blueprint graphs. It involves an additional plugin/server and should be tested on a throwaway project first.

Both are third-party projects, not Epic guarantees. Choose **one** fallback, test on a disposable map and confirm compatibility with the exact installed UE 5.8 build before using it on the main project.

---

## 5. Core gameplay rules

### The treatment loop

1. The aircraft arrives with ice/contamination on its required sections.
2. The player selects **Type I** and sprays each surface until it is clean. Type I is the faster cleaning/deicing fluid; it does not provide the Type IV protective holdover.
3. Once a surface is clean enough, the player selects **Type IV** and applies protective coverage.
4. Once every required section is clean and protected, the aircraft's holdover countdown begins.
5. Weather severity changes how quickly protection degrades and how quickly the countdown burns.
6. The player requests clearance. The server accepts only if all required sections remain within valid treatment state and the holdover hasn't expired.
7. A valid departure earns money and on-time credit. Late departures, collisions, wasted fluid and equipment damage cost money.
8. When the shift timer expires, the quota is evaluated and the shift-result panel appears.

### Important implementation detail

Do not represent everything with one vague percentage. Track at least:

- `CleanlinessPercent` (0–100) per surface section.
- `ProtectionPercent` (0–100) per surface section.
- `IceContamination` or `IceRemaining` per section.
- `ActiveFluidType` and remaining fluid quantity for each tank.
- A plane-level treatment state and holdover timer.

The exact formulas must be exposed as tunable values and playtested. Keep the rules deterministic on the server. The HUD shows section progress, overall cleanliness/protection, fluid, weather severity and holdover time.

### Fluid rules

- **Type I:** high cleaning rate; consumes tank fluid; no long-term protection.
- **Type IV:** applies only to sufficiently clean sections; creates the protective layer; consumes the Type IV tank.
- Starting a new treatment, changing fluid, running out of fluid and returning to a refill station must produce clear feedback.
- Make every rate, capacity and threshold adjustable through Data Assets or class defaults.

### Hazard rules

- Ice patches reduce traction and may cause a brief, funny stumble; they should not randomly lock the player out of control.
- Jet blast pushes players/loose equipment with strong warning audio/visual cues.
- Collisions with aircraft, jet bridges or expensive gear create a penalty and dispatcher message.
- Ragdoll/recovery is a later polish task. Do not let ragdoll block the core loop or replicate incorrectly.

### Shift rules

For the first test shift, use deliberately small values: one ramp, two aircraft, an eight-minute shift and a quota of two on-time departures. Keep these in a Data Asset so the difficulty can be tuned without recompiling.

---

## 6. Build phases and prompts

Work phase-by-phase. The agent must not try to generate the whole game in one enormous operation.

### Phase 0 — clean start and source control

**Tasks**
- Install UE 5.8 and the C++ toolchain.
- Create the C++ First Person project named `Holdover`.
- Initialise Git and make `00-clean-project` commit.
- Enable Enhanced Input and the official Unreal MCP; connect the AI client.
- Add the folder layout and this plan under `Docs/`.

**Agent prompt**

> Inspect this clean Unreal Engine 5.8 C++ First Person project. Do not use AngelScript or add AngelScript plugins. Set up the Holdover folder structure, create a starter map named `L_RampGreybox`, preserve the template's working character/input until replaced deliberately, enable the official Unreal MCP toolsets, and confirm the project builds. Use MCP only for supported editor operations and normal C++ files for gameplay code. Save everything and report exact files changed, build command/result, errors and next blocker. Do not claim the build succeeded unless the build output proves it.

**Done when:** clean project compiles; MCP test passes; Git is committed.

### Phase 1 — network spike before content

**Tasks**
- Confirm two local game instances can connect using a listen server.
- Confirm each player has a separate character, camera and input.
- Verify the character's replicated movement before implementing the deicing systems.
- Document how to launch host and client for every later test.

**Agent prompt**

> Prove basic Unreal networking in Holdover before building gameplay. Configure the project for a listen-server test and ensure the First Person character uses Unreal's standard replicated character movement. Each client must control only its own pawn and camera. Give me exact steps to start one host and one client locally. Run the test if MCP/automation can do so; otherwise provide the exact manual test and report what cannot be automated. Commit only after the build succeeds.

**Done when:** two instances connect and each sees the other's movement.

### Phase 2 — player interaction and greybox ramp

**Tasks**
- WASD, mouse look, sprint, interaction trace and reticle.
- A simple apron made from geometry: marked deicing pad, aircraft parking position, light poles, safety cones and refill station.
- Interactable objects use one interface/component pattern rather than custom one-off logic.
- Add labels/debug overlays so AI-generated test objects are identifiable.

**Agent prompt**

> Build the greybox ramp from simple meshes only. Add clear locations for one aircraft, one refill point, one ice patch and one jet-blast volume. Implement an interaction trace and a small on-screen prompt. Keep the environment bounded and easy to test. Use consistent actor labels and save `L_RampGreybox`. Enter PIE, verify scale and collision, then capture a screenshot. Do not import final art yet.

**Done when:** the character can move around a readable ramp and interact with a test object.

### Phase 3 — spray and per-section coverage

**Tasks**
- Build a test aircraft from separate, named surface sections.
- Add a spray trace from the camera/nozzle.
- Add Type I cleaning progression and per-section UI feedback.
- Add overall aircraft treatment totals and a simple spray effect placeholder.
- Server validates spray hit, range, fluid availability and active fluid before state changes.

**Agent prompt**

> Implement the aircraft treatment prototype in native C++ with small Blueprint children only for presentation. Each aircraft surface section must expose replicated cleanliness and protection values. Holding the spray input sends a validated server request; only a valid hit on a registered aircraft section changes coverage. Type I increases cleanliness. Show current section progress and the aircraft-wide total. Create a test aircraft from a few labelled primitive sections, wire the references and prove that spraying one section does not change another. Add automated/unit tests where practical and a manual PIE checklist.

**Done when:** all sections can be cleaned independently and the percentage shown on screen agrees with server state.

### Phase 4 — Type IV, tanks, weather and holdover

**Tasks**
- Type I and Type IV selection.
- Separate fluid tank levels and consumption.
- Type IV protection only applies to clean sections.
- A plane-level holdover timer starts when all required surfaces meet the treatment rules.
- Weather severity increases degradation / timer burn and can force retreatment.
- HUD warning states and timer colour/text changes.

**Agent prompt**

> Add the two-fluid treatment and holdover rules. Type I cleans; Type IV protects clean sections. Track cleanliness and protection separately. Start holdover only after every required section passes the configured treatment thresholds. Weather severity must be replicated and server-authoritative. Expose cleaning rates, application rates, tank capacities, consumption, degradation, holdover duration and valid-clearance thresholds in Data Assets or editable defaults. Add tests for incomplete coverage, wrong fluid, empty tanks, holdover expiry, worsening weather and valid re-treatment. Use deterministic server-side rules; never trust a client to write the final values.

**Done when:** a full aircraft can be cleaned, protected, degraded by weather, retreated and either cleared or rejected for valid reasons.

### Phase 5 — hazards, scoring and shift loop

**Tasks**
- Ice traction reduction, jet blast, collision/damage penalties.
- Clearance interaction and taxi/departure state.
- Shift timer, quota, on-time departures, money, deductions and results panel.
- Dispatcher feed with event-based, short lines.
- The shift can restart without restarting the game.

**Agent prompt**

> Complete one full shift loop. Aircraft may depart only when the server confirms all treatment conditions and holdover are valid. Award on-time points/pay on valid departures; penalise late departures, collisions and avoidable fluid waste. At shift end, compare completed on-time departures against the quota and display Shift Passed or Restructured, with a breakdown of pay and deductions. Add ice and jet-blast hazards with readable cues and fair recovery. Add a dispatcher message for each meaningful event. Make the shift repeatable without a full application restart and test both a pass and a fail outcome.

**Done when:** a player can start, complete/fail and restart a shift.

### Phase 6 — truck and boom

Build this after the on-foot loop is fun.

**Tasks**
- Parked truck first; boom rotates and extends; basket/nozzle points predictably.
- Enter/exit interaction and occupancy state.
- Arcade vehicle movement only after the stationary boom works.
- If driving and basket use destabilise the schedule, keep the truck parked for the first demo and add driving in the next milestone.

**Agent prompt**

> Create a deicing truck Blueprint child with assigned placeholder meshes, a rotating boom, an extendable arm and a basket/nozzle. First prove boom and spray aiming while stationary. Then add an arcade vehicle pawn with brake/reverse/steering and a clear enter/exit interaction. Make the server authoritative for vehicle occupancy and shared movement state. Replicate the boom, basket and spray presentation. Use placeholders and test in two clients before importing detailed truck art. Keep the implementation modular so a drivable truck can be disabled by configuration if it blocks the demo.

**Done when:** a player can operate the boom and spray a surface from a useful angle; driving is tested separately.

### Phase 7 — HUD and feedback

**Tasks**
- `WBP_HoldoverHUD`, departure board, dispatcher feed and shift results.
- Tank state, active fluid, weather, section percentages and holdover countdown.
- Radio beep, spray hiss, wind loop and collision cue.
- Controller support can follow keyboard/mouse if needed for the initial demo.

**Agent prompt**

> Build a readable UMG HUD using the existing C++ state as the source of truth. Include fluid selection and levels, target section progress, total aircraft status, holdover time, weather severity, shift time, quota, money and dispatcher messages. Avoid duplicate local gameplay state in widgets. Keep the cold-night background legible. Add temporary audio assets or silent placeholders with obvious log warnings where assets are missing. Verify the HUD with normal play, empty tanks, expired holdover, a collision penalty and shift end.

**Done when:** players can understand what to do and why a departure can or cannot be cleared without reading developer logs.

### Phase 8 — atmosphere and art pass

**Tasks**
- Replace the greybox floor and buildings with modular airport assets.
- Snowfall using Niagara, fog, cool ambience, warm orange ramp lights, wet/icy ground materials and taxi-line decals.
- Import one character and one aircraft first. Add animations only once the core actions are stable.
- Add audio and comedy polish after readability and performance are good.

**Agent prompt**

> Upgrade `L_RampGreybox` into a compact snowy-night airport ramp while preserving scale, collision, treatment targets and test routes. Use a cold blue ambient grade, warm orange ramp lighting, controlled fog and a Niagara snow system. Keep performance scalable and provide Medium/High presets. Replace one asset category at a time and re-run the full treatment shift after each swap. Do not make decorative objects block player or vehicle routes. If an asset is not available locally, use a placeholder and record the required asset rather than silently inventing an import path.

**Done when:** the ramp has a distinctive look and remains readable and playable.

### Phase 9 — multiplayer integration and hardening

**Tasks**
- Replicate surface values, fluid consumption, weather, timers, aircraft states, score, penalties and departure events.
- Use server RPCs for requests; reliable server validation for important state transitions; cosmetic effects can use appropriate multicast/replicated events.
- Validate that two clients cannot independently clear the same plane, double-award pay or set their own coverage.
- Test late join/reconnect behaviour only if supported by the session design.

**Agent prompt**

> Audit all replicated gameplay in Holdover. The server owns treatment values, fluid consumption, weather, holdover timers, aircraft state, shift rules, damage penalties and departures. Clients request actions; they never write authoritative values. Ensure only the owning client reads local input/camera. Prevent duplicate departure rewards and validate distance, line of sight, fluid and treatment state on the server. Run a two-client test through one complete shift and record any sync issues. Do not call multiplayer done until the host and client agree on section coverage, holdover, fluid, scoring and shift result.

**Done when:** a host and client complete one shift with matching results; then test three or four players.

### Phase 10 — final demo, packaging and Steam preparation

**Tasks**
- Fix crashes and blocker bugs before adding content.
- Build and run a packaged Windows build—not just PIE.
- Test fresh launch, display settings, audio, quit/restart, one whole shift, host/client, and a low-quality graphics preset.
- Add a short tutorial/first-shift tips, credits, legal notices and feedback route.
- Capture genuine gameplay for a trailer and GIFs; create the Steam store page only when the build represents the actual game.
- Add Steam session/friend invite support after the local listen-server flow passes. Verify the chosen Unreal 5.8 online plugin/transport against the exact build before relying on it.

**Agent prompt**

> Prepare a release-candidate checklist for Holdover. Produce a packaged Win64 development build, launch it outside the editor and run the complete shift. Check logs for errors, verify input/UI scaling, test the current multiplayer flow, and record reproducible bugs. Do not hide errors, disable warnings globally, or mark unsupported features complete. Prioritise crashes, broken treatment rules, network desync, blocked level routes and unclear HUD over cosmetic additions. Generate a concise changelog and an exact list of remaining blockers.

**Done when:** a packaged build completes the loop and the remaining issues are understood and prioritised.

---

## 7. Suggested 30-day sprint

This sprint targets a compelling vertical slice. It is not permission to skip testing.

| Days | Focus | Required output |
|---|---|---|
| 1–2 | Project, C++ toolchain, Git, MCP smoke test | Clean compiling project; MCP verified |
| 3–4 | Listen-server network spike | Two local players see each other moving |
| 5–7 | Player controls, ramp greybox, interactables | Walkable test ramp |
| 8–10 | Aircraft sections and Type I spray | Independent surface progress |
| 11–13 | Type IV, fluid tanks and holdover | Valid/invalid treatment rules |
| 14–16 | Weather, hazards and penalties | Weather changes the treatment decision; hazards work |
| 17–19 | Departure, shift quota and results | Full solo shift loop |
| 20–22 | HUD, dispatcher and audio | Readable and funny playable loop |
| 23–25 | Truck boom and art/lighting pass | A useful boom interaction and a convincing ramp |
| 26–27 | Multiplayer treatment synchronisation | Two clients complete one shift, or a specific blocker list exists |
| 28–29 | Bug fixing, packaged build and stranger playtest | Build and feedback notes |
| 30 | Demo freeze | Tagged version, known issues, next sprint backlog |

If behind schedule, cut in this order: advanced ragdolls, drivable truck, more aircraft types, additional gates, extra weather profiles, custom animations. **Do not cut treatment rules, clearance validation, shift results or packaged-build testing.** If co-op is still unstable, ship the internal demo as single-player while keeping multiplayer clearly marked experimental; don't advertise co-op as ready.

### Post-sprint: demo-to-release

1. **Stability pass:** fix the top playtest problems, improve onboarding and save settings.
2. **Co-op pass:** complete 2–4 player tests, host migration/reconnect decisions, invite flow, network-loss handling and reward duplication tests.
3. **Content pass:** add a second aircraft configuration, a second shift profile and a small number of meaningful hazards only if replay tests show the core loop is solid.
4. **Steam pass:** verify the chosen online/session integration on packaged builds; finish store assets, trailer, demo branch, support links and release checklist.
5. **Launch pass:** test public demo, collect crash/feedback reports, patch the highest-impact issues first.

---

## 8. Test checklist (must be repeated after significant changes)

### Treatment rules

- A hit on scenery does not change aircraft values.
- Spraying section A does not change section B.
- Type I increases cleanliness and consumes only Type I fluid.
- Type IV cannot fully protect a dirty section.
- Type IV increases protection and consumes only Type IV fluid.
- Empty tank stops treatment and shows a clear warning.
- Holdover does not start while required sections remain incomplete.
- Worse weather changes the timer/degradation according to configured values.
- Expired holdover blocks clearance and requires valid retreatment.
- A fully treated aircraft within holdover can be cleared exactly once.

### Shift and hazards

- On-time departure rewards match the configured values.
- Late/invalid departure does not award on-time credit.
- Penalties are applied once and shown in the results panel.
- Ice and jet blast are obvious, fair and recoverable.
- Shift pass and fail paths both work.
- Restarting a shift resets the right values and does not retain old timers or rewards.

### Multiplayer

- Owner-only input and camera.
- Server rejects out-of-range/invalid spray requests.
- All clients see the same surface values and aircraft state.
- The same plane cannot depart twice or pay twice.
- Weather and holdover are not independently advanced by every client.
- Host and client agree on final score and result.

### Packaged build

- Launches outside Unreal Editor.
- Correct default map and GameMode load.
- No missing asset references or blocking logs.
- Keyboard/mouse controls work, HUD remains legible at target resolutions.
- Full shift is playable without editor-only assets or MCP running.

---

## 9. Visual asset briefs

Keep one consistent chunky, cartoonish, cold-and-grimy style.

### Ground-crew character

> Stylised chunky airport ground-crew worker in a bulky hi-vis winter jacket, insulated gloves, ear defenders and a hard hat; exaggerated readable proportions, cold-weather equipment, game-ready topology, clean silhouette, slightly battered workwear, non-photorealistic materials, suitable for a comedic co-op game.

### Deicing truck

> Stylised airport deicing truck with tall articulated boom and operator basket, believable proportions but slightly battered, chunky shapes and simple readable controls; game-ready hard-surface asset, separate wheel/boom/basket parts for animation and interaction, cold-weather grime and reflective hi-vis details.

### Aircraft and ramp

> Stylised passenger aircraft with clean broad surface sections, readable wings/tail/nose and simple engine shapes; modular airport ramp, taxi markings, gate stand, cones, chocks, fluid barrels and weathered service equipment. No real airline logos unless properly licensed.

### Audio list

- High-pressure deicing spray hiss.
- Wind across an exposed snowy apron.
- Radio beep and slightly crackly dispatch line.
- Boom/metal clang, equipment scrape, ice slip, jet-blast warning.
- Short, tense-but-goofy loopable music bed.

Use generated or sourced assets only where their commercial licence permits the intended game use. Retain source/licence notes.

---

## 10. Agent working agreement (paste into the project's agent instructions)

1. Never use AngelScript or install AngelScript plugins.
2. Use native Unreal C++ for authoritative gameplay and Blueprints for assembly/presentation.
3. Read the project before editing; inspect available MCP tool schemas rather than guessing.
4. Make one focused change at a time. Do not replace the whole project or make a broad rewrite without a committed checkpoint.
5. For code changes, compile with Unreal Build Tool/Visual Studio and report the actual result. For editor changes, save, inspect the result, and test in PIE when relevant.
6. If compilation fails, fix the first root error and rebuild before adding features. Do not bury the error with unrelated changes.
7. Never claim a test passed unless it was run or clearly label it as “not run/manual test required.”
8. Use placeholders when artwork is missing; never invent file paths or claim imported assets exist.
9. Keep core values server-authoritative and replicated. Clients send requests; servers validate them.
10. Do not add online services, paid assets or paid plugins without explicit approval. Prefer free, already-installed tools.
11. Keep a `Docs/TEST_CHECKLIST.md`, `Docs/KNOWN_ISSUES.md` and `Docs/CHANGELOG.md` up to date.
12. At the end of every phase report: files/assets changed, build status, tests run, remaining failures, and the exact next phase.
13. If blocked, explain the specific blocker and perform other safe, independent work rather than pretending to have finished it.

---

## 11. Reference links checked for this plan

- Epic announcement that Unreal Engine 5.8 is available: https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available
- Epic Unreal MCP UE 5.8 documentation (setup, toolsets, config and limitations): https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor
- Epic UE 5.8 release notes: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes
- Epic Enhanced Input documentation: https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine
- OpenCode MCP server documentation: https://docs.opencode.ai/docs/mcp-servers/
- Optional community editor automation MCP: https://github.com/FunplayAI/funplay-unreal-mcp
- Optional community Blueprint graph MCP: https://github.com/qiqt/unreal-mcp

**Source baseline:** HOLDOVER — The Full Build Playbook · AI-First Edition, supplied by the user. This plan preserves its deicing/holdover loop, comedy, snowy ramp, hazards, shift quotas, 2–4 player co-op direction and Steam goal, while replacing Unity/Mirror/AngelScript-specific assumptions with an Unreal 5.8 C++/Blueprint/MCP workflow.

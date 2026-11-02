# NEEDS ALI — human-required items

Each item: what it is, exact steps for Ali, and what the AI will do once he answers.
The AI keeps working on unblocked work meanwhile.

---

## 1. DES-6 playtest gate (BLOCKS Phase 8 art pass per the build plan)

**What:** The Phase 7.6 design gate — a 2-player session must show the game is fun.
Two hard questions: (a) did anyone laugh unprompted, (b) did players talk to coordinate.

**Exact steps:**
1. Open `Holdover.uproject` in UE 5.8, press **Play** (Alt+P) — two windows open
   (listen-server host + client).
2. Two people play, one per window, on voice chat (Discord/Steam). A third person
   observes and takes notes.
3. Follow `Docs/DES6_PLAYTEST_SESSION.md` — pre-brief, 3 shifts, 22-item observation
   checklist, post-session interview.
4. Record results in the "DES-6 Playtest Results" template at the bottom of that file.
5. Tell the AI: PASS or FAIL on each of the two gate questions, plus the 3 strongest
   observations and the 3 weakest moments.

**Once you answer:** if PASS, the AI proceeds to Phase 8 art pass with confidence in the
loop. If FAIL, the AI writes a fix plan targeting the observed weak moments first and
re-runs the gate before Phase 8.

**Note:** the build is currently clean and 16/16 automation tests pass — the game is
ready to play. `L_RampGreybox` is the startup map, Night 1 profile is the default.

---

## 2. Phase 8 atmosphere look — eyeball pass (BLOCKS sign-off of Phase 8a)

**What:** `Tools/build_ramp_atmosphere.py` now sets up a snowy-night look on
`L_RampGreybox` (cold steel-blue moonlight + cool sky ambient + cold volumetric
fog + four warm sodium-orange ramp light pools + mild post-process). The values
are reasoned defaults, **not** a human-approved art direction, and the builder
runs headless so nobody has seen the render.

**Exact steps:**
1. Run `Tools/build_ramp_greybox.py` then `Tools/build_ramp_atmosphere.py` (see
   each file's header for the headless command), OR just open the editor on
   `L_RampGreybox` — it is already built and saved.
2. Enter PIE and look at the ramp. Judge four things:
   a. **Is it cold?** The night should read genuinely cold (blue), not neutral grey.
   b. **Is it readable?** Can you see the aircraft sections, the deicing pad, the
      refill station, the cones, the truck — and the HUD? Readability beats mood.
   c. **Is the warmth right?** The four orange pools should feel like "working at
      2am under sodium lamps" — the comedy warmth. Too weak = flat; too strong = day.
   d. **Is the fog right?** Should give depth and a snow-sky silhouette without
      hiding the plane or the game-critical targets.
3. Tell the AI: OK / too dark / too bright / too foggy / too flat, plus which of
   the four ramp lights (if any) are badly placed or too hot.

**Knobs the AI will turn** (all in `Tools/build_ramp_atmosphere.py`, top of file):
`MOON_RGB`/`SKY_RGB`/`FOG_RGB`/`RAMP_RGB` palettes, moon `intensity` (3.0) and
pitch, sky `intensity` (0.8), fog `fog_density` (0.03), ramp light `intensity`
(9000) / cone angles / `RAMP_LIGHT_Z`, and the post-process block (bloom,
vignette, AO, auto-exposure, saturation).

**Once you answer:** the AI retunes the named knobs, re-runs the builder, and
re-checks the READBACK block until you sign off. One round-trip per verdict.

---

## 3. Falling snow VFX — needs an authored Niagara emitter (BLOCKS Phase 8 "Snowfall")

**What:** The Phase 8 plan calls for "Snowfall using Niagara". This build's Python
API cannot author a Niagara emitter (no `NiagaraEditorLibrary`, no emitter handles
on `NiagaraSystem`, and no snow emitter ships with the engine to copy). Full
diagnosis in `Docs/AI/BLOCKERS.md`. Fog, warm lights, ground materials and taxi
lines are all DONE — only *falling* snow is missing. A `VolumetricCloud`
(`ATMO_Cloud`) is in place as the snow-sky placeholder.

**Exact steps (preferred — ~10 minutes in the editor):**
1. Open `Holdover.uproject` in UE 5.8, open the Niagara editor.
2. Create a new Niagara System, name it `NS_SnowFlurry`, save it to
   `/Game/Holdover/VFX/` (the folder may already exist from a probe).
3. Add one emitter and make it a snow flurry: sprites (small white/very pale blue),
   spawn rate ~2000-4000, gravity/slow downward velocity (~-60 to -120), random
   initial location inside a box roughly 6000x6000x4000, size 2-6, slight wind/blur,
   lifetime 4-8s. Keep it sparse enough to see the plane and the HUD through it.
4. Save. Tell the AI "NS_SnowFlurry is ready".

**Once you answer:** the AI spawns a `NiagaraActor` with `NS_SnowFlurry` from
`Tools/build_ramp_atmosphere.py` (tagged `builder:atmosphere`, sized to cover the
60m ramp), wires it into the READBACK verification, re-runs the builder and tests,
and commits. It will tune spawn rate/bounds headless from there.

**Alternative:** supply an importable snow VFX asset and the AI imports it via a
`Tools/*.py` builder instead.

---

## 4. Phase 8 art assets + purchases (BLOCKS asset imports)

**What:** Phase 8 also wants modular airport assets (floor, buildings), one
character and one aircraft, and real materials. The AI cannot buy assets or
approve art direction.

**Exact steps:**
1. Review the Phase 8 target look (see `Docs/HOLDOVER_UE5.8_MCP_BUILD_PLAN.md` §Phase 8).
2. Provide/point to locally-importable assets, OR approve placeholder-only for the slice.
3. Confirm the palette: cold blue ambient grade + warm orange ramp lights + fog.

**Once you answer:** the AI imports what you provide via the `Tools/*.py` builders / MCP
(never hand-editing binary assets), replaces one category at a time, and re-runs the full
treatment shift after each swap. For anything unavailable, the AI uses a placeholder and
lists the required asset here.

---

## 5. 3–4 player MP sessions (BLOCKS Phase 9 "done when")

**What:** Phase 9's done-when is "a host and client complete one shift with matching
results; then test three or four players." Needs 3–4 humans.

**Exact steps:**
1. Launch the editor, Play with 3–4 players (adjust PIE player count in
   `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` — see `Docs/TESTING.md`).
2. Play one complete shift. Watch for: two clients clearing the same plane, double-awarded
   pay, self-set coverage, HUD/scoring desync.
3. Report any mismatch between host and client views of section coverage, holdover,
   fluid, scoring and shift result.

**Once you answer:** the AI fixes every reported desync and re-verifies with the
world-based MP automation tests (PROC-1) before calling Phase 9 done.

---

## 6. Steamworks setup (BLOCKS Phase 10 Steam prep)

**What:** Steamworks account, app ID, store page, and online-subsystem config require
credentials and purchases. The AI must not create accounts, enter credentials, or buy.

**Exact steps (for Ali):** [to be enumerated when Phase 10 is reached — the AI will fill
this in with the exact Steamworks steps it needs done]

**Once you answer:** the AI wires the Steam online subsystem/transport against the exact
build and verifies invites/friends locally — it never touches the store page or payments.

---

## 7. Field-manual placeholders — studio / legal / feedback route (BLOCKS D2/D3 sign-off)

**What:** The in-game field manual (press **F1**, or it auto-opens on the first
shift) carries the tutorial, credits, legal notices and feedback route. The
tutorial copy is complete. The credits, legal and feedback text still contain the
marker `[[TODO:ALI]]` where a real name, licence or URL must go — deliberately,
so nothing ships with invented names or guessed URLs. A test
(`Holdover.Info.CreditsLegalFeedback`) asserts the markers are still present.

**Exact steps (fill these in):**
1. **Studio name** — what appears under "Studio:" in the credits.
2. **Credits names** — design/code, art and audio attribution (or "team" if you
   want no individual names).
3. **Copyright holder + year** — e.g. `Copyright (c) 2026 <Holder>.`.
4. **EULA** — a link or file reference, or say "omit the EULA line".
5. **Feedback route** — where players report bugs. An email, a Discord invite, or
   a GitHub issues URL. **The AI will not invent one.**

**Once you answer:** the AI replaces the `[[TODO:ALI]]` markers in
`Source/Holdover/HoldoverInfoContent.cpp`, flips the
`Holdover.Info.CreditsLegalFeedback` test from "asserts placeholder present" to
"asserts no placeholder remains", rebuilds, runs the suite, and commits. No other
code changes.

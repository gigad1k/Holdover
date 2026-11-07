# NEEDS ALI — consolidated, prioritised (2026-11-02)

Everything the AI cannot do itself, in the order that unblocks the most work.
Each item: what to do, exact steps, estimated time, and what the AI does after.
The AI keeps working on unblocked items meanwhile (none left — the queue is
entirely human-side). Nothing here has been approved yet; atmosphere, art and
playtest results stay "pending" until you say otherwise.

## Priority list

| # | Item | Est. minutes | Unblocks |
|---|---|---|---|
| 1 | Full shift on the packaged build (solo) | ~40 | Demo confidence; checklist B |
| 2 | DES-6 2-player playtest gate | ~90 | Phase 8 art sign-off |
| 3 | 3–4 player MP session | ~45 | Phase 9 "done when" |
| 4 | Field-manual placeholder values (6 strings) | ~5 | D2/D3 sign-off |
| 5 | Legal DRAFT review | ~20 (+ lawyer) | Shipping the legal text |
| 6 | Atmosphere look eyeball | ~10 | Phase 8a sign-off |
| 7 | Falling snow — author `NS_SnowFlurry` | ~10 | Phase 8 "Snowfall" |
| 8 | Art assets (or approve placeholders) | ~30 | Phase 8 asset imports |
| 9 | Steamworks account + app ID | ~60 | Phase 10 Steam pass |

Total if done in one sitting: **~5 hours**. Items 1–5 are the demo-critical path;
6–8 are the art pass; 9 is post-demo.

---

## 1. Full shift on the packaged build (HIGHEST PRIORITY)

**What:** Play one complete shift in the packaged Win64 build — the first real
outside-the-editor run of the slice. Also covers display/audio/quit/restart.

**Exact steps:**
1. Run `Tools\package_win64.ps1` if `Builds\Windows\Holdover.exe` is missing or
   stale (it builds, cooks, and smoke-tests automatically — ~1 min warm).
2. Launch `Builds\Windows\Holdover.exe` directly (no editor).
3. Play one whole shift solo: clean → protect → holdover → depart, meet quota,
   watch the shift results screen.
4. While you're in there: check windowed/fullscreen + resolution apply and
   survive a restart; audio (radio beep, spray hiss, wind, collision) is audible
   and loops stop when they should; quit and relaunch cleanly.
5. Press **F1** — the field manual (tutorial / credits / legal / feedback) must
   open. Also confirm whether it auto-opens on the first shift (headless testing
   could not verify auto-open — UMG tick needs a real window).
6. Optional: try the Low graphics preset — the ramp must stay readable.

**Report back:** anything broken/unfair/confusing, plus "F1 works / auto-open
works or not". `Docs/DEMO_CHECKLIST.md` section B is the same list.

**Once you answer:** the AI fixes every reported bug first (crashes and
treatment-rule breaks before cosmetics), repackages, and re-runs the smoke test.

---

## 2. DES-6 2-player playtest gate

**What:** The Phase 7.6 design gate — a 2-player session must show the game is
fun. Two hard questions: (a) did anyone laugh unprompted, (b) did players talk
to coordinate. This gates the Phase 8 art pass.

**Exact steps:**
1. Open `Holdover.uproject` in UE 5.8, press **Play** (Alt+P) — two windows
   (listen-server host + client).
2. Two people play, one per window, on voice chat. A third observes and takes
   notes (optional but better).
3. Follow `Docs/DES6_PLAYTEST_SESSION.md` — pre-brief, 3 shifts, 22-item
   observation checklist, post-session interview.
4. Record results in the "DES-6 Playtest Results" template at the bottom of
   that file.
5. Tell the AI: PASS or FAIL on each gate question, plus the 3 strongest
   observations and the 3 weakest moments.

**Once you answer:** PASS → the AI starts the Phase 8 art pass with confidence
in the loop. FAIL → the AI writes a fix plan targeting the observed weak
moments first and re-runs the gate before art.

---

## 3. 3–4 player MP session

**What:** Phase 9's done-when is "a host and client complete one shift with
matching results; then test three or four players." The code side is verified by
automation (`Holdover.MP.*` — spray validation, double-award guard, refill
guard, boom rate, LOS, replication contract); live sessions are the missing
evidence.

**Exact steps:**
1. Packaged build or editor PIE with 3–4 players (PIE player count:
   `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, see
   `Docs/TESTING.md`).
2. Play one complete shift. Watch for: two clients clearing the same plane,
   double-awarded pay, self-set coverage, HUD/scoring desync between machines.
3. Also try: a client disconnects while in the deicing truck (truck must stay
   usable), and a late join mid-shift (timers/ledger must look right).
4. Report any mismatch between host and client views of section coverage,
   holdover, fluid, scoring and shift result.

**Once you answer:** the AI fixes every reported desync, re-verifies with the
world-based MP tests, and only then calls Phase 9 done.

---

## 4. Field-manual placeholder values (6 strings)

**What:** The in-game field manual (**F1**) carries credits, legal notices and
feedback route. Six values carry the marker `[[TODO:ALI]]` because the AI must
not invent names, licences or URLs. A test asserts the markers are still there.

**Exact steps (reply with 6 values):**
1. **Studio name** — under "Studio:" in the credits.
2. **Design/code attribution** — a name, or "the Holdover team".
3. **Art/audio attribution** — a name, or "the Holdover team".
4. **Copyright line** — e.g. `2026 <Holder>`.
5. **EULA reference** — a file/URL you want cited, or "omit the EULA line".
6. **Feedback route** — where players report bugs: email, Discord invite, or
   issues URL. The AI will not invent one.

They live in ONE block at the top of `Source/Holdover/HoldoverInfoContent.cpp`
(`GStudioName` … `GFeedbackRoute`) — a six-line edit.

**Once you answer:** the AI fills the six constants, flips the
`Holdover.Info.CreditsLegalFeedback` test from "placeholder present" to "no
placeholder remains", rebuilds, runs the suite, repackages, commits.

---

## 5. Legal DRAFT review

**What:** The field-manual legal block is a **DRAFT** — it is labelled
"DRAFT - not legal advice, needs Ali/lawyer review" in-game and a test enforces
the label. It is not final and must not ship presented as final.

**Exact steps:**
1. Open the field manual (**F1** → Legal) or read `LegalText()` in
   `Source/Holdover/HoldoverInfoContent.cpp`.
2. Review yourself, then have a lawyer check it (it is currently 4 short
   paragraphs: copyright, fiction disclaimer, evaluation-use clause,
   third-party notices pointer).
3. Reply with: APPROVED (wording is final), or the exact replacement text.

**Once you answer:** the AI swaps in the approved text, removes the DRAFT label
and its test assertion only when you say APPROVED, and treats the replacement
text as literal (no invented clauses). Until then the label stays.

---

## 6. Atmosphere look eyeball

**What:** `Tools/build_ramp_atmosphere.py` sets a snowy-night look on
`L_RampGreybox` (cold steel-blue moonlight + cool sky + cold volumetric fog +
four warm sodium-orange ramp light pools + mild post-process). The values are
reasoned defaults, **not** approved art direction, and nobody has seen the
render.

**Exact steps:**
1. Open the editor on `L_RampGreybox` (already built and saved), enter PIE.
2. Judge four things: (a) is it genuinely cold (blue)? (b) is everything
   readable — plane sections, pad, refill station, cones, truck, HUD?
   (c) do the four orange pools feel like "2am under sodium lamps"?
   (d) does the fog give depth without hiding targets?
3. Reply: OK / too dark / too bright / too foggy / too flat, plus any ramp
   light that is badly placed or too hot.

**Once you answer:** the AI retunes the named knobs
(`MOON_RGB`/`SKY_RGB`/`FOG_RGB`/`RAMP_RGB`, moon/sky/fog intensities, ramp light
intensity/cones, post-process block — all at the top of the builder), re-runs
the builder, and re-checks the READBACK block. One round-trip per verdict.

---

## 7. Falling snow — author `NS_SnowFlurry` (Niagara)

**What:** Phase 8 wants Niagara snowfall. This build's Python API cannot author
a Niagara emitter (no `NiagaraEditorLibrary`, no emitter handles, no engine
template to copy — full diagnosis in `Docs/AI/BLOCKERS.md`). Everything else in
Phase 8 is done; only *falling* snow is missing (`ATMO_Cloud` is the snow-sky
placeholder).

**Exact steps (~10 min in the editor):**
1. Open `Holdover.uproject`, open the Niagara editor.
2. Create a Niagara System named `NS_SnowFlurry`, save to `/Game/Holdover/VFX/`.
3. One emitter: small white/very pale blue sprites, spawn rate 2000–4000,
   downward velocity ~-60 to -120, spawn box ~6000x6000x4000, size 2–6, slight
   wind/blur, lifetime 4–8s. Sparse enough to see the plane and HUD through it.
4. Save. Tell the AI "NS_SnowFlurry is ready".

**Once you answer:** the AI spawns a `NiagaraActor` with `NS_SnowFlurry` from
`Tools/build_ramp_atmosphere.py` (tagged `builder:atmosphere`, covering the 60m
ramp), wires it into READBACK verification, re-runs builder + tests +
repackage, and tunes spawn rate/bounds headless from there.

**Alternative:** supply an importable snow VFX asset and the AI imports it via a
`Tools/*.py` builder instead.

---

## 8. Art assets (or approve placeholders)

**What:** Phase 8 wants modular airport assets (floor, buildings), one character
and one aircraft, and real materials. The AI cannot buy assets or approve art
direction.

**Exact steps:**
1. Review the Phase 8 target look
   (`Docs/HOLDOVER_UE5.8_MCP_BUILD_PLAN.md` §Phase 8).
2. Provide/point to locally-importable assets, OR approve placeholder-only for
   the demo slice.
3. Confirm the palette: cold blue ambient grade + warm orange ramp lights + fog.

**Once you answer:** the AI imports what you provide via the `Tools/*.py`
builders / MCP (never hand-editing binary assets), replaces one category at a
time, and re-runs the full treatment shift after each swap. Unavailable assets
get placeholders listed back here.

---

## 9. Steamworks account + app ID

**What:** Phase 10 Steam pass (session/friend invites) needs a Steamworks
account, an app ID, and a store page eventually. The AI must not create
accounts, enter credentials, buy anything, or touch store pages/payments.

**Exact steps (yours — the AI never does these):**
1. Create/confirm a Steamworks partner account (partner.steampowered.com) and
   pay the app-submission fee if a new app is needed.
2. Create the app entry (or use an existing one) and give the AI just the
   **App ID**.
3. Install/launch the Steam client on the dev machine (local invites testing
   needs it running).
4. Later (post-demo): store page, trailer, screenshots — your call entirely.

**Once you answer:** the AI wires `OnlineSubsystemSteam`/transport against that
exact App ID in `DefaultEngine.ini`, verifies invites/friends on the local
listen-server flow in a packaged build, and never touches the store page or
payments. The store page itself is yours until you explicitly hand over the
non-payment parts.

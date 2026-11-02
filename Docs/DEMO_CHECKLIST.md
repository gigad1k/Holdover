# HOLDOVER — Release-candidate demo checklist (Phase 10)

How to verify a packaged build before calling the demo done. Every item is
marked **AUTO** (scripted, already green), **HUMAN** (needs Ali or a playtester),
or **PENDING** (not implemented yet). Do not mark an item done without evidence.

Build: `Tools\package_win64.ps1` → `Builds\Windows\Holdover.exe`
(Win64 Development, cook+stage+pak+archive, then headless smoke test).

## A. Build & launch (scripted)

| # | Item | Status | Evidence |
|---|---|---|---|
| A1 | Editor target builds clean (only the pre-existing `OnPostEngineInit` deprecation warning) | AUTO | `Build.bat HoldoverEditor Win64 Development` |
| A2 | Automation suite passes to 0 failures | AUTO | `Tools\run_tests.ps1` — 23/23 (`Holdover.MP.*` + legacy suite) |
| A3 | Packaged Win64 Development build produced | AUTO | `Tools\package_win64.ps1` → `Builds\Windows\Holdover.exe`, BUILD SUCCESSFUL |
| A4 | Packaged game launches outside the editor and loads `L_RampGreybox` | AUTO | smoke test (20 s headless, no fatal errors, `LoadMap: L_RampGreybox` + "Bringing World up for play") |

## B. Full shift on the packaged build (HUMAN)

| # | Item | Status | What to check |
|---|---|---|---|
| B1 | Fresh launch on a clean machine/account | HUMAN | first run creates no missing-file errors; window+resolution sane |
| B2 | Complete one whole shift end-to-end | HUMAN | clean → protect → holdover → depart loop, quota, shift results screen |
| B3 | Display settings (window/fullscreen/resolution) | HUMAN | apply + persist across restart |
| B4 | Audio levels (radio beep, spray hiss, wind, collision cue) | HUMAN | audible, no looping leaks (P7-2 was the hiss-leak fix) |
| B5 | Quit and restart | HUMAN | clean exit, no orphan processes, settings persist |
| B6 | Low-quality graphics preset | HUMAN | ramp stays readable (readability beats mood) |
| B7 | HUD readability at 2am-glow look | HUMAN | overlaps NEEDS_ALI #2 (atmosphere eyeball) |

## C. Multiplayer (HUMAN — NEEDS_ALI #5)

| # | Item | Status | What to check |
|---|---|---|---|
| C1 | Host + 1 client complete one shift with matching results | HUMAN | section coverage, holdover, fluid, scoring, shift result all agree |
| C2 | 3–4 player session | HUMAN | same, plus double-clear / double-award / self-set-coverage watch list |
| C3 | Net emulation soak (`PktLag=120 PktLoss=5`) | HUMAN | no rubber-banding, boom usable, no disconnects from RPC flood |
| C4 | Client disconnect while in the truck | HUMAN | truck stays usable, occupant character restored (MP-3) |
| C5 | Late join mid-shift | HUMAN | joined client sees correct timers/ledger (G-5) |

Code-side authority for C1–C5 is verified by automation (`Holdover.MP.*` —
spray dt/range/LOS, depart-once, coverage authority, refill guard, boom rate,
replication contract); live sessions are the remaining evidence.

## D. Content & polish (PENDING / HUMAN)

| # | Item | Status | Notes |
|---|---|---|---|
| D1 | First-shift tutorial / tips | PENDING | text-only, no store page work |
| D2 | Credits + legal notices | PENDING | text-only |
| D3 | Feedback route (where players report bugs) | PENDING | text-only |
| D4 | Snow (falling, Niagara) | BLOCKED | see `Docs/AI/BLOCKERS.md` — needs authored `NS_SnowFlurry` (NEEDS_ALI #3) |
| D5 | Art pass sign-off (atmosphere, assets, character, aircraft) | NEEDS_ALI | #2 atmosphere eyeball, #4 assets — NOT approved by Ali yet |
| D6 | Trailer / GIFs capture | HUMAN | capture genuine gameplay only after B2 passes |
| D7 | Steam store page / Steamworks | NEEDS_ALI #6 | AI must never touch store pages, accounts or payments |

## E. Remaining blockers at this checkpoint (2026-11-02)

1. **NEEDS_ALI #5** — 2-client and 3–4 player live sessions (Phase 9 done-when).
2. **NEEDS_ALI #1** — DES-6 playtest gate results (gates art sign-off).
3. **NEEDS_ALI #2/#3/#4** — atmosphere look, snow emitter, art assets.
4. **NEEDS_ALI #6** — Steamworks (Phase 10 Steam pass).
5. **D1–D3** — tutorial/credits/legal/feedback text (AI-implementable, queued).
6. **Cook warnings (non-blocking)** — `BP_FirstPersonPlayerController.uasset`
   has a stale `/Script/TP_FirstPerson` import (template leftover, warning only);
   the map's GameMode is `BP_FirstPersonGameMode_C` — verify at B2 that it
   derives from `AHoldoverGameMode` (shift/HUD worked in PIE playtests).

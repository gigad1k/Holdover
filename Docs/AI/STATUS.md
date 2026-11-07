# READY FOR ALI

_2026-11-06 — AI-fixable backlog complete (Claude's Sonnet review remainder)._

## What was fixed (one commit per item, all on `origin/gigad1k/holdover`)

| Item | Commit | Fix |
|---|---|---|
| **M4** client sprint prediction ignores ice slowdown | `72aee56` | `IceScale` is replicated (`OnRep_IceScale` recomputes); the client now predicts `Base*Ice*Sprint` exactly like the server. Pinned by `Holdover.MP.ReplicationContract` + `ReplicationLint`. |
| **M5** ice slowdown not overlap-counted | `1cec5f8` | Per-character overlap counter — leaving one ice patch while inside another keeps the slow; cleared only at the last release (unbalanced releases clamped). New `Holdover.Hazard.IceOverlapCount` (runtime-red first: "still slowed ... expected 300 but was 600"). |
| **M6** jet-blast prediction diverges/doubles on listen host | `2c09dc8` | Prediction only on true clients (`ShouldPredictLaunch`); ice-stumble and jet-blast directions are deterministic shared formulas (the ice path used its own `FRandRange` per side). Cue still plays locally. New `Holdover.Hazard.JetBlastPrediction` (gate matrix + both directions, driven in the listen-host seat). |
| **M8** disconnecting truck occupant leaves an orphan | `7c7509d` | `EvictOccupant(bDestroyOrphan)` destroys the abandoned occupant on the leaving path; the duplicate eviction in `GameMode::Logout` is gone — `PawnLeavingGame` is the single disconnect path (it also protects the level-placed truck). New `Holdover.Truck.OccupantEviction` (runtime-red first: orphan was still alive). |
| **M12** departure board shows one aircraft | `41bd70d` | `GatherBoardAircraft` selects ALL arrived, non-departed aircraft; one row-group each (pooled text blocks). New `Holdover.UI.DepartureBoardRows`. |
| **M14 remainder** audio hygiene | `19fd92e` | Sounds cached in `UHoldoverAudioSubsystem` (one load, GC-safe, misses cached); `Play2D` skipped on dedicated servers; `OnRep_Dispatch` no longer beeps on the initial join replicate. New `Holdover.Audio.Hygiene`. |
| **N2–N11** nits | `2ed4303` | N2 HUD/board `SetText`-on-change + pawn-cached spray lookup; N3 boom RPC throttled to ~20 Hz; N4 exit teleports the driver beside the truck (`Holdover.Truck.ExitTeleport`); N5 restart interactable single reset + per-instance toggle, post-verdict-only `RestartShift` (`Holdover.Shift.RestartGuard`); N6 `HasAuthority` guards on the shift API; N7 refill-station scan cached; N8 `Aircraft::Tick` stops clobbering designer tuning (`Holdover.Treatment.AircraftTuningStays`); N9 unused AI/StateTree deps dropped; N10 done with M8; N11 `GetCurrentProfile()` const. |

## Tests

`Tools/run_tests.ps1` — **39/39 expected tests pass** (was 31; +8 new: `Holdover.Hazard.IceOverlapCount`, `Holdover.Hazard.JetBlastPrediction`, `Holdover.Truck.OccupantEviction`, `Holdover.UI.DepartureBoardRows`, `Holdover.Audio.Hygiene`, `Holdover.Shift.RestartGuard`, `Holdover.Truck.ExitTeleport`, `Holdover.Treatment.AircraftTuningStays`). The expected list in `Tools/run_tests.ps1` carries all 39 names. Each new behavioural test was run red before its fix where the item was testable.

## Smoke gate

`Tools/package_win64.ps1` (strict gate) — **PASS** on the packaged Win64 Development build at `Builds\Windows\Holdover\Binaries\Win64\Holdover.exe`: `LoadMap: /Game/Holdover/Maps/L_RampGreybox` + `Field manual built: 8 tips + credits/legal/feedback` present, process tree killed cleanly, 0 fatal errors, 0 content errors, no `PLACEHOLDER AUDIO MISSING`. (`Saved\Logs\package_smoke.log`.)

## Still needs you (play — NOT verified by the AI)

Fixes above are code-and-test verified only. The feel/visual/two-player checks, foldable into the existing `Docs/AI/NEEDS_ALI.md` queue items 1–3:

1. **Hazard feel (M4/M5/M6)** — sprint on ice (no rubber-banding), walking out of one ice patch while in another keeps the slow, jet blast pushes once and the same way for host and client (2-player PIE, item 2/3 of NEEDS_ALI).
2. **Truck disconnect (M8)** — 2-player: a client in the truck disconnects; the truck stays usable and no body is left standing (NEEDS_ALI item 3).
3. **Departure board + HUD look (M12/N2)** — windowed run: both aircraft on the board, rows readable, HUD looks unchanged (UMG ticks do not run under `-nullrhi`, so this cannot be smoke-tested).
4. **Audio + boom + exit (M14/N3/N4)** — join beep only on news (not on join), no spray/beep hitches (cache), boom still feels right at 20 Hz, truck exit lands you beside the truck.
5. **Field-manual auto-open** (carried over) — still needs the windowed-run confirmation; F1 remains the reliable path.

## Not fixed / caveats

- **N3 owner drift (residual):** the boom RPC is throttled and targets are absolute (the server converges on release), but `COND_SkipOwner` (the deliberate MP-7 fix) means the host's view of a client's boom can lag transiently while the key is held. True reconciliation needs a correction RPC — left as a feel question for you.
- **Dispatcher beeps under-play on clients** when the server batches several `AddDispatch` calls into one replication (unchanged gameplay gap in `Docs/KNOWN_ISSUES.md`); the M14 fix is exactly the prescribed "skip when the feed did not grow".
- Everything else in the Medium backlog (M4–M6, M8, M12, M14 remainder, N2–N11) is fixed; `Docs/KNOWN_ISSUES.md` and `Docs/CHANGELOG.md` are updated to match.

`Docs/AI/NEEDS_ALI.md` is unchanged from the consolidated 9-item queue (items 1–5 are still the demo-critical path). Nothing touched `main`; all commits are on `origin/gigad1k/holdover` (PR #6).

# HANDOFF

_Last written 2026-11-02 at the end of Phase 10 no-human scope. Read this whole file before continuing._

## Project / rules recap
- **HOLDOVER** — UE 5.8 C++ co-op comedy game. Underqualified ground crew deicing
  aircraft before holdover timers expire. Project root:
  `C:\Users\shahr\orca\workspaces\Holdover\mimo-dev`, branch **`gigad1k/mimo-dev`**.
- **Work only in this worktree.** Do not touch `main` or `claude-dev`, do not merge
  to main, do not push.
- Loop per item: implement → build `HoldoverEditor` Win64 Development → run
  `Tools/run_tests.ps1` to 0 failures → small commit → tick `Docs/CHANGELOG.md`.
- Never hand-edit binary assets. Generate maps/assets **only** through
  `Tools/*.py` builders / MCP.
- Never enter credentials / create accounts / buy anything / touch Steamworks,
  payments or store pages — those become `Docs/AI/NEEDS_ALI.md` tasks for Ali.
- Anything needing a human → `Docs/AI/NEEDS_ALI.md` (exact steps + what happens
  after he answers), then keep going on unblocked work.
- 3 repeats of the same failure → stop, `Docs/AI/BLOCKERS.md`, move on.
- ~75% context → commit + rewrite this file + say so.
- At each phase end and about hourly → 5-line status in `Docs/AI/STATUS.md`.
- **Use the Grep tool for content search, not PowerShell scans.** (`Falcon_Grep`
  is not reachable — no MCP server exposes it and `chrome-devtools` is down.)

## DONE
- **Phase 7.6 complete** — DES-1..DES-6 + SIM-1..SIM-5. `a49dd5c` fixed real bugs
  (one-tank toolset JSON, `HasAuthority` on a component, null-`GetWorld()` crashes,
  missing `#include "Holdover.h"` masked by unity builds, 2 stale test assertions).
- **DES-6 playtest script** — `Docs/DES6_PLAYTEST_SESSION.md` (`34388d0`).
- **Phase 8a complete** (`dadec8c`→`3a9f8c4`) — PROC-5 actor-tag ownership model
  (`Tools/mapbuild_common.py`), atmosphere (`build_ramp_atmosphere.py`), wet/icy
  materials + taxi decals (`build_ramp_surface.py`), snow-sky cloud placeholder.
- **Phase 9 complete** (`1b16274`→`edc9210`) — MP-1..MP-9 audit
  (`Docs/MP_AUDIT_2026-11-02.md`), PROC-1 world-based MP tests (5 `Holdover.MP.*`),
  hardening G-1..G-5 (refill guard, token-bucket boom rate, server LOS ×2, late-join
  replication).
- **Phase 10 no-human complete** (`b483301`→`9ad8c86`) — packaging script
  (`Tools/package_win64.ps1`), DEMO_CHECKLIST, D1-D3 field manual, packaged audio
  fix, PROC-6 config hygiene.

## IN PROGRESS
Nothing. Working tree clean at `9ad8c86`. Suite 26/26 green.

## NEXT 5 STEPS (in order)
1. **Ask Ali for `Docs/AI/NEEDS_ALI.md` #7** — the field-manual placeholders
   (studio name, credits, copyright holder, EULA, feedback route). Until then the
   credits/legal/feedback text carries `[[TODO:ALI]]` and
   `Holdover.Info.CreditsLegalFeedback` asserts it is still there.
2. **Human windowed play-through** (DEMO_CHECKLIST B1-B7) on the packaged build —
   **including verifying the D1 auto-open** (see GOTCHAS: it cannot be verified
   headless). This also covers B7 HUD readability, which overlaps NEEDS_ALI #2.
3. **NEEDS_ALI #5** — 2-client and 3–4 player live MP sessions (C1-C5). Code-side
   authority is automated; live sessions are the remaining Phase 9 evidence.
4. **NEEDS_ALI #1** — DES-6 playtest gate. Gates the Phase 8 art sign-off.
5. **NEEDS_ALI #2/#3/#4** — atmosphere look eyeball, `NS_SnowFlurry` snow emitter,
   art assets. Then D6 trailer capture and D7 Steamworks (NEEDS_ALI #6).

**There is no remaining AI-implementable work** in the current plan. Every
DEMO_CHECKLIST item left is HUMAN, BLOCKED, or NEEDS_ALI. If Ali clears a queue
item, pick up the corresponding row and re-enter the loop.

## GOTCHAS (hard-won — read before touching builders, materials, or UMG)
- **UMG `UUserWidget::NativeTick` does not run under the headless `-nullrhi` smoke
  test.** Widgets tick only through `SObjectWidget`'s Slate paint pass, which
  headless has none of. Verified with a temporary diagnostic: `NativeConstruct`
  fires (`state=0` / `WaitingToStart`), `NativeTick` never does. Consequence: any
  per-frame UI logic is **unverifiable in the packaged smoke test** and needs the
  human windowed run. Evidence lines (`Field manual built: …`) that run from
  `RebuildWidget`/`NativeConstruct` DO appear headless — use those instead.
- **Unreal Python API names differ per build — probe before you code.** We built
  `Tools/probe_*.py` scripts for this (they are gitignored via `Tools/probe_*.py`
  in `.gitignore`). Verified names in UE 5.8:
  - Material inputs: `MaterialEditingLibrary.connect_material_property(expr, "",
    MaterialProperty.MP_BASE_COLOR)` — **not** `connect_material_expressions()`
    (that is expression→expression only). Enum uses underscores: `MP_BASE_COLOR`,
    `MP_EMISSIVE_COLOR`, `MP_ROUGHNESS`, `MP_METALLIC`.
  - `MaterialDomain.MD_DEFERRED_DECAL` (not `MD_DECAL`) for decal materials.
  - `DecalComponent` has `set_decal_material()` / `decal_material`. **No**
    `set_material()` on it.
  - `PostProcessVolume` keeps `settings` / `unbound` / `blend_radius` /
    `blend_weight` **on the actor** — no `PostProcessComponent`. Override flags are
    `override_<name>` inside `FPostProcessSettings`.
  - `FPostProcessSettings.color_saturation` / `color_contrast` are **`FVector4`**,
    not `FLinearColor`.
  - `ExponentialHeightFogComponent`: volumetric toggle is `enable_volumetric_fog`;
    fog colours go through `set_fog_inscattering_color()` /
    `set_directional_inscattering_color()` (the fields are `*_luminance`).
  - `-ScriptArgs=-foo=bar` does **not** populate `sys.argv`. Read
    `unreal.SystemLibrary.get_command_line()` and substring-search.
  - `UScrollBox` uses `AddChild()`, not `AddChildToScrollBox()`.
- **`UActorComponent` has no `HasAuthority()`** — use `GetOwner()->HasAuthority()`.
- **`AController` has no `GetHUD()`** — cast to `APlayerController` first.
- **`#include "Holdover.h"` for `LogHoldover`** — unity builds silently mask a
  missing include; it only surfaces when the adaptive build compiles a file alone.
  Hit this twice. Check includes when adding a `UE_LOG(LogHoldover, …)`.
- **`unreal.Rotator` positional ctor is `(roll, pitch, yaw)`.** The spawn helpers in
  `Tools/build_ramp_greybox.py` take `(pitch, yaw, roll)` and map explicitly. Positive
  pitch points AWAY from the scene (map renders black). Keep that convention.
- **Builder run order:** `build_ramp_greybox.py` → `build_ramp_atmosphere.py` →
  `build_ramp_surface.py`. Each is idempotent and owns only its own tagged actors
  (`builder:greybox` / `builder:atmosphere` / `builder:surface`).
- **Packaging:** `Tools/package_win64.ps1 [-Iterate] [-SmokeSeconds N]`. Kill any
  leftover `Holdover.exe` process first — it holds `Saved\Logs\package_smoke.log`
  and blocks the archive step (cost us a 30-minute timeout).
- **Build:** `& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" HoldoverEditor Win64 Development -project="<root>\Holdover.uproject" -waitmutex`
  (editor must be closed — Live Coding blocks UBT).
- **Tests:** `Tools/run_tests.ps1 -UEPath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"`.
  The script's `D:\Gamez\EPIC\UE_5.8` default is stale — that path does not exist.
  26 tests run; the script's 7 "expected" names are a legacy subset. Exit 0 = all pass.
- **One pre-existing build warning** (`FCoreDelegates::OnPostEngineInit` deprecated
  in `HoldoverEditorModule.cpp`) is harmless and not ours to fix.
- **`Docs/TEST_CHECKLIST.md` contains raw cp1252 `0x97` em-dash bytes** — whole-file
  re-encoding editors corrupt them. Patch at byte level or restore after.
- **ClassRedirects in `Config/DefaultEngine.ini` must stay** (TP_FirstPerson →
  Holdover) until the template Blueprint content is replaced — see `KNOWN_ISSUES.md`.
  The map's GameMode is `BP_FirstPersonGameMode_C`, which derives from
  `AHoldoverGameMode` via those redirects. Verified working (the packaged build's
  `ShiftStart` dispatch fires).

## Files to know
- `Tools/mapbuild_common.py` — ownership helpers (`claim`, `clear_owned`,
  `adopt_untagged_of_classes`, `should_clear`, `ATMO_CLASSES`).
- `Tools/build_ramp_greybox.py`, `build_ramp_atmosphere.py`, `build_ramp_surface.py` —
  the three map builders, in run order.
- `Tools/verify_proc5.py` — `-ScriptArgs=-mode=plant|check|clean` PROC-5 regression check.
- `Tools/package_win64.ps1` — packaged Win64 build + headless smoke test.
- `Source/Holdover/HoldoverInfoContent.h/cpp` — all D1-D3 field-manual text.
  Placeholders use `[[TODO:ALI]]` (`UHoldoverInfoContent::PlaceholderTag()`).
- `Source/Holdover/HoldoverInfoWidget.h/cpp` — the scrollable manual panel.
- `Source/Holdover/Tests/` — `HoldoverInfoTests.cpp` (D1-D3 copy),
  `HoldoverWorldTests.cpp` (`Holdover.MP.*`, PROC-1), plus the legacy suite.
- `Docs/AI/NEEDS_ALI.md` (7 items), `BLOCKERS.md` (snow), `STATUS.md`, `DEMO_CHECKLIST.md`.
- `Docs/MP_AUDIT_2026-11-02.md`, `Docs/CHANGELOG.md` — every commit ticked with API notes.

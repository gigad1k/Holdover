# HANDOFF

_Last written 2026-11-02 at the start of Phase 9 (MP audit done). Read this whole file before continuing._

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

## DONE
- **Phase 7.6 complete** (DES-1..DES-6, SIM-1..SIM-5). Commit `a49dd5c` fixed real
  bugs found by build+tests: one-tank toolset JSON, `HasAuthority` on a component,
  null-`GetWorld()` crashes in `HoldoverGameState`, missing `#include "Holdover.h"`
  (unity builds were masking it), and 2 stale test assertions.
- **DES-6 playtest script** committed (`34388d0`) — `Docs/DES6_PLAYTEST_SESSION.md`.
- **Phase 8a complete** (4 commits, `dadec8c` → `3a9f8c4`):
  - `Tools/mapbuild_common.py` — actor-tag ownership model. Builders tag actors
    `builder:<name>` and clear **only their own**. Untagged actors never destroyed.
    Fixes **CODE_REVIEW PROC-5** ("greybox builder deletes hand-placed work").
    Verified: planted an untagged probe actor, re-ran greybox, it SURVIVED.
  - `Tools/build_ramp_atmosphere.py` — cold steel-blue moon (3.0) + cool sky (0.8) +
    cold volumetric height fog (0.03) + 4 warm sodium-orange ramp spot pools (9000cd)
    + mild post-process (bloom 0.75 / vignette 0.28 / AO 0.55 / auto-exposure
    0.55-1.85) + `ATMO_Cloud` snow-sky placeholder.
  - `Tools/build_ramp_surface.py` — `M_ApronWet` (wet apron), `M_Ice` (glassy pad),
    `M_TaxiLine` + 5 deferred-decal taxi lines.
  - Tests **7/7 PASS** after every commit. `L_RampGreybox.umap` is at 45 actors
    (30 `builder:greybox` / 10 `builder:atmosphere` / 5 `builder:surface`).

## IN PROGRESS
**Phase 9 (started 2026-11-02).** MP-1..MP-9 audit DONE (`Docs/MP_AUDIT_2026-11-02.md`).
PROC-1 world-based MP tests DONE — 5 new tests, suite 21/21 green (`1b16274`+).
Next: hardening gaps G-1..G-5 (refill RPC not station-bound, boom rate clamp
per-RPC not per-time, no server LOS re-trace on spray/interact, late-join
shift-duration + pay tuning unreplicated), then Phase 10 no-human items
(packaging script, build config, demo checklist).

## NEXT 5 STEPS (in order)
1. **G-1 refill-station guard** — `ServerBeginRefill` must validate station
   proximity + valid enum + not-already-refilling (test in
   `Holdover.MP.RefillGuard`).
2. **G-2 boom rate clamp per elapsed time** (not per-RPC) + test update.
3. **G-3/G-4 server LOS re-trace** on spray and interact + tests.
4. **G-5 late-join replication** (`LastShiftDuration`, pay/deduction tuning) +
   contract-test update; then packaged Win64 dev build (`Tools/package_win64.ps1`).
5. **Tutorial / first-shift tips / credits / legal / feedback route** (Phase 10a,
   text only — no store page work).

Blocked on Ali (do not wait, keep working on the above):
`NEEDS_ALI.md` #1 DES-6 playtest gate (gates Phase 8 art sign-off), #2 atmosphere
look eyeball, #3 `NS_SnowFlurry` snow emitter, #4 art assets/purchases, #5 3-4p MP
sessions, #6 Steamworks.

## GOTCHAS (hard-won — read before touching builders or materials)
- **UE 5.8 automation-test gotchas (PROC-1, verified 2026-11-02):**
  - `AHoldoverCharacter` / `AHoldoverGameMode` are `UCLASS(abstract)` —
    `SpawnActor` refuses them ("is abstract"). Use CDOs
    (`StaticClass()->GetDefaultObject()`) for contract tests and plain `AActor`
    spray owners in world tests.
  - `UWorld::GameState` is **private**; wire it in tests via
    `FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), "GameState")->SetObjectPropertyValue_InContainer(...)`.
  - Calling `GetLifetimeReplicatedProps` from automation on `AHoldoverCharacter`
    **asserts** (`bIsPushBased == Other.bIsPushBased`, CoreNet.h `FLifetimeProperty::operator==`)
    and kills the whole test run. Don't enumerate it; check `CPF_Net` flags instead.
  - `IHoldoverInteractableInterface::Execute_Interact` does NOT dispatch to the
    native `Interact_Implementation` in automation — call `_Implementation`
    directly in tests (it works fine in the real PIE game path).
  - A test-side crash kills the rest of the run: the runner then reports every
    later test as MISSING and the log shows no "Automation Test Queue Empty".
  - A bare spawned `AActor` has **no root component** — `GetActorLocation()`
    silently returns (0,0,0). Give world-test owners a `USceneComponent` root.
- **Unreal Python API names differ per build — probe before you code.** We built
  `Tools/probe_*.py` scripts for exactly this. Verified names in UE 5.8:
  - `MaterialEditingLibrary.connect_material_property(expr, "", MaterialProperty.MP_BASE_COLOR)`
    for material inputs. **NOT** `connect_material_expressions()` (that is
    expression→expression only). Enum uses underscores: `MP_BASE_COLOR`,
    `MP_EMISSIVE_COLOR`, `MP_ROUGHNESS`, `MP_METALLIC`.
  - `MaterialDomain.MD_DEFERRED_DECAL` (not `MD_DECAL`) for decal materials.
  - `DecalComponent` has `set_decal_material()` / `decal_material`. There is **no**
    `set_material()` on it.
  - `PostProcessVolume` keeps `settings` / `unbound` / `blend_radius` / `blend_weight`
    **on the actor** — there is no `PostProcessComponent` on it. Override flags are
    `override_<name>` inside `FPostProcessSettings`.
  - `FPostProcessSettings.color_saturation` / `color_contrast` are **`FVector4`**,
    not `FLinearColor`.
  - `ExponentialHeightFogComponent`: volumetric toggle is `enable_volumetric_fog`;
    fog colours go through `set_fog_inscattering_color()` /
    `set_directional_inscattering_color()` (the fields are `*_luminance`).
  - `-ScriptArgs=-foo=bar` does **not** populate `sys.argv`. Read
    `unreal.SystemLibrary.get_command_line()` and substring-search.
- **`UActorComponent` has no `HasAuthority()`** — use `GetOwner()->HasAuthority()`.
- **`#include "Holdover.h"` for `LogHoldover`** — unity builds silently mask a
  missing include; it only surfaces when the adaptive build compiles a file alone.
- **`unreal.Rotator` positional ctor is `(roll, pitch, yaw)`.** The spawn helpers in
  `Tools/build_ramp_greybox.py` take `(pitch, yaw, roll)` and map explicitly. Positive
  pitch points AWAY from the scene (map renders black). Keep that convention.
- **Builder run order:** `build_ramp_greybox.py` → `build_ramp_atmosphere.py` →
  `build_ramp_surface.py`. Each is idempotent and owns only its own tagged actors.
- **`Tools/probe_*.py` is gitignored** (`Tools/probe_*.py` in `.gitignore`) — that is
  intentional; the named `verify_proc5.py` and `mapbuild_common.py` are tracked.
- **Tests:** `Tools/run_tests.ps1` — 7 expected tests, but 16 actually run. Exit 0 =
  all pass. Use `-UEPath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"`
  (the script's `D:\Gamez\EPIC\UE_5.8` default is stale — that path does not exist).
- **Build:** `& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" HoldoverEditor Win64 Development -project="<root>\Holdover.uproject" -waitmutex`
  (editor must be closed — Live Coding blocks UBT).
- **The one pre-existing build warning** (`FCoreDelegates::OnPostEngineInit`
  deprecated in `HoldoverEditorModule.cpp`) is harmless and not ours to fix.
- `Docs/TEST_CHECKLIST.md` contains raw cp1252 `0x97` em-dash bytes — whole-file
  re-encoding editors corrupt them. Patch at byte level or restore after.

## Files to know
- `Tools/mapbuild_common.py` — ownership helpers (`claim`, `clear_owned`,
  `adopt_untagged_of_classes`, `should_clear`, `ATMO_CLASSES`).
- `Tools/build_ramp_greybox.py`, `build_ramp_atmosphere.py`, `build_ramp_surface.py` —
  the three map builders, in run order.
- `Tools/verify_proc5.py` — `-ScriptArgs=-mode=plant|check|clean` PROC-5 regression check.
- `Docs/AI/NEEDS_ALI.md`, `BLOCKERS.md`, `STATUS.md` — the human-facing queue.
- `Docs/CHANGELOG.md` — every commit is ticked here with UE API notes.

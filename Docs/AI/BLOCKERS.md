# BLOCKERS

Diagnoses for work stopped on after repeated failure, per standing orders
(3 repeats -> stop, write it here, move on). Nothing here is "done" — it is
"stopped, here is why, here is what unblocks it".

---

## 2026-11-02 — Falling snow VFX (Niagara) cannot be authored headless

**Phase 8 task:** "Snowfall using Niagara, fog, cool ambience, warm orange ramp
lights, wet/icy ground materials and taxi-line decals."

**Status:** STOPPED after one feasibility probe (not 3 failed runs — the probe
proved the whole approach is unreachable rather than needing retries).

**What works:** fog, ambience, warm ramp lights, ground materials and taxi-line
decals are all DONE (see CHANGELOG 2026-11-02 entries). Only *falling snow* is blocked.

**Diagnosis (verified via `Tools/probe_niagara_api.py` in UE 5.8):**
1. `unreal.NiagaraSystemFactoryNew` exists and CAN create a `NiagaraSystem` asset
   — verified, produced a valid object at `/Game/Holdover/VFX/NS_ProbeSnow`.
2. But the created system exposes **no emitter authoring surface**: `emitter_handles`,
   `num_emitters`, `get_num_emitters` are all absent, and there are no
   `*emitter*`/`*source*`/`*effect*`/`*spawn*` properties on `NiagaraSystem`.
3. `unreal.NiagaraEditorLibrary` — the class that holds emitter/graph authoring
   helpers — **does not exist** in this build (`hasattr` = False).
4. **No Niagara content ships with the engine** to copy a snow emitter from:
   `/Engine/Plugins/FX`, `/Engine/Plugins/FX/Niagara`, `/Engine/Content`,
   `/Engine/Functions`, `/Niagara` all list **0 assets**. `/Game` has 245 assets,
   none Niagara.

So we can create an *empty* NiagaraSystem, but there is no Python-reachable way to
add an emitter and configure spawn rate / gravity / sprite size / colour-over-life,
and no template emitter to duplicate. A zero-emitter system renders nothing.

**Why not to work around it:** the build plan explicitly says "If an asset is not
available locally, use a placeholder and record the required asset rather than
silently inventing an import path." Fabricating an `.uasset` binary for a snow
emitter would be hand-editing a binary asset, which is forbidden.

**Placeholder in place now:** `ATMO_Cloud` (VolumetricCloud) in
`Tools/build_ramp_atmosphere.py` gives a heavy overcast snow-sky, which is the
best scriptable "it is snowing hard" read available headless.

**What unblocks it (any one of these):**
1. **A human authors a snow emitter in the Niagara editor** and saves it as
   `NS_SnowFlurry` under `/Game/Holdover/VFX/`. Then the AI can spawn an
   `NiagaraActor` with that system from `build_ramp_atmosphere.py` and tune its
   `NiagaraComponent` parameters (spawn rate, bounds) headless. **This is the
   preferred path** — see `Docs/AI/NEEDS_ALI.md`.
2. An importable snow VFX asset is supplied (FBX/ABC/particle asset) and the AI
   imports it via a `Tools/*.py` builder.
3. A future engine/plugin update exposes `NiagaraEditorLibrary` or emitter handles
   on `NiagaraSystem` in Python — then this becomes scriptable and the AI retries.

**Do not** keep retrying the current approach; the probe result is conclusive.

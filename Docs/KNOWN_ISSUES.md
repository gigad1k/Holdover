# KNOWN ISSUES

## Sonnet review 2026-11-02 — Medium backlog: DONE (2026-11-06)

Full detail in `Docs/AI/SONNET_REVIEW_P7-10.md`. The 4 Highs and the cheap
Mediums (M1-M3, M7, M9-M11, M13-M17-fixes, M15-M16, N1) were done 2026-11-02.
The remaining AI-fixable items are now also done (one commit each, all on
`origin/gigad1k/holdover`; tests 39/39; packaged smoke gate PASS — see
`Docs/AI/STATUS.md` and `Docs/CHANGELOG.md`):

- **M4 FIXED (`72aee56`)** — `IceScale` replicated; client sprint prediction
  includes the ice clamp. Need playtest feel check (PIE).
- **M5 FIXED (`1cec5f8`)** — ice slowdown is overlap-counted per character.
- **M6 FIXED (`2c09dc8`)** — hazard launch prediction only on true clients,
  deterministic directions (no more listen-host double launch / FRandRange
  divergence). Need playtest feel check (2-player PIE).
- **M8 FIXED (`7c7509d`)** — disconnecting truck occupant is destroyed with the
  leaving controller; eviction merged into `PawnLeavingGame` (the
  `GameMode::Logout` duplicate is gone). Needs 2-player disconnect check.
- **M12 FIXED (`41bd70d`)** — departure board shows all arrived, non-departed
  aircraft (one row-group each). Visual check needs a windowed run.
- **M14 remainder FIXED (`19fd92e`)** — sounds cached in
  `UHoldoverAudioSubsystem`, `Play2D` skipped on dedicated servers,
  `OnRep_Dispatch` skips the initial join replicate. Residual: beeps still
  under-play when the server batches `AddDispatch` (see Gameplay gaps).
- **N2-N11 FIXED (`2ed4303`)** — HUD/board SetText-on-change, boom RPC ~20 Hz,
  exit teleport beside the truck, restart interactable cleanup + post-verdict
  `RestartShift`, shift-API authority guards, refill-station scan cache,
  `Aircraft::Tick` no longer clobbers designer tuning, unused AI/StateTree deps
  dropped (N9), `GetCurrentProfile()` const (N11). N10 was done with M8.
  Residual (N3): `COND_SkipOwner` boom divergence while a key is held — see
  `Docs/AI/STATUS.md` "Not fixed / caveats".

## Tooling / MCP
- **`GetVisibleActors` only reports the editor world**, not PIE worlds — during PIE you cannot enumerate
  spawned pawns with it. Use `LogsToolset.GetLogEntries` for PIE evidence.
- **`call_tool` validation errors read "input params Json is empty"** even when params were sent but
  incomplete (e.g. missing required fields). Check the tool's `inputSchema` in the error text.
- **Unreal MCP is experimental** (UE 5.8): tool surfaces may change with engine patches. After engine
  updates run `ModelContextProtocol.RefreshTools` and re-verify.
- MCP server is **loopback-only, unauthenticated** — never expose port 8000.

## Editor / build
- **Actors without a root component spawn at the origin** — `spawn_actor_from_class` writes the
  spawn transform to the root; no root = no placement. Give every actor a scene root
  (cost us an "aircraft registered 0 sections" bug).
- **`unreal.Rotator` positional constructor is `(roll, pitch, yaw)`**, not `(pitch, yaw, roll)` like
  the editor UI. A spawned DirectionalLight with positive pitch points AWAY from the scene and the
  whole map renders black (SkyAtmosphere computes a night sky). `Tools/build_ramp_greybox.py` maps
  `(pitch, yaw, roll)` explicitly via keywords — keep that convention.
- **`GameFeatureData / Asset Manager settings` cook errors are FIXED** —
  `Config/DefaultGame.ini` now carries the `PrimaryAssetTypesToScan` entry
  (without it `package_win64.ps1`'s cook fails).
- **`new_level` cannot overwrite an existing map asset and `delete_asset` silently fails to delete
  a just-saved map** (returns True, asset still exists). No `save_current_level_to`/`save_current_level_as`
  in this build's Python bindings. The builder is idempotent by loading + clearing + `save_current_level`.
- **Live Coding blocks UBT**: close the editor before CLI builds (or use Live Coding from the editor).
- **PIE settings are per-user**: `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` overrides
  `Config/DefaultEditorPerProjectUserSettings.ini`. If PIE starts with the wrong player count / net mode,
  fix the Saved ini (see Docs/TESTING.md).
- First PIE / asset open compiles shaders for several minutes — not a hang.

## Template remnants (to be replaced deliberately per the build plan)
- Class names/log categories still carry template naming (`LogTemplateCharacter`, `BP_FirstPerson*` assets).
  Renamed module uses CoreRedirects (`Config/DefaultEngine.ini`) to keep template Blueprints alive —
  do not remove those redirects until the template content is replaced.
- `Media/` template thumbnails and Horror/Shooter variants were pruned; `Content/FirstPerson` is placeholder.

## Truck
- **Driving is intentionally off** (DrivingEnabled=false): movement uses FloatingPawnMovement
  without server move RPCs, so enabled driving is single-player-quality only until the Phase 9
  multiplayer audit adds authoritative movement. Boom/occupancy/spray are server-authoritative.

## Gameplay gaps (Phase 7 review backlog)
- **Hazard volumes only detect `ACharacter`** (`AHoldoverHazardVolume::OnOverlapBegin`) — the deicing
  truck pawn is immune to jet blast (no launch, no penalty) and triggers no collision cue. Decide
  truck hazard semantics in the Phase 9 multiplayer audit.
- **Empty-tank hint is transient**: `TargetHint` ("tank empty" style) appears only while spraying at
  a section and resets on release. The tank readout is always visible; a persistent hint when idle is
  nice-to-have.
- **Dispatcher radio beeps under-play on clients** when the server batches several `AddDispatch`
  calls into one replication (`OnRep_Dispatch` fires once per batch).
- **`Docs/TEST_CHECKLIST.md` contains raw `0x97` (cp1252 em dash) bytes** in the Phase 6 bullets.
  Whole-file re-encoding editors (e.g. the Edit tool) silently turn them into U+FFFD replacement
  chars — patch such files at byte level or restore the bytes afterwards.

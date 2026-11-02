# KNOWN ISSUES

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
- **Every editor/commandlet start logs 2 "GameFeatureData / Asset Manager settings" errors**
  (from the GameFeatures plugin enabled by AllToolsets). Harmless for development; silence by
  adding a `GameFeatureData` PrimaryAssetTypesToScan entry if it becomes noisy.
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

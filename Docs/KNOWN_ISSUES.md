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

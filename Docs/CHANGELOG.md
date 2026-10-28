# CHANGELOG

## 2026-10-28 — Phase 1: network spike
- `AHoldoverCharacter`: explicit `bReplicates` + `SetReplicatingMovement(true)` (standard replicated
  character movement for listen-server co-op).
- PIE defaults set to **2-player listen server** (`Config/DefaultEditorPerProjectUserSettings.ini`).
- Verified via MCP + logs: "Server logged in" / "Client logged in" / "Welcomed by server",
  worlds `UEDPIE_0` (host) and `UEDPIE_1` (client) up for play. Movement-see-each-other remains a
  manual test (input injection not automatable yet).
- Added `Docs/TESTING.md`, `Docs/TEST_CHECKLIST.md`, `Docs/KNOWN_ISSUES.md`, `Docs/CHANGELOG.md`.

## 2026-10-28 — Phase 0: clean project (`00-clean-project`, commit af02efe)
- UE 5.8 C++ First Person scaffold as `Holdover`: module/targets/classes renamed from `TP_FirstPerson`
  with CoreRedirects keeping template Blueprints alive; Horror/Shooter variants pruned; shared packs
  (Characters/Input/LevelPrototyping) added.
- Unreal MCP (`ModelContextProtocol` + `ToolsetRegistry` + `AllToolsets`) enabled with auto-start on
  `http://127.0.0.1:8000/mcp`; verified end-to-end (toolsets, actor reads, PIE round-trip).
- Team README (setup, branch+PR workflow, MCP wiring); collaborators dandog25 & HreaperH78 invited.

## 2026-10-27 — Repo reset
- Unity 6 URP template purged (zero game content); UE `.gitignore`/`.gitattributes` (Git LFS) added.
- `Docs/` seeded with the UE 5.8 MCP build plan and master agent prompt (playbook).

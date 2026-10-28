# HOLDOVER

Up to 4 players. Zero qualifications. A comedic online co-op game where you and your friends are the dangerously underqualified ground crew at a freezing airport, deicing planes before their holdover clock expires.

**Engine: Unreal Engine 5.8 · C++ · PC/Steam target · 2–4 player online co-op**

## Prerequisites

1. **Unreal Engine 5.8** via the [Epic Games Launcher](https://www.unrealengine.com/download) (any install path is fine — the project uses `"EngineAssociation": "5.8"`).
   - Core Editor + **Windows** target platform is enough (~40–60 GB).
2. **Visual Studio 2022** (Community is fine) with the **Game development with C++** workload and a Windows SDK ≥ 10.0.22621.
3. **Git** + **Git LFS** (`git lfs install`) — binary assets are stored in LFS.

## Getting started

```powershell
git clone https://github.com/gigad1k/Holdover.git
cd Holdover
git lfs install
git lfs pull
```

Open `Holdover.uproject` (double-click, or right-click → Switch Unreal Engine version → 5.8 if prompted). First editor launch compiles shaders — expect several minutes.

**Build from CLI (optional):**
```powershell
& "<UE_5.8>\Engine\Build\BatchFiles\Build.bat" HoldoverEditor Win64 Development -project="D:\Projects\HOLDOVER\Holdover.uproject" -waitmutex
```

## Git workflow (3-person team)

- **No direct pushes to `main`.** Branch (`feature/<name>`, `fix/<name>`) → push → open a PR → review → merge.
- Commit before each milestone and before broad AI/editor automation runs.
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` are gitignored — never commit them.

## Unreal MCP (AI editor control)

The project ships with Epic's experimental **Unreal MCP** plugin enabled (`ModelContextProtocol` + `AllToolsets`), with **Auto Start Server** on in `Config/DefaultEditorPerProjectUserSettings.ini`. When the editor is open it serves `http://127.0.0.1:8000/mcp` (streamable HTTP, loopback-only, no auth — never expose this port).

To let an MCP-capable agent (MiMoCode, Claude Code, Cursor…) drive the editor, point it at that URL. MiMoCode config example (`~/.config/mimocode/mimocode.jsonc`):

```jsonc
"mcp": {
  "unreal": { "type": "remote", "url": "http://127.0.0.1:8000/mcp", "oauth": false }
}
```

MCP calls execute serially on the game thread — don't issue overlapping mutations.

## Project docs

- `Docs/HOLDOVER_UE5.8_MCP_BUILD_PLAN.md` — the full staged build plan (phases, gameplay rules, test checklist, 30-day sprint).
- `Docs/HOLDOVER_MASTER_AGENT_PROMPT.md` — agent working agreement (hard rules: **no AngelScript**, server-authoritative C++, never claim untested results).
- `Docs/START_HERE.txt` — quick start summary.

## Layout

```
Holdover.uproject
Source/Holdover/          C++ gameplay module (HoldoverCharacter/GameMode/PlayerController/…)
Content/FirstPerson/      Template First Person content (to be replaced deliberately per the plan)
Content/Characters|Input|LevelPrototyping/   Shared template packs
Config/                   Project settings + CoreRedirects (module renamed from TP_FirstPerson)
Docs/                     Build plan and agent docs
```

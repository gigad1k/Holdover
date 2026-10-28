# TESTING — host & client launch recipes

## A. PIE two-player listen server (primary local test)

Project defaults are already configured (in `Config/DefaultEditorPerProjectUserSettings.ini`):
`PlayNetMode=PIE_ListenServer`, `PlayNumberOfClients=2`, `RunUnderOneProcess=True`.

1. Open `Holdover.uproject` in UE 5.8.
2. Press **Play** (Alt+P). Two game windows open: player 1 is the listen-server host, player 2 is a connected client.
3. In each window press **WASD + mouse**. Each window controls only its own character; you should see the other character stand/move in your window.
4. Stop with **Esc** or the editor Stop button.

Expected log lines (Output Log, category `LogPlayLevel` / `LogNet`):
```
PIE: Server logged in
PIE: Client logged in
LogNet: NotifyAcceptingChannel Control 0 server World /Game/FirstPerson/UEDPIE_0_Lvl_FirstPerson.Lvl_FirstPerson: Accepted
LogNet: Welcomed by server (Level: /Game/FirstPerson/UEDPIE_0_Lvl_FirstPerson, ...)
Bringing World /Game/FirstPerson/UEDPIE_1_Lvl_FirstPerson.Lvl_FirstPerson up for play
```

**Per-machine note:** if PIE starts with one window, your user settings override the defaults — set the same three values in
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` under `[/Script/UnrealEd.LevelEditorPlaySettings]`, or via
Editor Preferences → Play. (`Saved/` is gitignored, so this is per-developer.)

## B. Two standalone processes (closer to shipping)

```powershell
# Terminal 1 — listen server host
& "D:\Gamez\EPIC\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Projects\HOLDOVER\Holdover.uproject" -game -server -log

# Terminal 2 — client (use the host's IP; 127.0.0.1 on the same machine)
& "D:\Gamez\EPIC\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Projects\HOLDOVER\Holdover.uproject" -game 127.0.0.1 -log
```
(Adjust engine path per machine. Once packaged: `Holdover.exe -server` / `Holdover.exe 127.0.0.1`.)

## C. What automation can and cannot do today

MCP (`EditorToolset.EditorAppToolset`) can **StartPIE / StopPIE / IsPIERunning** and read the log
(`EditorToolset.LogsToolset.GetLogEntries`) — enough to prove connection/handshake. It **cannot** inject
player input, so "each client sees the other's movement" is a **manual test** (step A3) until we add a
custom toolset for input injection.

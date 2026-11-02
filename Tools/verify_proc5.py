"""PROC-5 verification: plant an untagged hand-placed actor, then report survival.

Step 1 (-mode=plant): load the map, spawn an untagged StaticMeshActor labelled
  'PROC5_HumanProbe' (simulating hand-placed Phase 8 art), save.
Step 2 (-mode=check): load the map and report whether that probe survived plus an
  owner breakdown, so we can prove the greybox builder no longer deletes
  hand-placed work.

Run headless (editor closed first):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript \
      -script="<repo>/Tools/verify_proc5.py" -ScriptArgs=-mode=plant -unattended -nosplash
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapbuild_common as mbc

MAP_PATH = "/Game/Holdover/Maps/L_RampGreybox"
PROBE_LABEL = "PROC5_HumanProbe"

mode = "check"
# -ScriptArgs does not populate sys.argv in this engine build, and the flag is
# embedded as one token ("... -ScriptArgs=-mode=plant ..."), so substring-search
# the command line instead.
_cmdline = unreal.SystemLibrary.get_command_line()
_idx = _cmdline.find("-mode=")
if _idx >= 0:
    mode = _cmdline[_idx + len("-mode="):].split()[0]
unreal.log_error(f"[verify_proc5] mode={mode}")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
try:
    _eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = _eas.get_all_level_actors
    spawn = lambda cls, loc: _eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0, pitch=0, yaw=0))
except AttributeError:
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors
    spawn = lambda cls, loc: unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0, pitch=0, yaw=0))

if not les.load_level(MAP_PATH):
    raise RuntimeError(f"could not load {MAP_PATH}")

if mode == "plant":
    existing = [a for a in all_actors() if str(a.get_actor_label() or "") == PROBE_LABEL]
    for a in existing:
        a.destroy_actor()
    actor = spawn(unreal.StaticMeshActor, (3000, -3000, 100))
    actor.set_actor_label(PROBE_LABEL)
    # Deliberately left UNTAGGED - this is the hand-placed case PROC-5 is about.
    if not les.save_current_level():
        raise RuntimeError("failed to save level")
    unreal.log_error(f"[verify_proc5] planted untagged '{PROBE_LABEL}' (tags={mbc._tags(actor)})")

elif mode == "clean":
    n = 0
    for a in list(all_actors()):
        if str(a.get_actor_label() or "") == PROBE_LABEL:
            a.destroy_actor()
            n += 1
    if not les.save_current_level():
        raise RuntimeError("failed to save level")
    unreal.log_error(f"[verify_proc5] cleaned {n} probe actor(s)")

else:
    actors = list(all_actors())
    probe = [a for a in actors if str(a.get_actor_label() or "") == PROBE_LABEL]
    owners = {}
    for a in actors:
        owners[mbc.tag_of(a) or "untagged"] = owners.get(mbc.tag_of(a) or "untagged", 0) + 1
    unreal.log_error(f"[verify_proc5] actor count={len(actors)} owners={owners}")
    if probe:
        unreal.log_error(f"[verify_proc5] SURVIVED tags={mbc._tags(probe[0])} "
                         f"should_clear_by_greybox={mbc.should_clear(probe[0], 'greybox', legacy_labels=set())}")
    else:
        unreal.log_error("[verify_proc5] MISSING - hand-placed probe was destroyed (PROC-5 NOT fixed)")

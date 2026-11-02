"""Builds Content/Holdover/Maps/L_RampGreybox.umap from engine basic shapes.

Idempotent for the actors it owns: loads and clears only its own geometry and
gameplay actors, then recreates them. Saves via save_current_level.

Ownership (CODE_REVIEW PROC-5): this builder tags its actors ``builder:greybox``
and destroys only those, so hand-placed / Phase 8 art work survives. Lighting and
fog belong to ``build_ramp_atmosphere.py`` (tagged ``builder:atmosphere``) and are
only created here as a fallback when that builder has not run yet. Run order:
build_ramp_greybox.py first, then build_ramp_atmosphere.py.

Gotcha (cost us a black-screen debug): unreal.Rotator's positional constructor
is (roll, pitch, yaw) — the spawn helpers below take (pitch, yaw, roll) and map
explicitly. A sun with positive pitch points AWAY from the scene and the whole
map renders black (atmosphere computes a night sky).

Run headless (editor closed first):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<repo>/Tools/build_ramp_greybox.py" -unattended -nosplash

Actor labels are the source of truth for what each greybox element is
(playbook Phase 2: consistent labels so AI-generated objects are identifiable).
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapbuild_common as mbc

MAP_PATH = "/Game/Holdover/Maps/L_RampGreybox"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"
CONE = "/Engine/BasicShapes/Cone.Cone"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# EditorActorSubsystem is the modern API; fall back to the legacy library.
# NOTE: unreal.Rotator's positional constructor is (roll, pitch, yaw) — our
# helpers take (pitch, yaw, roll) to match UE editor conventions.
try:
    _eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    def spawn_actor(cls, loc, rot):
        return _eas.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))

    def all_actors():
        return _eas.get_all_level_actors()
except AttributeError:
    def spawn_actor(cls, loc, rot):
        return unreal.EditorLevelLibrary.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))

    def all_actors():
        return unreal.EditorLevelLibrary.get_all_level_actors()

# Idempotent rebuild of OUR actors only. Untagged actors (hand-placed art,
# atmosphere, anything not ours) are never destroyed. Untagged actors whose
# label matches what this script creates are legacy greybox output and get
# adopted into ownership so the map migrates cleanly.
GREYBOX_LABELS = {
    "RampFloor", "DeicingPad", "TestAircraft", "RefillStation",
    "TestInteractable_Refill", "DeicingTruck", "PlayerStart",
}
GREYBOX_LABEL_PREFIXES = ("MARKER_", "Hazard_", "AC_", "LightPole_", "SafetyCone_")


def _is_legacy_greybox_label(label):
    return label in GREYBOX_LABELS or label.startswith(GREYBOX_LABEL_PREFIXES)


if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not les.load_level(MAP_PATH):
        raise RuntimeError(f"could not load existing {MAP_PATH}")
    actors = list(all_actors())
    legacy = {a.get_actor_label() for a in actors
              if mbc.tag_of(a) is None and not mbc.is_system(a)
              and _is_legacy_greybox_label(str(a.get_actor_label() or ""))}
    mbc.clear_owned(actors, "greybox", legacy_labels=legacy)
else:
    if not les.new_level(MAP_PATH):
        raise RuntimeError(f"could not create {MAP_PATH}")


def spawn_mesh(label, mesh_path, loc, scale=(1, 1, 1), rot=(0, 0, 0), owner="greybox"):
    actor = spawn_actor(unreal.StaticMeshActor, loc, rot)
    mesh = unreal.load_asset(mesh_path)
    smc = actor.get_component_by_class(unreal.StaticMeshComponent)
    smc.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    mbc.claim(actor, owner)
    return actor


def spawn_class(label, cls, loc, rot=(0, 0, 0), owner="greybox"):
    actor = spawn_actor(cls, loc, rot)
    actor.set_actor_label(label)
    mbc.claim(actor, owner)
    return actor


# --- apron floor (60m x 60m, bounded and easy to test) ---
spawn_mesh("RampFloor", CUBE, (0, 0, -20), scale=(60, 60, 0.4))

# --- marked deicing pad ---
spawn_mesh("DeicingPad", CUBE, (0, 1500, 1), scale=(12, 16, 0.05))

# --- required clear locations (markers; gameplay actors land here later) ---
spawn_mesh("MARKER_IcePatch", CYL, (-1800, 800, 5), scale=(3, 3, 0.05))
spawn_mesh("MARKER_JetBlastVolume", CUBE, (0, 4200, 300), scale=(10, 4, 3))

# --- hazard volumes on the marked locations ---
ice = spawn_actor(unreal.HoldoverHazardVolume, (-1800, 800, 0), (0, 0, 0))
ice.set_actor_label("Hazard_IcePatch")
ice.set_editor_property("hazard_type", unreal.HoldoverHazardType.ICE)
ice.set_actor_scale3d(unreal.Vector(2, 2, 1))
mbc.claim(ice, "greybox")

blast = spawn_actor(unreal.HoldoverHazardVolume, (0, 4200, 0), (0, 0, 0))
blast.set_actor_label("Hazard_JetBlast")
blast.set_editor_property("hazard_type", unreal.HoldoverHazardType.JET_BLAST)
blast.set_actor_scale3d(unreal.Vector(5, 3, 2))
mbc.claim(blast, "greybox")

# --- test aircraft: one AHoldoverAircraft + four labelled surface sections ---
spawn_class("TestAircraft", unreal.HoldoverAircraft, (0, 2600, 0))


def spawn_section(label, section_name, loc, scale):
    actor = spawn_actor(unreal.HoldoverSurfaceSection, loc, (0, 0, 0))
    actor.set_actor_label(label)
    actor.set_editor_property("section_name", section_name)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    mbc.claim(actor, "greybox")
    return actor


spawn_section("AC_Nose", "Nose", (0, 3400, 150), (1.5, 1.5, 1.5))
spawn_section("AC_Fuselage", "Fuselage", (0, 2800, 150), (1.5, 3, 1.2))
spawn_section("AC_LeftWing", "LeftWing", (-450, 2800, 120), (4, 1.5, 0.3))
spawn_section("AC_RightWing", "RightWing", (450, 2800, 120), (4, 1.5, 0.3))
spawn_section("AC_Tail", "Tail", (0, 2200, 250), (1.2, 1, 1.5))

# --- refill station (interactable test object next to it) ---
spawn_mesh("RefillStation", CUBE, (2200, -1500, 120), scale=(2, 2, 1.2))
spawn_class("TestInteractable_Refill", unreal.HoldoverTestInteractable, (2200, -900, 50))

# --- deicing truck (stationary boom; driving behind a config flag) ---
truck = spawn_class("DeicingTruck", unreal.HoldoverDeicingTruck, (-2500, 1500, 100), rot=(0, 90, 0))
truck.set_actor_label("DeicingTruck")

# --- light poles at the corners ---
for i, (x, y) in enumerate([(-2600, -2600), (2600, -2600), (-2600, 2600), (2600, 2600)]):
    spawn_mesh(f"LightPole_{i}_Pole", CYL, (x, y, 300), scale=(0.35, 0.35, 6))
    spawn_mesh(f"LightPole_{i}_Head", CUBE, (x, y, 620), scale=(1.2, 1.2, 0.3))

# --- safety cones along the pad edge ---
for i in range(6):
    x = -1200 + i * 480
    spawn_mesh(f"SafetyCone_{i}", CONE, (x, -300, 30), scale=(0.6, 0.6, 0.9))

# --- player start, facing the pad ---
spawn_class("PlayerStart", unreal.PlayerStart, (0, -2200, 120), rot=(0, 90, 0))

# --- lighting fallback: only if build_ramp_atmosphere.py has not run yet ---
# These belong to the atmosphere builder (tagged builder:atmosphere) so its pass
# can retune or replace them without this script recreating duplicates. Legacy
# untagged lighting is adopted into atmosphere ownership first.
mbc.adopt_untagged_of_classes(all_actors(), "atmosphere", mbc.ATMO_CLASSES)
_have_atmo = any(mbc.is_owned_by(a, "atmosphere") for a in all_actors())
if not _have_atmo:
    # Negative pitch so the sun points DOWN at the scene.
    sun = spawn_class("Sun_Directional", unreal.DirectionalLight, (0, 0, 1200),
                      rot=(-45, 30, 0), owner="atmosphere")
    dlc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    dlc.set_intensity(6.0)
    dlc.set_editor_property("atmosphere_sun_light", True)

    sky = spawn_class("SkyLight", unreal.SkyLight, (0, 0, 1000), owner="atmosphere")
    slc = sky.get_component_by_class(unreal.SkyLightComponent)
    slc.set_intensity(1.0)
    try:
        slc.set_editor_property("real_time_capture", True)
    except Exception:
        unreal.SystemLibrary.execute_console_command(None, "r.SkyLightRealTimeCapture 1")

    spawn_class("SkyAtmosphere", unreal.SkyAtmosphere, (0, 0, 0), owner="atmosphere")
    spawn_class("HeightFog", unreal.ExponentialHeightFog, (0, 0, 0), owner="atmosphere")
else:
    unreal.log_error("[build_ramp_greybox] atmosphere-owned lighting present; leaving it alone")

if not les.save_current_level():
    raise RuntimeError("failed to save level")

unreal.log_error(
    f"[build_ramp_greybox] saved {MAP_PATH} with {len(all_actors())} actors "
    f"({mbc.count_owned(all_actors(), 'greybox')} greybox-owned)")

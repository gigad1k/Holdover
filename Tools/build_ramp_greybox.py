"""Builds Content/Holdover/Maps/L_RampGreybox.umap from engine basic shapes.

Idempotent: loads and clears the map if it exists (system/lighting actors kept
only when listed in _SYSTEM), otherwise creates it. Saves via save_current_level.

Gotcha (cost us a black-screen debug): unreal.Rotator's positional constructor
is (roll, pitch, yaw) — the spawn helpers below take (pitch, yaw, roll) and map
explicitly. A sun with positive pitch points AWAY from the scene and the whole
map renders black (atmosphere computes a night sky).

Run headless (editor closed first):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<repo>/Tools/build_ramp_greybox.py" -unattended -nosplash

Actor labels are the source of truth for what each greybox element is
(playbook Phase 2: consistent labels so AI-generated objects are identifiable).
"""

import unreal

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

# Idempotent rebuild: clear the map if it exists (system actors are kept),
# otherwise create it fresh. (delete_asset/new_level cannot overwrite cleanly.)
_SYSTEM = ("WorldSettings", "DefaultPhysicsVolume", "Brush", "AbstractNavData",
           "NavigationDataChunk", "GameplayDebuggerPlayerManager", "ChaosDebugDrawActor",
           "BuoyancyManager", "LevelBounds", "WorldDataLayers", "WorldPartitionMiniMap",
           "VolumetricCloud")

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not les.load_level(MAP_PATH):
        raise RuntimeError(f"could not load existing {MAP_PATH}")
    for actor in list(all_actors()):
        if actor.get_class().get_name() not in _SYSTEM:
            actor.destroy_actor()
else:
    if not les.new_level(MAP_PATH):
        raise RuntimeError(f"could not create {MAP_PATH}")


def spawn_mesh(label, mesh_path, loc, scale=(1, 1, 1), rot=(0, 0, 0)):
    actor = spawn_actor(unreal.StaticMeshActor, loc, rot)
    mesh = unreal.load_asset(mesh_path)
    smc = actor.get_component_by_class(unreal.StaticMeshComponent)
    smc.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    return actor


def spawn_class(label, cls, loc, rot=(0, 0, 0)):
    actor = spawn_actor(cls, loc, rot)
    actor.set_actor_label(label)
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

blast = spawn_actor(unreal.HoldoverHazardVolume, (0, 4200, 0), (0, 0, 0))
blast.set_actor_label("Hazard_JetBlast")
blast.set_editor_property("hazard_type", unreal.HoldoverHazardType.JET_BLAST)
blast.set_actor_scale3d(unreal.Vector(5, 3, 2))

# --- test aircraft: one AHoldoverAircraft + four labelled surface sections ---
spawn_class("TestAircraft", unreal.HoldoverAircraft, (0, 2600, 0))


def spawn_section(label, section_name, loc, scale):
    actor = spawn_actor(unreal.HoldoverSurfaceSection, loc, (0, 0, 0))
    actor.set_actor_label(label)
    actor.set_editor_property("section_name", section_name)
    actor.set_actor_scale3d(unreal.Vector(*scale))
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

# --- emissive lamp material (Lumen treats emissive surfaces as GI sources) ---
def _ensure_glow_material():
    path = "/Game/Holdover/Materials/M_PoleHeadGlow"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    try:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = asset_tools.create_asset("M_PoleHeadGlow", "/Game/Holdover/Materials",
                                       unreal.Material, unreal.MaterialFactoryNew())
        mel = unreal.MaterialEditingLibrary
        em = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        em.set_editor_property("const_vector", unreal.LinearColor(40.0, 24.0, 10.0, 1.0))  # HDR warm
        mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        return mat
    except Exception as e:
        unreal.log_warning(f"[build_ramp_greybox] glow material failed: {e} — lamp heads stay grey")
        return None

_glow = _ensure_glow_material()

# --- light poles at the corners: emissive head + real warm point light ---
for i, (x, y) in enumerate([(-2600, -2600), (2600, -2600), (-2600, 2600), (2600, 2600)]):
    spawn_mesh(f"LightPole_{i}_Pole", CYL, (x, y, 300), scale=(0.35, 0.35, 6))
    head = spawn_mesh(f"LightPole_{i}_Head", CUBE, (x, y, 620), scale=(1.2, 1.2, 0.3))
    if _glow:
        head.get_component_by_class(unreal.StaticMeshComponent).set_material(0, _glow)
    lamp = spawn_class(f"LightPole_{i}_Lamp", unreal.PointLight, (x, y, 580))
    plc = lamp.get_component_by_class(unreal.PointLightComponent)
    plc.set_intensity(5000.0)
    plc.set_light_color(unreal.LinearColor(1.0, 0.62, 0.28, 1.0))  # warm sodium amber
    for _n, _v in (("attenuation_radius", 2600.0), ("source_radius", 25.0)):
        try:
            plc.set_editor_property(_n, _v)
        except Exception:
            unreal.log_warning(f"[build_ramp_greybox] lamp property '{_n}' not set")

# --- safety cones along the pad edge ---
for i in range(6):
    x = -1200 + i * 480
    spawn_mesh(f"SafetyCone_{i}", CONE, (x, -300, 30), scale=(0.6, 0.6, 0.9))

# --- player start, facing the pad ---
spawn_class("PlayerStart", unreal.PlayerStart, (0, -2200, 120), rot=(0, 90, 0))

# --- lighting: cold moonlit NIGHT grade (Lumen GI stays the runtime baseline) ---
# Negative pitch so the "moon" points DOWN at the scene (see gotcha at top).
# Low intensity + cold blue color: this reads as moonlight, not daylight.
sun = spawn_class("Sun_Directional", unreal.DirectionalLight, (0, 0, 1200), rot=(-45, 30, 0))
dlc = sun.get_component_by_class(unreal.DirectionalLightComponent)
dlc.set_intensity(1.2)  # lux-scale moonlight (daylight was 6.0)
dlc.set_light_color(unreal.LinearColor(0.55, 0.68, 1.0, 1.0))  # cold blue-white
dlc.set_editor_property("atmosphere_sun_light", True)

# Dim cold skylight = Lumen's night ambient base (real-time capture feeds GI).
sky = spawn_class("SkyLight", unreal.SkyLight, (0, 0, 1000))
slc = sky.get_component_by_class(unreal.SkyLightComponent)
slc.set_intensity(0.4)
slc.set_light_color(unreal.LinearColor(0.6, 0.72, 1.0, 1.0))
try:
    slc.set_editor_property("real_time_capture", True)
except Exception:
    unreal.SystemLibrary.execute_console_command(None, "r.SkyLightRealTimeCapture 1")

spawn_class("SkyAtmosphere", unreal.SkyAtmosphere, (0, 0, 0))

# Controlled night haze: low density so gameplay stays readable, volumetric
# fog on so the lamp cones read in Lumen.
fog = spawn_class("HeightFog", unreal.ExponentialHeightFog, (0, 0, 0))
fgc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
for _n, _v in (("fog_density", 0.02),
               ("fog_inscattering_luminance", unreal.LinearColor(0.01, 0.02, 0.05, 1.0)),
               ("fog_max_opacity", 0.75),
               ("volumetric_fog", True)):
    try:
        fgc.set_editor_property(_n, _v)
    except Exception:
        unreal.log_warning(f"[build_ramp_greybox] fog property '{_n}' not set")

# --- post process: unbound night grade (exposure clamped so the ramp never
# blows out under lamp light or crushes to black in the shadows) ---
ppv = spawn_class("PPV_Night", unreal.PostProcessVolume, (0, 0, 0))
try:
    ppv.set_editor_property("unbound", True)
    ppv.set_editor_property("priority", 1.0)
except Exception:
    unreal.log_warning("[build_ramp_greybox] PPV unbound/priority not set")


def _set_pp(names, value, label):
    """Try each candidate property name until one sticks (API names vary by version)."""
    for n in names:
        try:
            _settings.set_editor_property(n, value)
            return
        except Exception:
            continue
    unreal.log_warning(f"[build_ramp_greybox] PP setting '{label}' not set")


_settings = ppv.get_editor_property("settings")
# Fixed exposure at EV ~-1.5: deterministic dark night, immune to metering swings.
_set_pp(("b_override_auto_exposure_fixed_brightness",
         "override_auto_exposure_fixed_brightness"), True, "fixed exposure")
_set_pp(("auto_exposure_fixed_brightness",), -1.5, "exposure value")
_set_pp(("b_override_white_temp", "override_white_temp"), True, "white temp")
_set_pp(("white_temp",), 8500.0, "white temp value")
try:
    ppv.set_editor_property("settings", _settings)
except Exception:
    unreal.log_warning("[build_ramp_greybox] PP settings write-back failed")

if not les.save_current_level():
    raise RuntimeError("failed to save level")

unreal.log_error(f"[build_ramp_greybox] saved {MAP_PATH} with {len(all_actors())} actors")

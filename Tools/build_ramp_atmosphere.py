"""Phase 8a atmosphere pass for L_RampGreybox: cold blue night, warm orange ramps.

Upgrades the greybox into a snowy-night airport ramp look without any purchased
art assets. Everything here is engine-light + fog + post-process + generated
materials, so it is repeatable and script-owned.

The look (art-direction note -> Docs/AI/NEEDS_ALI.md):
  * cold steel-blue moonlight and sky ambient  -> the night is genuinely cold
  * warm sodium-orange spot pools under the    -> "working at 2am" comedy warmth
    four ramp light poles
  * cold blue-grey height fog between them     -> depth + snow-sky silhouette
  * mild post-process (bloom, vignette, AO)    -> readability + cinematic read

Ownership (PROC-5): every actor this script creates is tagged
``builder:atmosphere`` and only those are cleared. Greybox geometry, gameplay
actors and hand-placed art are never touched.

Idempotent. Run AFTER build_ramp_greybox.py (see that file's header).

Run headless (editor closed first):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript \\
      -script="<repo>/Tools/build_ramp_atmosphere.py" -unattended -nosplash

NOTE: values below are reasoned defaults, not a human-approved art direction.
They need an eyeball pass in the editor - see Docs/AI/NEEDS_ALI.md.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapbuild_common as mbc

MAP_PATH = "/Game/Holdover/Maps/L_RampGreybox"

# --- palette (linear-ish RGB, alpha ignored) ---
# Cold steel-blue moonlight and sky fill.
MOON_RGB = (0.52, 0.66, 1.00)
SKY_RGB = (0.42, 0.56, 0.92)
FOG_RGB = (0.30, 0.40, 0.58)
# Warm sodium-orange ramp working lights (the comedy warmth).
RAMP_RGB = (1.00, 0.58, 0.24)

# The four light-pole positions from build_ramp_greybox.py (pole heads at z=620).
RAMP_LIGHTS = [(-2600, -2600), (2600, -2600), (-2600, 2600), (2600, 2600)]
RAMP_LIGHT_Z = 612


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
try:
    _eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = _eas.get_all_level_actors

    def spawn_actor(cls, loc, rot):
        return _eas.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
except AttributeError:
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors

    def spawn_actor(cls, loc, rot):
        return unreal.EditorLevelLibrary.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))


if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError(f"{MAP_PATH} missing - run Tools/build_ramp_greybox.py first")
if not les.load_level(MAP_PATH):
    raise RuntimeError(f"could not load {MAP_PATH}")


def set_prop(obj, name, value, log_fail=True):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:
        if log_fail:
            unreal.log_error(f"[build_ramp_atmosphere] set {name}: FAIL {e}")
        return False


def set_light_color(component, rgb):
    """LightColor is FColor on light components; set_light_color() takes FLinearColor."""
    try:
        component.set_light_color(unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return True
    except Exception:
        try:
            component.set_editor_property(
                "light_color",
                unreal.Color(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255), 255))
            return True
        except Exception as e:
            unreal.log_error(f"[build_ramp_atmosphere] light color: FAIL {e}")
            return False


def pp_set(actor, name, value):
    """Set an FPostProcessSettings field on a PostProcessVolume plus its bOverride_ flag.

    PostProcessVolume keeps Settings/Unbound/BlendRadius on the actor itself - it
    has no PostProcessComponent (verified via Tools/probe_pp_names.py).
    """
    try:
        settings = actor.get_editor_property("settings")
        settings.set_editor_property(name, value)
        try:
            settings.set_editor_property(f"override_{name}", True)
        except Exception:
            pass
        actor.set_editor_property("settings", settings)
        return True
    except Exception as e:
        unreal.log_error(f"[build_ramp_atmosphere] pp {name}: FAIL {e}")
        return False


def spawn_class(label, cls, loc, rot=(0, 0, 0)):
    actor = spawn_actor(cls, loc, rot)
    actor.set_actor_label(label)
    mbc.claim(actor, "atmosphere")
    return actor


# ---------------------------------------------------------------------------
# Clear our own previous pass, and adopt any legacy untagged lighting so the
# pre-PROC-5 map migrates instead of accumulating duplicates.
# ---------------------------------------------------------------------------
actors = list(all_actors())
mbc.adopt_untagged_of_classes(actors, "atmosphere", mbc.ATMO_CLASSES)
cleared = mbc.clear_owned(all_actors(), "atmosphere")
unreal.log_error(f"[build_ramp_atmosphere] cleared {cleared} atmosphere actor(s)")

# ---------------------------------------------------------------------------
# 1. Moonlight - cold steel-blue, low angle for long raking shadows.
#    Negative pitch points DOWN at the scene (see build_ramp_greybox.py gotcha).
# ---------------------------------------------------------------------------
moon = spawn_class("ATMO_Moon", unreal.DirectionalLight, (0, 0, 1200), rot=(-28, 38, 0))
dlc = moon.get_component_by_class(unreal.DirectionalLightComponent)
set_prop(dlc, "intensity", 3.0)
set_light_color(dlc, MOON_RGB)
set_prop(dlc, "atmosphere_sun_light", True)
set_prop(dlc, "cast_shadows", True)
set_prop(dlc, "light_source_angle", 1.5)

# ---------------------------------------------------------------------------
# 2. Sky ambient fill - cool, low, so shadows read blue rather than black.
# ---------------------------------------------------------------------------
sky = spawn_class("ATMO_SkyLight", unreal.SkyLight, (0, 0, 1000))
slc = sky.get_component_by_class(unreal.SkyLightComponent)
set_prop(slc, "intensity", 0.8)
set_light_color(slc, SKY_RGB)
try:
    set_prop(slc, "real_time_capture", True)
except Exception:
    unreal.SystemLibrary.execute_console_command(None, "r.SkyLightRealTimeCapture 1")
set_prop(slc, "cast_shadows", False)

spawn_class("ATMO_Sky", unreal.SkyAtmosphere, (0, 0, 0))

# ---------------------------------------------------------------------------
# 3. Cold height fog - snow-sky silhouette, light shafts from the ramp lamps.
# ---------------------------------------------------------------------------
fog = spawn_class("ATMO_Fog", unreal.ExponentialHeightFog, (0, 0, 0))
fgc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
set_prop(fgc, "fog_density", 0.03)
set_prop(fgc, "fog_height_falloff", 0.18)
set_prop(fgc, "fog_max_opacity", 0.92)
set_prop(fgc, "start_distance", 600.0)
# Property names verified via Tools/probe_atmo_names.py in UE 5.8: the volumetric
# toggle is 'enable_volumetric_fog', and the inscattering colours are set through
# the set_*_color() setters (the fields themselves are *_luminance).
set_prop(fgc, "enable_volumetric_fog", True)
set_prop(fgc, "volumetric_fog_extinction_scale", 1.4)
set_prop(fgc, "volumetric_fog_scattering_distribution", 0.7)
set_prop(fgc, "volumetric_fog_start_distance", 300.0)
try:
    fgc.set_fog_inscattering_color(unreal.LinearColor(FOG_RGB[0], FOG_RGB[1], FOG_RGB[2], 1.0))
except Exception as e:
    unreal.log_error(f"[build_ramp_atmosphere] fog inscattering color: FAIL {e}")
# Directional inscattering gives the moon a cold halo in the mist.
try:
    fgc.set_directional_inscattering_color(unreal.LinearColor(0.42, 0.54, 0.82, 1.0))
    set_prop(fgc, "directional_inscattering_exponent", 6.0)
    set_prop(fgc, "directional_inscattering_start_distance", 2000.0)
except Exception as e:
    unreal.log_error(f"[build_ramp_atmosphere] directional inscattering: FAIL {e}")

# ---------------------------------------------------------------------------
# 4. Warm sodium-orange ramp working lights under the four light poles.
#    These are the warmth against the cold - key to the comedy contrast.
# ---------------------------------------------------------------------------
for i, (x, y) in enumerate(RAMP_LIGHTS):
    spot = spawn_class(f"ATMO_RampLight_{i}", unreal.SpotLight, (x, y, RAMP_LIGHT_Z),
                       rot=(-90, 0, 0))  # pitch -90 = straight down
    plc = spot.get_component_by_class(unreal.SpotLightComponent)
    set_prop(plc, "intensity", 9000.0)
    set_light_color(plc, RAMP_RGB)
    set_prop(plc, "attenuation_radius", 4200.0)
    set_prop(plc, "inner_cone_angle", 34.0)
    set_prop(plc, "outer_cone_angle", 66.0)
    set_prop(plc, "source_radius", 22.0)
    set_prop(plc, "cast_shadows", True)
    try:
        set_prop(plc, "intensity_units", unreal.LightUnits.CANDELA)
    except Exception:
        pass

# ---------------------------------------------------------------------------
# 5. Mild post-process - readability + cinematic read. Contrast comes from the
#    light colours above, not from a heavy screen grade.
# ---------------------------------------------------------------------------
pp = spawn_class("ATMO_PostProcess", unreal.PostProcessVolume, (0, 0, 0))
set_prop(pp, "unbound", True)
set_prop(pp, "blend_radius", 0.0)
set_prop(pp, "blend_weight", 1.0)
set_prop(pp, "priority", 1.0)

# Auto-exposure keeps the dark scene playable across dark/warm zones.
pp_set(pp, "auto_exposure_min_brightness", 0.55)
pp_set(pp, "auto_exposure_max_brightness", 1.85)
pp_set(pp, "auto_exposure_speed_up", 1.2)
pp_set(pp, "auto_exposure_speed_down", 0.7)
# Warm lamps bloom; the cold night stays restrained.
pp_set(pp, "bloom_intensity", 0.75)
pp_set(pp, "bloom_threshold", 0.85)
pp_set(pp, "vignette_intensity", 0.28)
pp_set(pp, "ambient_occlusion_intensity", 0.55)
pp_set(pp, "ambient_occlusion_radius", 120.0)
# Slight cool desaturation: pull red back, let blue breathe.
# ColorSaturation/ColorContrast are FVector4 in UE5, not FLinearColor.
pp_set(pp, "color_saturation", unreal.Vector4(0.90, 0.94, 1.06, 1.0))
pp_set(pp, "color_contrast", unreal.Vector4(1.04, 1.04, 1.04, 1.0))

if not les.save_current_level():
    raise RuntimeError("failed to save level")

# --- read-back verification: prove the look settings actually persisted ---
# We cannot see the render headless, so at minimum confirm every value landed.
def read_back():
    out = {}
    for actor in all_actors():
        label = str(actor.get_actor_label() or "")
        cls = actor.get_class().get_name()
        if label == "ATMO_Moon":
            c = actor.get_component_by_class(unreal.DirectionalLightComponent)
            out["moon_intensity"] = c.get_editor_property("intensity")
            out["moon_light_color"] = str(c.get_light_color())
        elif label == "ATMO_Fog":
            c = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            out["fog_density"] = c.get_editor_property("fog_density")
            out["fog_volumetric"] = c.get_editor_property("enable_volumetric_fog")
            out["fog_inscat"] = str(c.get_editor_property("fog_inscattering_luminance"))
        elif label == "ATMO_RampLight_0":
            c = actor.get_component_by_class(unreal.SpotLightComponent)
            out["ramp_intensity"] = c.get_editor_property("intensity")
            out["ramp_inner_cone"] = c.get_editor_property("inner_cone_angle")
        elif label == "ATMO_PostProcess":
            s = actor.get_editor_property("settings")
            out["pp_unbound"] = actor.get_editor_property("unbound")
            out["pp_bloom"] = s.get_editor_property("bloom_intensity")
            out["pp_bloom_override"] = s.get_editor_property("override_bloom_intensity")
            out["pp_vignette"] = s.get_editor_property("vignette_intensity")
            out["pp_ao"] = s.get_editor_property("ambient_occlusion_intensity")
            out["pp_saturation"] = str(s.get_editor_property("color_saturation"))
            out["pp_contrast"] = str(s.get_editor_property("color_contrast"))
            out["pp_exp_min"] = s.get_editor_property("auto_exposure_min_brightness")
            out["pp_exp_max"] = s.get_editor_property("auto_exposure_max_brightness")
    return out


unreal.log_error(f"[build_ramp_atmosphere] READBACK {read_back()}")
unreal.log_error(
    f"[build_ramp_atmosphere] saved {MAP_PATH} with {len(all_actors())} actors "
    f"({mbc.count_owned(all_actors(), 'atmosphere')} atmosphere-owned)")

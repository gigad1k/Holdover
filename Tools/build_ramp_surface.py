"""Phase 8a surface detail for L_RampGreybox: wet/icy ground + painted taxi lines.

Generates simple materials from scratch (no purchased textures) and applies them
to the greybox apron, then lays painted taxi lines as decals. Everything is
script-owned (``builder:surface``) so the greybox and atmosphere passes never
clobber it and it never clobbers hand-placed art (PROC-5).

Look intent (see Docs/AI/NEEDS_ALI.md for the eyeball pass):
  * wet cold apron  - dark blue-grey, low roughness  => the ramp is frozen wet
  * ice / deicing pad - bright cold white-blue, glassy => readable + comedic risk
  * taxi lines      - warm orange-yellow, faint glow  => readable in the dark

Idempotent. Run AFTER build_ramp_greybox.py and build_ramp_atmosphere.py.

Run headless (editor closed first):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript \\
      -script="<repo>/Tools/build_ramp_surface.py" -unattended -nosplash
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapbuild_common as mbc

MAP_PATH = "/Game/Holdover/Maps/L_RampGreybox"
MAT_DIR = "/Game/Holdover/Materials"

# --- material recipes: (base rgb, roughness, metallic, emissive rgb or None) ---
RECIPES = {
    "M_ApronWet": ((0.055, 0.075, 0.115), 0.22, 0.0, None),
    "M_Ice": ((0.62, 0.72, 0.88), 0.06, 0.0, None),
    "M_TaxiLine": ((1.00, 0.62, 0.12), 0.45, 0.0, (0.38, 0.19, 0.04)),
}

# Which greybox actors get which material.
APPLY_TO = {
    "M_ApronWet": ("RampFloor",),
    "M_Ice": ("DeicingPad", "MARKER_IcePatch"),
}

# Painted taxi lines as floor decals: (label, loc, rot(pitch,yaw,roll), size(depth,width,height)).
# Decals project along the actor's +X, so pitch -90 points them at the floor.
TAXI_LINES = (
    ("ATMO_Decal_Centerline", (0, 100, 8), (-90, 0, 0), (40, 130, 2300)),
    ("ATMO_Decal_StopLine", (0, -400, 8), (-90, 90, 0), (40, 2400, 130)),
    ("ATMO_Decal_PadEdge_L", (-700, 1500, 8), (-90, 0, 0), (40, 130, 800)),
    ("ATMO_Decal_PadEdge_R", (700, 1500, 8), (-90, 0, 0), (40, 130, 800)),
    ("ATMO_Decal_PadStop", (0, 650, 8), (-90, 90, 0), (40, 1500, 130)),
)


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


def log(msg):
    unreal.log_error(f"[build_ramp_surface] {msg}")


def ensure_material(name, base_rgb, roughness, metallic, emissive_rgb):
    """Create or update a simple surface/decal material. Returns the material asset."""
    path = f"{MAT_DIR}/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if existing is None:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        if mat is None:
            raise RuntimeError(f"could not create material {path}")
        log(f"created {path}")
    else:
        mat = existing
        log(f"reusing {path}")

    # Clear any previous expressions so re-runs stay deterministic.
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    except Exception as e:
        log(f"delete_all_material_expressions: {e}")

    def const3(rgb, x, y):
        e = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, x, y)
        e.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return e

    def const1(val, x, y):
        e = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant, x, y)
        e.set_editor_property("r", val)
        return e

    # Material inputs are reached via connect_material_property() + EMaterialProperty,
    # not connect_material_expressions() (which is expression->expression only).
    # Enum names verified via Tools/probe_mat_api.py in UE 5.8.
    PROP = {
        "BaseColor": unreal.MaterialProperty.MP_BASE_COLOR,
        "Roughness": unreal.MaterialProperty.MP_ROUGHNESS,
        "Metallic": unreal.MaterialProperty.MP_METALLIC,
        "EmissiveColor": unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    }

    def connect(expr, input_name):
        unreal.MaterialEditingLibrary.connect_material_property(expr, "", PROP[input_name])

    try:
        connect(const3(base_rgb, -300, 0), "BaseColor")
        connect(const1(roughness, -300, 180), "Roughness")
        connect(const1(metallic, -300, 320), "Metallic")
        if emissive_rgb is not None:
            connect(const3(emissive_rgb, -300, 460), "EmissiveColor")
    except Exception as e:
        log(f"connect: FAIL {e}")

    # Taxi lines are deferred decals; the domain/blend names verified via
    # Tools/probe_mat_enums.py in UE 5.8 (MD_DEFERRED_DECAL, not MD_DECAL).
    if name == "M_TaxiLine":
        for prop, val in (("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL),
                          ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)):
            try:
                mat.set_editor_property(prop, val)
            except Exception as e:
                log(f"decal {prop}: FAIL {e}")

    unreal.MaterialEditingLibrary.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_asset(path):
        log(f"save_asset {path} returned False")
    return mat


def apply_material(label, mat):
    """Set material slot 0 on the StaticMeshActor with the given label."""
    for actor in all_actors():
        if str(actor.get_actor_label() or "") != label:
            continue
        smc = actor.get_component_by_class(unreal.StaticMeshComponent)
        if smc is None:
            log(f"apply {label}: no StaticMeshComponent")
            return False
        smc.set_material(0, mat)
        return True
    log(f"apply {label}: actor not found")
    return False


# ---------------------------------------------------------------------------
# 1. Materials
# ---------------------------------------------------------------------------
mats = {}
for name, (base, rough, metal, emissive) in RECIPES.items():
    mats[name] = ensure_material(name, base, rough, metal, emissive)

# ---------------------------------------------------------------------------
# 2. Apply to the greybox apron / pad / ice marker
# ---------------------------------------------------------------------------
for mat_name, labels in APPLY_TO.items():
    for label in labels:
        apply_material(label, mats[mat_name])

# ---------------------------------------------------------------------------
# 3. Painted taxi lines (decals, owned by this builder)
# ---------------------------------------------------------------------------
# Decals are surface detail, so they share the surface tag even though the label
# prefix is ATMO_ for historical reasons.
for label, loc, rot, size in TAXI_LINES:
    # Drop any previous copy so re-runs are idempotent.
    for actor in list(all_actors()):
        if str(actor.get_actor_label() or "") == label:
            actor.destroy_actor()
    dec = spawn_actor(unreal.DecalActor, loc, rot)
    dec.set_actor_label(label)
    mbc.claim(dec, "surface")
    dcomp = dec.get_component_by_class(unreal.DecalComponent)
    if dcomp is not None:
        try:
            dcomp.set_editor_property("decal_size", unreal.Vector(*size))
        except Exception as e:
            log(f"{label} decal_size: FAIL {e}")
        # DecalComponent exposes set_decal_material()/decal_material, not
        # set_material()/material (verified via Tools/probe_decal_api.py).
        try:
            dcomp.set_decal_material(mats["M_TaxiLine"])
        except Exception as e:
            log(f"{label} set_decal_material: FAIL {e}")
        try:
            dcomp.set_editor_property("sort_order", 1)
        except Exception:
            pass
    else:
        log(f"{label}: no DecalComponent")

if not les.save_current_level():
    raise RuntimeError("failed to save level")

# --- read-back verification ---
rb = {}
for name, mat in mats.items():
    rb[name] = {
        "domain": str(mat.get_editor_property("material_domain")),
        "blend": str(mat.get_editor_property("blend_mode")),
    }
dec_rb = []
for actor in all_actors():
    label = str(actor.get_actor_label() or "")
    if label.startswith("ATMO_Decal_"):
        d = actor.get_component_by_class(unreal.DecalComponent)
        dec_rb.append({
            "label": label,
            "size": str(d.get_editor_property("decal_size")) if d else None,
            "mat": str(d.get_editor_property("decal_material")) if d else None,
        })
log(f"READBACK materials={rb}")
log(f"READBACK decals={dec_rb}")
log(f"saved {MAP_PATH} with {len(all_actors())} actors "
    f"({mbc.count_owned(all_actors(), 'surface')} surface-owned)")

"""Shared actor-ownership helpers for HOLDOVER map builders.

Why this exists (CODE_REVIEW PROC-5): `build_ramp_greybox.py` used to clear every
non-system actor on each run, which deleted hand-placed work and made the Phase 8
art pass impossible. Builders now own their actors explicitly.

Convention
----------
* A builder tags every actor it creates with ``builder:<name>`` (e.g.
  ``builder:greybox``, ``builder:atmosphere``).
* A builder's clear step destroys **only** actors it owns. Untagged actors are
  never destroyed, so hand-placed / human / future art work survives any builder
  run.
* Legacy actors created before this convention are adopted by label prefix or
  class during a builder's clear step, so the existing map migrates cleanly.

Import from a sibling script (run via ``-run=pythonscript``)::

    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import mapbuild_common as mbc
"""

TAG_PREFIX = "builder:"

# Actor classes owned by the atmosphere pass. Shared so the greybox builder can
# migrate legacy untagged lighting into atmosphere ownership instead of
# duplicating it (the pre-PROC-5 map has these untagged).
ATMO_CLASSES = (
    "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
    "PostProcessVolume", "DecalActor", "PointLight", "SpotLight",
    "VolumetricCloud", "NiagaraActor",
)


def _tags(actor):
    try:
        return list(actor.get_editor_property("tags") or [])
    except Exception:
        return []


def _set_tags(actor, tags):
    try:
        actor.set_editor_property("tags", tags)
        return True
    except Exception:
        # Fall back to the reflected member if the property setter is unavailable.
        try:
            actor.tags = tags
            return True
        except Exception:
            return False


def tag_of(actor):
    """Return the builder tag of ``actor`` (e.g. 'greybox') or None."""
    for t in _tags(actor):
        s = str(t)
        if s.startswith(TAG_PREFIX):
            return s[len(TAG_PREFIX):]
    return None


def is_owned_by(actor, owner):
    return tag_of(actor) == owner


def claim(actor, owner):
    """Tag ``actor`` as owned by builder ``owner``. Idempotent."""
    tags = [t for t in _tags(actor) if not str(t).startswith(TAG_PREFIX)]
    tags.append(TAG_PREFIX + owner)
    _set_tags(actor, tags)
    return actor


def is_system(actor):
    """True for world/system actors that must never be destroyed."""
    name = actor.get_class().get_name()
    return name in (
        "WorldSettings", "DefaultPhysicsVolume", "Brush", "AbstractNavData",
        "NavigationDataChunk", "GameplayDebuggerPlayerManager", "ChaosDebugDrawActor",
        "BuoyancyManager", "LevelBounds", "WorldDataLayers", "WorldPartitionMiniMap",
    )


def should_clear(actor, owner, legacy_labels=(), legacy_classes=()):
    """True if this builder's clear step may destroy ``actor``.

    Owned actors always clear. Untagged, non-system actors clear only when they
    match a legacy label or legacy class the builder is migrating into ownership.
    """
    if is_system(actor):
        return False
    owner_tag = tag_of(actor)
    if owner_tag is not None:
        return owner_tag == owner
    label = str(actor.get_actor_label() or "")
    if legacy_labels and label in legacy_labels:
        return True
    if legacy_classes and actor.get_class().get_name() in legacy_classes:
        return True
    return False


def clear_owned(all_actors, owner, legacy_labels=(), legacy_classes=()):
    """Destroy actors owned by ``owner`` (plus migratable legacy ones).

    Returns the number of actors destroyed.
    """
    n = 0
    for actor in list(all_actors):
        if should_clear(actor, owner, legacy_labels, legacy_classes):
            actor.destroy_actor()
            n += 1
    return n


def count_owned(all_actors, owner):
    return sum(1 for a in all_actors if is_owned_by(a, owner))


def adopt_untagged_of_classes(all_actors, owner, classes):
    """Tag untagged, non-system actors of ``classes`` as owned by ``owner``.

    Used to migrate pre-ownership map content. Returns the number adopted.
    """
    n = 0
    for actor in list(all_actors):
        if is_system(actor) or tag_of(actor) is not None:
            continue
        if actor.get_class().get_name() in classes:
            claim(actor, owner)
            n += 1
    return n

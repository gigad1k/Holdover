# Copyright Epic Games, Inc. All Rights Reserved.
"""Generate and import placeholder audio for HOLDOVER (Phase 7).

Run headless (editor closed):
  "<UE_ROOT>\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
    "D:\\Projects\\HOLDOVER\\Holdover.uproject" -run=pythonscript ^
    -script="D:\\Projects\\HOLDOVER\\Tools\\build_placeholder_audio.py" -unattended -nosplash

Creates deterministic 16-bit mono WAVs with the stdlib only and imports them as
USoundWave assets at /Game/Holdover/Audio/<Name>. Idempotent (replace_existing).
Cues: radio_beep, spray_hiss (loop), wind_loop (loop), collision_cue.
Licence: generated procedurally by this script — free to use in the game.
"""

import array
import math
import os
import random
import tempfile
import wave

import unreal

SAMPLE_RATE = 44100
DEST_PATH = "/Game/Holdover/Audio"
LOOPED = ("spray_hiss", "wind_loop")


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        ints = array.array("h", (int(max(-1.0, min(1.0, s)) * 32767) for s in samples))
        w.writeframes(ints.tobytes())


def normalize(samples, peak=0.8):
    m = max((abs(s) for s in samples), default=1.0) or 1.0
    return [s * peak / m for s in samples]


def detrend(samples):
    """Remove linear drift so loop endpoints meet without a DC pop.

    Subtracts the ramp from samples[0] to samples[-1], pinning BOTH endpoints
    to zero (P7-4: the old form subtracted samples[-1] * i/(n-1) only, leaving
    the first sample nonzero and clicking at the loop seam).
    """
    n = len(samples)
    if n < 2:
        return samples
    first = samples[0]
    step = (samples[-1] - first) / (n - 1)
    return [s - (first + step * i) for i, s in enumerate(samples)]


def gen_radio_beep():
    n = int(SAMPLE_RATE * 0.18)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t * 18.0)
        s = 0.6 * math.sin(2 * math.pi * 880 * t) + 0.2 * math.sin(2 * math.pi * 1320 * t)
        out.append(s * env)
    return normalize(out)


def gen_collision_cue():
    n = int(SAMPLE_RATE * 0.5)
    partials = [(220.0, 1.0), (347.0, 0.7), (523.0, 0.5), (781.0, 0.35), (1163.0, 0.2)]
    rng = random.Random(1234)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        s = 0.0
        for freq, amp in partials:
            s += amp * math.sin(2 * math.pi * freq * t) * math.exp(-t * (6.0 + freq / 300.0))
        if t < 0.03:
            s += (rng.random() * 2.0 - 1.0) * 0.5 * (1.0 - t / 0.03)
        out.append(s * 0.45)
    return normalize(out)


def gen_spray_hiss():
    n = int(SAMPLE_RATE * 2.0)
    rng = random.Random(2026)
    out = []
    prev = 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        prev = prev * 0.6 + (rng.random() * 2.0 - 1.0) * 0.4
        wobble = 0.85 + 0.15 * math.sin(2 * math.pi * 0.7 * t)
        out.append(prev * wobble)
    return normalize(detrend(out), peak=0.55)


def gen_wind_loop():
    n = int(SAMPLE_RATE * 4.0)
    rng = random.Random(555)
    out = []
    brown = 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        brown = (brown + (rng.random() * 2.0 - 1.0) * 0.02) * 0.998
        lfo = 0.7 + 0.3 * math.sin(2 * math.pi * 0.13 * t)
        out.append(brown * lfo)
    return normalize(detrend(out), peak=0.5)


def import_wavs(wav_paths):
    tasks = []
    for path in wav_paths:
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = DEST_PATH
        task.automated = True
        task.save = True
        task.replace_existing = True
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)


def set_looping(name):
    asset_path = f"{DEST_PATH}/{name}.{name}"
    sound = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not sound:
        raise RuntimeError(f"import missing: {asset_path}")
    # USoundWave::bLooping is exposed to Python as 'looping'.
    candidates = ("looping", "bLooping")
    for prop in candidates:
        try:
            sound.set_editor_property(prop, True)
            unreal.EditorAssetLibrary.save_loaded_asset(sound)
            return prop
        except Exception:
            continue
    raise RuntimeError(f"could not set looping on {asset_path}")


def main():
    generators = {
        "radio_beep": gen_radio_beep,
        "spray_hiss": gen_spray_hiss,
        "wind_loop": gen_wind_loop,
        "collision_cue": gen_collision_cue,
    }

    wav_dir = os.path.join(tempfile.gettempdir(), "holdover_placeholder_audio")
    os.makedirs(wav_dir, exist_ok=True)

    wav_paths = []
    for name, gen in generators.items():
        samples = gen()
        if name in LOOPED:
            first, last = samples[0], samples[-1]
            if abs(first) > 1e-4 or abs(last) > 1e-4:
                raise RuntimeError(f"{name} loop seam not zero: first={first} last={last}")
            unreal.log(f"[build_placeholder_audio] {name} seam ok (first={first:.6f} last={last:.6f})")
        path = os.path.join(wav_dir, f"{name}.wav")
        write_wav(path, samples)
        wav_paths.append(path)
        unreal.log(f"[build_placeholder_audio] wrote {path}")

    import_wavs(wav_paths)

    for name in generators:
        asset_path = f"{DEST_PATH}/{name}.{name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError(f"import failed: {asset_path}")
        unreal.log(f"[build_placeholder_audio] imported {asset_path}")

    for name in LOOPED:
        prop = set_looping(name)
        unreal.log(f"[build_placeholder_audio] {name} looping=True (via '{prop}')")

    unreal.log("[build_placeholder_audio] DONE")


if __name__ == "__main__":
    main()

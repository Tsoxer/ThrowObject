"""Stage the six original HKX clips and the state-gated OAR carry replacements."""
import argparse
import json
from pathlib import Path
import shutil
import struct

NAMES = ("PickUp", "Hold", "Throw", "CarryWalk", "CarryJog", "CarrySprint")
OAR_PATH = Path("meshes/actors/character/animations/OpenAnimationReplacer/ThrowObject")
CARRY_PRIORITY = (1 << 31) - 1
ROOT = Path(__file__).resolve().parents[1]
DIRECTIONS = (
    "forward", "forwardleft", "forwardright", "backward", "backwardleft",
    "backwardright", "left", "right", "leftfast", "rightfast",
    "leftslow", "rightslow",
)


def carry_slots():
    yield "mt_idle.hkx", "Hold"
    yield "mt_sprintforward.hkx", "CarrySprint"
    for direction in DIRECTIONS:
        yield f"mt_walk{direction}.hkx", "CarryWalk"
        yield f"mt_run{direction}.hkx", "CarryJog"


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def stage(exports, mod):
    sources = {name: exports / f"ThrowObject_{name}.hkx" for name in NAMES}
    for name, path in sources.items():
        data = path.read_bytes()
        if len(data) < 64 or struct.unpack_from("<II", data) != (0x57E0E057, 0x10C0C010) or data[16] != 8:
            raise ValueError(f"Expected x64 Havok packfile for {name}: {path}")
        marker = {"PickUp": b"ThrowObjectGrab", "Throw": b"ThrowObjectRelease"}.get(name)
        if marker and marker + b"\0" not in data:
            raise ValueError(f"Missing {marker.decode()} in {path}")

    base = mod / "meshes/actors/character/animations"
    originals = base / "ThrowObject"
    replacer = mod / OAR_PATH
    carry = replacer / "Carry"
    originals.mkdir(parents=True, exist_ok=True)
    carry.mkdir(parents=True, exist_ok=True)
    write_json(replacer / "config.json", {
        "name": "Throw Object",
        "author": "Throw Object",
        "description": "The player's third-person carry cycles while Throw Object owns a world reference.",
    })
    write_json(carry / "config.json", {
        "name": "Carry",
        "description": "Active only with a Throw Object reference in the player's hand.",
        "priority": CARRY_PRIORITY,
        "interruptible": True,
        "hasCustomBlendTimeOnInterrupt": True,
        "blendTimeOnInterrupt": 0.10,
        "replaceOnLoop": True,
        "replaceOnEcho": True,
        "conditions": [
            {"condition": "IsForm", "requiredVersion": "1.0.0.0",
             "Form": {"pluginName": "Skyrim.esm", "formID": "000014"}},
            {"condition": "CompareValues", "requiredVersion": "1.0.0.0",
             "Value A": {"form": {"pluginName": "ThrowObject.esm", "formID": "000800"}},
             "Comparison": "==", "Value B": {"value": 1.0}},
        ],
    })
    for path in sources.values():
        shutil.copyfile(path, originals / path.name)
    for directory in (carry, carry / "female", carry / "male"):
        directory.mkdir(exist_ok=True)
        for slot, name in carry_slots():
            shutil.copyfile(sources[name], directory / slot)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exports", type=Path)
    parser.add_argument("--mod", type=Path, default=ROOT / "mod")
    args = parser.parse_args()
    stage(args.exports, args.mod)

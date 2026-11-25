"""Report competing equal or higher OAR priorities for Throw Object clip paths."""
import argparse
import json
from pathlib import Path

from stage_animations import CARRY_PRIORITY, OAR_PATH


def config(folder):
    """OAR loads a user configuration in preference to the author defaults."""
    for name in ("user.json", "config.json"):
        path = folder / name
        if path.is_file():
            try:
                data = json.loads(path.read_text(encoding="utf-8-sig"))
            except (ValueError, OSError) as error:
                print(f"Could not read {path}: {error}")
                continue
            if isinstance(data, dict):
                return data
    return {}


def animations(folder):
    paths = set()
    for path in folder.rglob("*"):
        if path.suffix.lower() != ".hkx" or not path.is_file():
            continue
        names = list(path.relative_to(folder).parts)
        for i, part in enumerate(names[:-1]):
            if part.lower().startswith("_variants_"):
                names = [*names[:i], part[10:] + ".hkx"]
                break
        paths.add("/".join(names).lower())
    return paths


def conflicts(data_root):
    carry = data_root / OAR_PATH / "Carry"
    own = config(carry)
    priority = own.get("priority", CARRY_PRIORITY)
    if type(priority) is not int or not -(1 << 31) <= priority <= CARRY_PRIORITY:
        raise ValueError("Throw Object's Carry priority is outside OAR's signed 32-bit range.")
    if own.get("disabled") or not animations(carry):
        raise ValueError("Throw Object's Carry submod is missing or disabled.")
    paths = animations(carry)
    replacers = carry.parent.parent
    legacy = replacers.parent / "DynamicAnimationReplacer/_CustomConditions"
    for parent in sorted(replacers.iterdir()):
        if not parent.is_dir() or parent == carry.parent:
            continue
        for submod in sorted(parent.iterdir()):
            if not submod.is_dir():
                continue
            options = config(submod)
            rank = options.get("priority")
            if type(rank) is int and not options.get("disabled") and priority <= rank <= CARRY_PRIORITY:
                overlap = animations(submod) & paths
                if overlap:
                    yield rank, submod, sorted(overlap)
    if legacy.is_dir():
        for submod in sorted(legacy.iterdir()):
            try:
                fallback = int(submod.name)
            except ValueError:
                continue
            options = config(submod)
            rank = options.get("priority", fallback)
            if submod.is_dir() and type(rank) is int and not options.get("disabled") and priority <= rank <= CARRY_PRIORITY:
                overlap = animations(submod) & paths
                if overlap:
                    yield rank, submod, sorted(overlap)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="Skyrim Data directory as seen by the mod manager")
    args = parser.parse_args()
    try:
        found = list(conflicts(args.data))
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")
    for rank, submod, paths in found:
        print(f"{rank}  {submod.relative_to(args.data)}")
        for path in paths:
            print(f"  {path}")
    if not found:
        print("No overlapping equal or higher OAR priorities in the visible Data tree.")
    return int(bool(found))


if __name__ == "__main__":
    raise SystemExit(main())

"""Build the sole new record used by the carry animation condition."""
from pathlib import Path
import struct

MASTER, LIGHT = 0x1, 0x200
FIRST_LOCAL_ID = 0x800
PLUGIN_FORM_ID = 0x01000000 | FIRST_LOCAL_ID  # One master precedes this file.
FORM_VERSION = 44
RECORD = struct.Struct("<4sIIIIHH")
GROUP = struct.Struct("<4sI4sIII")


def field(kind, payload):
    return struct.pack("<4sH", kind, len(payload)) + payload


def record(kind, payload, flags=0, form_id=0):
    return RECORD.pack(kind, len(payload), flags, form_id, 0, FORM_VERSION, 0) + payload


def build():
    header = (
        field(b"HEDR", struct.pack("<fII", 1.71, 2, FIRST_LOCAL_ID + 1))
        + field(b"CNAM", b"Throw Object\0")
        + field(b"SNAM", b"Throw Object carry animation condition\0")
        + field(b"MAST", b"Skyrim.esm\0")
        + field(b"DATA", bytes(8))
        + field(b"INCC", struct.pack("<I", 0))
    )
    definition = (
        field(b"EDID", b"ThrowObjectCarryEnabled\0")
        + field(b"FNAM", b"f")
        + field(b"FLTV", struct.pack("<f", 0.0))
    )
    glob = record(b"GLOB", definition, form_id=PLUGIN_FORM_ID)
    group = GROUP.pack(b"GRUP", GROUP.size + len(glob), b"GLOB", 0, 0, 0) + glob
    return record(b"TES4", header, MASTER | LIGHT) + group


def main():
    plugin = Path(__file__).resolve().parents[1] / "mod" / "ThrowObject.esm"
    plugin.write_bytes(build())
    print(plugin)


if __name__ == "__main__":
    main()

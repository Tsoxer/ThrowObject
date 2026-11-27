"""Build the carry condition and the packages offered by the native plugin."""
from pathlib import Path
import struct

MASTER, LIGHT = 0x1, 0x200
FIRST_LOCAL_ID = 0x800
TRAVEL_LOCAL_ID = 0x810
TRAVEL_SLOTS = 32
HOLD_LOCAL_ID = TRAVEL_LOCAL_ID + TRAVEL_SLOTS
FORM_VERSION = 44
RECORD = struct.Struct("<4sIIIIHH")
GROUP = struct.Struct("<4sI4sIII")


def field(kind, payload):
    return struct.pack("<4sH", kind, len(payload)) + payload


def record(kind, payload, flags=0, form_id=0):
    return RECORD.pack(kind, len(payload), flags, form_id, 0, FORM_VERSION, 0) + payload


def plugin_form(local_id):
    return 0x01000000 | local_id  # One master precedes this file.


def group(kind, records):
    return GROUP.pack(b"GRUP", GROUP.size + len(records), kind, 0, 0, 0) + records


def package_events():
    return b"".join(field(event, b"") + field(b"INAM", bytes(4)) + field(b"PDTO", bytes(8))
                    for event in (b"POBA", b"POEA", b"POCA"))


def travel_package(index):
    # Skyrim.esm's Travel template has Location (UID 0), Ride Horse (2),
    # and Prefer Preferred Path (4). Its idle destination is the actor's own
    # position. The DLL points only a claimed slot at the thrown object.
    payload = (
        field(b"EDID", f"ThrowObjectInvestigateTravel{index:02d}\0".encode("ascii"))
        + field(b"PKDT", struct.pack("<IBBBBHH", 0x2000, 18, 0, 0, 0, 0, 0))
        + field(b"PSDT", struct.pack("<bbBbb3xI", -1, -1, 0, -1, -1, 0))
        + field(b"PKCU", struct.pack("<III", 3, 0x00016FAA, 3))
        + field(b"ANAM", b"Location\0")
        + field(b"PLDT", struct.pack("<III", 12, 0, 42))
        + (field(b"ANAM", b"Bool\0") + field(b"CNAM", b"\0")) * 2
        + b"".join(field(b"UNAM", bytes([uid])) for uid in (0, 2, 4))
        + field(b"XNAM", b"\x03")
    )
    return record(b"PACK", payload + package_events(), form_id=plugin_form(TRAVEL_LOCAL_ID + index))


def inspection_hold_package():
    # Vanilla HoldPosition template, holding the package start location.
    # Permit the regular aggro-radius response. Leave both weapon flags clear.
    payload = (
        field(b"EDID", b"ThrowObjectInspectHold\0")
        + field(b"PKDT", struct.pack("<IBBBBHH", 0, 18, 0, 0, 0, 0x40, 0))
        + field(b"PSDT", struct.pack("<bbBbb3xI", -1, -1, 0, -1, -1, 0))
        + field(b"PKCU", struct.pack("<III", 1, 0x000503D0, 5))
        + field(b"ANAM", b"Location\0")
        + field(b"PLDT", struct.pack("<III", 2, 0, 64))
        + field(b"UNAM", b"\0")
        + field(b"XNAM", b"\x08")
    )
    return record(b"PACK", payload + package_events(), form_id=plugin_form(HOLD_LOCAL_ID))


def build():
    header = (
        field(b"HEDR", struct.pack("<fII", 1.71, 4 + TRAVEL_SLOTS, HOLD_LOCAL_ID + 1))
        + field(b"CNAM", b"Tsoxer\0")
        + field(b"SNAM", b"Pick up loose objects with G and throw them with Left Mouse Button. Press E to drop.\0")
        + field(b"MAST", b"Skyrim.esm\0")
        + field(b"DATA", bytes(8))
        + field(b"INCC", struct.pack("<I", 0))
    )
    definition = (
        field(b"EDID", b"ThrowObjectCarryEnabled\0")
        + field(b"FNAM", b"f")
        + field(b"FLTV", struct.pack("<f", 0.0))
    )
    glob = record(b"GLOB", definition, form_id=plugin_form(FIRST_LOCAL_ID))
    packages = b"".join(travel_package(i) for i in range(TRAVEL_SLOTS))
    return record(b"TES4", header, MASTER | LIGHT) + group(b"GLOB", glob) + group(b"PACK", packages + inspection_hold_package())


def main():
    plugin = Path(__file__).resolve().parents[1] / "mod" / "ThrowObject.esm"
    staged = plugin.with_suffix(".tmp")
    staged.write_bytes(build())
    staged.replace(plugin)
    print(plugin)


if __name__ == "__main__":
    main()

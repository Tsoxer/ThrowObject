"""Validate the PE and SKSE version record using pefile."""
from pathlib import Path
import argparse
import struct

import pefile


def check(path):
    pe = pefile.PE(str(path), fast_load=True)
    assert pe.FILE_HEADER.Machine == 0x8664, "Expected Windows AMD64"
    assert pe.FILE_HEADER.Characteristics & 0x2000, "Expected a DLL"
    assert pe.OPTIONAL_HEADER.Magic == 0x20B, "Expected PE32+"
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    exports = {symbol.name: symbol.address for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols}
    assert b"SKSEPlugin_Load" in exports, "Missing load export"
    version = pe.get_data(exports[b"SKSEPlugin_Version"], 0x350)
    assert struct.unpack_from("<II", version) == (1, 0xF0), "Incorrect 0.0.15 metadata"
    assert version[8:264].split(b"\0", 1)[0] == b"ThrowObject"
    assert struct.unpack_from("<I", version, 0x308)[0] == 5, "Address Library and post-1.6.629 flags required"
    supported = struct.unpack_from("<16I", version, 0x30C)
    assert supported == (0x01064920,) + (0,) * 15, "Expected only Skyrim 1.6.1170.0"
    assert struct.unpack_from("<I", version, 0x34C)[0] == 0x02020060, "Expected minimum SKSE64 2.2.6"
    print("Valid AMD64 SKSE DLL, ThrowObject 0.0.15, runtime 1.6.1170, Address Library metadata.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    check(parser.parse_args().dll)

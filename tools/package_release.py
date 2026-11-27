"""Package the assembled MO2 mod and its sources."""
from hashlib import sha256
from pathlib import Path
import argparse
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
DOCS = ("CHANGELOG.txt", "README.txt")
DEVELOPER_DOCS = ("TESTING.md", "CRASH_ANALYSIS.md", "ANIMATION_CALIBRATION.md")
IGNORED = {"__pycache__", "target"}


def add_tree(archive, source, prefix, exclude=()):
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if not path.is_file() or any(part in IGNORED for part in relative.parts) or path.suffix in exclude:
            continue
        archive.write(path, str(prefix / relative))


def add_mod(archive, prefix, exclude=()):
    archive.write(ROOT / "mod/ThrowObject.esm", str(prefix / "ThrowObject.esm"))
    for directory in ("SKSE", "meshes"):
        add_tree(archive, ROOT / "mod" / directory, prefix / directory, exclude)


def package(output, pdb):
    output.mkdir(parents=True, exist_ok=True)
    mo2 = output / "Throw Object v0.3.9 RC3.zip"
    sources = output / "Throw Object v0.3.9 RC3 Source.zip"
    symbols = output / "Throw Object v0.3.9 RC3 Symbols.zip"
    dll = ROOT / "mod/SKSE/Plugins/ThrowObject.dll"
    expected = tuple(sorted((ROOT / "exported").glob("ThrowObject_*.hkx")))
    if len(expected) != 6 or not dll.is_file() or not pdb.is_file():
        raise ValueError("Build and stage the compiled DLL, its matching PDB and all six exports first.")
    with ZipFile(mo2, "w", ZIP_DEFLATED, compresslevel=6) as archive:
        add_mod(archive, Path())
        add_tree(archive, ROOT / "LICENSES", Path("LICENSES"))
        for name in DOCS:
            archive.write(ROOT / name, name)
    prefix = Path("ThrowObject-v0.3.9-rc3")
    with ZipFile(sources, "w", ZIP_DEFLATED, compresslevel=6) as archive:
        excluded = {".dll", ".pdb", ".blend1", ".hkx"}
        for directory in (".github", "animation", "cmake", "LICENSES", "src", "tests", "tools"):
            add_tree(archive, ROOT / directory, prefix / directory, excluded)
        add_mod(archive, prefix / "mod", excluded)
        for name in ("CMakeLists.txt", ".gitignore", *DOCS, *DEVELOPER_DOCS):
            archive.write(ROOT / name, str(prefix / name))
        archive.write(ROOT / "exported/hkx-qa.json", str(prefix / "docs/hkx-qa.json"))
    with ZipFile(symbols, "w", ZIP_DEFLATED, compresslevel=6) as archive:
        archive.write(pdb, "ThrowObject.pdb")
        archive.write(dll, "ThrowObject.dll")
        archive.writestr("SHA256SUMS", f"{sha256(dll.read_bytes()).hexdigest()}  ThrowObject.dll\n"
                         f"{sha256(pdb.read_bytes()).hexdigest()}  ThrowObject.pdb\n")
        add_tree(archive, ROOT / "LICENSES", Path("LICENSES"))
    for path in (mo2, sources, symbols):
        with ZipFile(path) as archive:
            assert archive.testzip() is None, path
        print(f"{sha256(path.read_bytes()).hexdigest()}  {path.name}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--pdb", type=Path, required=True)
    options = parser.parse_args()
    package(options.output, options.pdb)

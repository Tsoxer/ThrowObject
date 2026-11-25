# Throw Object - Animated Object Throwing

**v0.0.15 source archive** · **Skyrim Special Edition / Anniversary Edition 1.6.1170 (Steam)** · [Nexus Mods release](https://www.nexusmods.com/skyrimspecialedition/mods/200504)

Throw Object lets you pick up loose objects, carry them in your right hand, and throw them wherever you aim. Six custom third-person animations cover pickup, idle, walking, jogging, sprinting, and throwing. The mod handles first-person aiming, weight-based flight, impacts, and NPC reactions.

**G** picks up an eligible object, **E** drops it while you carry it, and **left mouse** throws it. The optional menu lets you rebind the controls and display a nearby NPC's daily tolerance for hits. See the [player README](README.txt) and the [complete changelog](CHANGELOG.txt).

v0.0.15 fixes a pickup lockout caused by a stale held-object entry on load. It preserves the v0.0.14 player settings, ESL master, and SKSE co-save formats.

## Project structure

```text
src/                          Original C++ SKSE plugin and settings menu
mod/                          ESL master, INI, and animation-framework configuration
animation/ThrowObjectAnims9/  Editable Blender project, animation tools, and browser preview
cmake/                        Windows cross-build toolchain and compatibility patches
tools/                        ESM generator, animation staging, and release packaging
tests/                        Native logic and animation configuration tests
docs/hkx-qa.json              Retained HKX export verification report
LICENSES/                     Third-party license notices
README.txt                    Exact player documentation supplied by Tsoxer
CHANGELOG.txt                 Exact release history supplied by Tsoxer
```

`src/plugin.cpp` handles input, item ownership, animation events, and SKSE serialization. `src/physics.cpp` implements release, flight, and impact behavior. `src/tolerance.cpp` tracks daily NPC tolerance. `src/held_recovery.hpp` contains the v0.0.15 recovery fix.

The animation project contains all six editable actions and the matching local browser viewer. Pickup attaches the object at **frame 22**, and Throw releases it at **frame 15**, both at **30 FPS**. [Animation project notes](animation/ThrowObjectAnims9/README.md) · [Pose calibration](ANIMATION_CALIBRATION.md)

## Runtime requirements

- **Skyrim SE/AE Steam 1.6.1170** and **SKSE64 2.2.6 or newer** for that executable. [SKSE](https://skse.silverlock.org/)
- **Address Library** with the Anniversary Edition database for 1.6.1170. [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- **Animation Injector 1.0.8 or newer** for pickup and throw actions. [Animation Injector](https://github.com/skypia0147-dev/Animation-Injector)
- **Open Animation Replacer** for the third-person carry animations. [Open Animation Replacer](https://www.nexusmods.com/skyrimspecialedition/mods/92109)
- **Optional** SKSE Menu Framework 3 for in-game settings, key rebinding, and the tolerance meter. [SKSE Menu Framework 3](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)

## Build prerequisites

- CMake **3.25 or later**, Visual Studio 2022 C++ tools with x64 support, a Windows SDK, and Python 3.
- Git and network access for CMake's first dependency fetch. The build pins CommonLibSSE-NG, spdlog, rapidcsv, and the Menu Framework SDK by commit.
- Blender **4.3.2**, PyNifly **V30.3.0**, and a locally extracted vanilla third-person skeleton and standing idle if regenerating the Skyrim animations. The vanilla inputs stay local. [PyNifly V30.3.0](https://github.com/BadDogSkyrim/PyNifly/releases/tag/V30.3.0)

## Building

Build the x64 DLL and generate the ESL master from the repository root on Windows.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix mod --component ThrowObjectRuntime
python tools/make_esm.py
```

The Windows GitHub Actions workflow runs these steps and uploads the native staging files and PDB. The original Linux-to-Windows cross-build instructions are in the [player README](README.txt).

To stage the animation files, regenerate the six HKX clips using the animation export instructions, or copy the six `ThrowObject_*.hkx` files from `meshes/actors/character/animations/ThrowObject/` in the Nexus v0.0.15 install ZIP into a local `exported/` folder. If reusing the shipped clips for packaging, also copy `docs/hkx-qa.json` from this repository to `exported/hkx-qa.json`. [Animation export instructions](animation/ThrowObjectAnims9/README.md#export-the-skyrim-hkx-clips-on-windows-or-linux)

```powershell
python tools/stage_animations.py exported --mod mod
python tests/test_oar.py
python tools/package_release.py releases --pdb build/Release/ThrowObject.pdb
```

The browser preview also works independently with the included original mannequin and animation assets. See the [local preview instructions](animation/ThrowObjectAnims9/README.md#open-the-browser-preview-locally).

## Verification

This export uses the original source files retained with the v0.0.15 Windows build. The native source, dependency pins, and build scripts match the retained release source. The two player-facing text files match the supplied release documents byte for byte.

- All six C++ source files and nine project headers match the source checksums in the PDB paired with the release DLL.
- The three portable C++ test suites and the animation staging test passed. The generated ESM, 84 staged HKX paths, and packaged configuration match the retained runtime files when using the six shipped clips.
- Blender 4.3.2 opened the included project and found all six editable actions without external asset links.

The retained cloud-built MO2 reference archive, `Throw Object v0.0.15.zip`, has SHA-256:

```text
eda47fe526cbc30be7e3d0443f4057277849ebcb8124f6feba066f2c3b4f1a65
```

That hash identifies the retained reference archive. Rebuilding can change DLL debug paths, ZIP metadata, and the packaged documentation. The source export contains the original editable Blender project and local preview. It excludes the release DLL, PDB, Skyrim animation binaries, and original Bethesda build inputs.

Run `sha256sum -c SHA256SUMS.txt` from the repository root to check the extracted source files. See [TESTING.md](TESTING.md) for the original verification and in-game test cases.

## Credits

**Author, design, direction, and in-game testing** · Tsoxer

**Programming and debugging** · Codex (OpenAI)

## Licensing

See the published permissions for the original Throw Object material and the separate notices for its dependencies. [Nexus Mods permissions](https://www.nexusmods.com/skyrimspecialedition/mods/200504#permissions) · [Third-party notices](THIRD_PARTY_NOTICES.md)

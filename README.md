# Throw Object - Animated Object Throwing

**v0.3.9 RC3 source archive** Â· **Skyrim Special Edition / Anniversary Edition 1.6.1170 (Steam)** Â· [Nexus Mods page](https://www.nexusmods.com/skyrimspecialedition/mods/200504)

Throw Object lets you pick up loose objects, carry them in your right hand, and throw them wherever you aim. Six custom third-person animations cover pickup, idle, walking, jogging, sprinting, and throwing. The mod handles first-person aiming, weight-based flight, impacts, and NPC reactions.

**G** picks up an eligible object, **E** drops it while you carry it, and **left mouse** throws it. The optional menu lets you rebind the controls and display a nearby NPC's daily tolerance for hits. See the [player README](README.txt) and the [complete changelog](CHANGELOG.txt).

v0.3.9 RC3 supports pickup for crossbows with skinned world models. It builds a separate held visual and uses the original crossbow reference when you drop or throw it. Crossbow pickup was confirmed in-game.

## Gameplay

- **Carry and throw loose items.** Supported items include clutter, arrows, Riekling spears, one-handed swords, daggers, war axes, maces, pickaxes, woodcutterâ€™s axes, fishing rods, helmets, gloves, boots, staves, backpacks, and crossbows. World items need a compatible model and a single loaded Havok body.
- **Throw by weight or melee damage.** Weight, shape, and impact speed affect flight and damage. Thrown one-handed weapons, staves, and crossbows tumble in flight. The optional 50% normal-swing damage setting is enabled by default and applies to one-handed melee weapons.
- **Distract NPCs while hidden.** With the sneak eye closed, a throw can draw eligible NPCs to the landing spot. They can leave furniture, kneel to inspect for **4â€“7 seconds** by default, and then resume their routines. Nearby misses with a sword, dagger, war axe, or mace can draw an investigation. A direct weapon hit triggers the combat response for the NPC it strikes.
- **Track daily tolerance.** Eligible non-hostile NPCs have a daily hit threshold. Ordinary thrown objects can build toward it. Direct weapon hits can fill it immediately. The optional tolerance meter shows the current count and threshold.

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

`src/plugin.cpp` handles input, item ownership, animation events, and SKSE serialization. `src/physics.cpp` implements release, flight, and impact behavior. `src/carry_visual.cpp` builds held visuals, and `src/equipment.cpp` classifies equipment and handles melee-based damage. `src/distraction.cpp` manages NPC investigations, `src/suspicion.cpp` plays installed Skyrim voice lines, and `src/tolerance.cpp` tracks daily NPC tolerance. `src/held_recovery.hpp` handles held-item recovery on load.

The animation project contains all six editable actions and the matching local browser viewer. Pickup attaches the object at **frame 22**, and Throw releases it at **frame 15**, both at **30 FPS**. [Animation project notes](animation/ThrowObjectAnims9/README.md) Â· [Pose calibration](ANIMATION_CALIBRATION.md)

## Runtime requirements

- **Skyrim SE/AE Steam 1.6.1170** and **SKSE64 2.2.6 or newer** for that executable. [SKSE](https://skse.silverlock.org/)
- **Address Library** with the Anniversary Edition database for 1.6.1170. [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- **Animation Injector 1.0.8 or newer** for pickup and throw actions. [Animation Injector](https://github.com/skypia0147-dev/Animation-Injector)
- **Open Animation Replacer** for the third-person carry animations. [Open Animation Replacer](https://www.nexusmods.com/skyrimspecialedition/mods/92109)
- **Optional** SKSE Menu Framework 3 for in-game settings, key rebinding, and the tolerance meter. [SKSE Menu Framework 3](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)

## Save upgrades

When upgrading from v0.0.15, replace the installed mod with v0.3.9 RC3 and keep `ThrowObject.esm` enabled. Keep your paired `.ess` and `.skse` files and your `ThrowObject.user.ini`. RC3 uses the existing carry FormID and reads the existing held-item and tolerance co-save records. When loading a save made while carrying, the mod restores the matching item to the world and clears the carry pose. [Installation and save recovery](README.txt)

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

To stage the animation files, regenerate the six HKX clips using the animation export instructions, or copy the six `ThrowObject_*.hkx` files from `meshes/actors/character/animations/ThrowObject/` in the v0.3.9 RC3 install ZIP into a local `exported/` folder. If reusing the shipped clips for packaging, also copy `docs/hkx-qa.json` from this repository to `exported/hkx-qa.json`. [Animation export instructions](animation/ThrowObjectAnims9/README.md#export-the-skyrim-hkx-clips-on-windows-or-linux)

```powershell
python tools/stage_animations.py exported --mod mod
python tests/test_oar.py
python tools/package_release.py releases --pdb build/Release/ThrowObject.pdb
```

The browser preview also works independently with the included original mannequin and animation assets. See the [local preview instructions](animation/ThrowObjectAnims9/README.md#open-the-browser-preview-locally).

## Verification

This export uses the native source, dependency pins, and build scripts from the v0.3.9 RC3 Windows build. The two player-facing text files match the supplied release documents byte for byte.

- The Windows DLL build, the three portable C++ test suites, and the animation staging test passed. The release DLL and PDB have matching debug identifiers.
- The upgrade audit compared the original carry FormID, held-item and tolerance co-save readers, default controls, and all 84 installed HKX files against v0.0.15.
- In-game testing confirmed the hidden weapon near-miss behavior in RC2 and crossbow pickup in RC3. The v0.0.15 save checks covered record compatibility.

SHA-256 for the retained MO2 archive, `Throw Object v0.3.9 RC3.zip`

```text
2bf1c5bd5142732fddb475c57751b6f93e1f95f93c6d130a0a5438e390a9898f
```

That hash identifies the retained reference archive. Rebuilding can change DLL debug paths, ZIP metadata, and the packaged documentation. The source export contains the original editable Blender project and local preview. It excludes the release DLL, PDB, Skyrim animation binaries, and original Bethesda build inputs.

See [TESTING.md](TESTING.md) for the verification and in-game test cases.

## Credits

**Author, design, direction, and in-game testing** Â· Tsoxer

**Programming and debugging** Â· Codex (OpenAI)

## Licensing

See the published permissions for the original Throw Object material and the separate notices for its dependencies. [Nexus Mods permissions](https://www.nexusmods.com/skyrimspecialedition/mods/200504#permissions) Â· [Third-party notices](THIRD_PARTY_NOTICES.md)
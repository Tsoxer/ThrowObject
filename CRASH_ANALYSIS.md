# Throw Object v0.0.3 crash analysis

## Release crash call chain

The report is from Skyrim Steam 1.6.1170. While handling a pending pickup for the Dwemer Plate (`02013EA7`, base `00027F15`, `Clutter\Dwemer\DwePlate01.nif`), the engine's `DeepCopyStream` reached a `bhkConvexVerticesShape`. `NiMemStream` was active when the C runtime attempted a 16-byte copy with a null destination. The trace does not establish why the stream buffer was null.

The deployed v0.0.2 DLL has SHA-256 `15134564d6010808539c7e36027b6ea130a554b9c38ae7dd430ae25239dd931f` and its PE has no debug directory. Relinking the original release objects with a map reproduced every PE byte except two bytes in the COFF timestamp. The reported return instructions also match the archived binary at all three offsets. The v0.0.2 release did not retain a matching PDB, so these names come from the release linker map, unwind ranges and call instructions.

| Return RVA | Resolved function | Call at this frame |
| --- | --- | --- |
| `0x0009E61` | `to::UpdateHook::run` | The inlined update consumes the grab marker and calls `to::onMarker` with `Event::grab`. |
| `0x002DF60` | `to::onMarker` | The inlined `grab` reaches `RE::NiObject::CreateDeepCopy` with the world reference's 3D root. |
| `0x004A896` | `RE::NiObject::CreateDeepCopy` | The CommonLib wrapper calls the Skyrim function for Address Library ID `70191`. |

v0.0.2 removed collision from the copy only after `CreateDeepCopy` returned, leaving the original model's live collision in the serialization path. The player update consumed a pending grab marker. With the logged bindings, **D** was ordinary movement. The report does not show D initiating pickup.

## Changes

- The plugin assembles a new carry node from validated, collision-free static `BSTriShape` leaves using the engine's scene-object clone API. It walks original branches to compute the mesh transforms and leaves their collision objects alone. Pickup no longer invokes `CreateDeepCopy`.
- The pickup handler pins the target and its current 3D and checks the reference, cell, attachment, and vanilla grab state before parking it. If the source 3D or the carry geometry is unavailable or unsupported, the action cancels before the reference is disabled. Drop and throw use the original reference and physics solution.
- Pickup preparation runs after the original player update. The animation callback sets marker bits only. v0.0.4 removes v0.0.3's OS thread-ID check from input dispatch, because the user's log showed that Skyrim delivered input on a separate thread. World operations still run through the player update.
- The log flushes each pickup stage, including the source reference, render leaf, attachment, parking, cancellation and recovery. Unsupported visible models are reported explicitly.

## Verification status

The crash path was identified from the archived DLL and the supplied report. The portable tests exercise the render-tree selection and failure policy using synthetic scenes, including a convex-collision branch modeled after the report. The fixtures contain no actual game meshes. The runtime cases in `TESTING.md` are pending on Steam 1.6.1170.

Each symbols archive contains that release's DLL, matching PDB and both file hashes. Load the pair from the release that produced the crash report.

# Throw Object crash analysis

## v0.1.5 native positional voice playback

The 1.6.1170 engine call sites pass the secondary
`BSISoundOutputModel` base to Address Library ID 67624. With the pinned
CommonLib signature, passing the `BGSSoundOutput` form supplies its
primary TESForm vtable. This accounts for the bad indirect dispatch
seen on the v0.1.3 audio worker. v0.1.5 casts to the sound interface
before calling the same Skyrim entry point. It uses the game's
positional dialogue model and attaches the sound to the loaded actor.
Voice resources are validated before queuing playback and the sound
is stopped at unload or when the loaded actor model changes.
[1.6.1170 call site and argument analysis](https://github.com/marthofdoom/MFO/blob/0bd3fee148fc2748101509c95451c8c6b0335ea9/Docs/ENGINE_NOTES.md#L2659-L2663)

## v0.1.3 sneak throw audio worker crash

The supplied ThrowObject.log loaded v0.1.3 and showed a gold ingot being
thrown. It queued a FemaleShrill suspicion recording for an NPC at
21:21:24.158. The crash report is from the same second and shows an
invalid indirect call at `SkyrimSE.exe+0x0CBBD12` (Address Library
`67892+0x82`) on the audio worker stack. The report identified
`BSXAudio2GameSound` and `BSAudioManagerThread`. The request completing
on the player-update thread did not establish successful playback.
There are no ThrowObject.dll frames to resolve in this audio worker
stack. The log and timing tie the report to the direct sound play path.
The report does not establish the precise invalid engine sound field.

v0.1.4 removes that path, including archive clip probing, sound handle
creation, and sound output and playback commands. Near misses and hits
retain their flinch. Ordinary sneak reactions inject no player detection
event. The standing reaction path and the existing combat and crime
rules remain in place. No scripts, state fields, or master records were
added for this fix.

The same log identified pickaxe reference `000DF1DE`, base
`000E3C16`, as failing the eligibility filter before physics or 3D
checks. v0.1.4 qualifies that standard Skyrim weapon base even if the
loaded animation type fails the general one-handed test. The report
does not include the weapon's loaded animation type. [Skyrim pickaxe record](https://cslist.uesp.net/index.php?game=sr&formid=0x000e3c16)

## v0.1.2 sneak throw near the Whiterun Guard

The 11/25/2026 report caught a read access violation at
`ThrowObject.dll+0x57E25` while a throw passed the guard in Dragonsreach.
The v0.1.2 release DLL has SHA-256
`90959b4700a233b3e728daae6e825beb12a489ec401db6ae13f62b8be7ce28f4`.
The archived DLL and PDB share GUID
`AF6BA52E-CCBA-9D32-4C4C-44205044422E`, age 1. Their disassembly and source
line table identify the following frames.

| RVA | Resolved source |
| --- | --- |
| `0x57E25` | `playSuspicion`, v0.1.2 `physics.cpp:342` |
| `0x56E62` | `sneakReaction`, `physics.cpp:404` |
| `0x53395` | `sweepFlight`, inlined in `updateFlights` |
| `0x9BF1` | `update`, inlined in `UpdateHook::run` |

The former `TESNPC::GetObjectVoiceType` virtual call returned
`0x000001BCB1795401`. The next instruction sequence treated that
misaligned return value as a voice form and dispatched its editor-ID
virtual call through the invalid table address `0x00000001BCDAB9DB`.
The crash occurred before the first resource or audio call. In v0.1.3
the actor-base `voiceType` field is read directly, with a form-type
check and direct editor-ID access. That removes both virtual calls
from this lookup. The master file is byte-identical.

The new DLL and core regressions passed cloud checks. Live playback
and reproduction with the Dragonsreach guard remain in the acceptance
checklist. [SKSE ActorBase.GetVoiceType](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusActorBase.cpp)

## v0.0.2 pickup crash

### Release crash call chain

The report is from Skyrim Steam 1.6.1170. While handling a pending pickup for the Dwemer Plate (`02013EA7`, base `00027F15`, `Clutter\Dwemer\DwePlate01.nif`), the engine's `DeepCopyStream` reached a `bhkConvexVerticesShape`. `NiMemStream` was active when the C runtime attempted a 16-byte copy with a null destination. The trace does not establish why the stream buffer was null.

The deployed v0.0.2 DLL has SHA-256 `15134564d6010808539c7e36027b6ea130a554b9c38ae7dd430ae25239dd931f` and its PE has no debug directory. Relinking the original release objects with a map reproduced every PE byte except two bytes in the COFF timestamp. The reported return instructions also match the archived binary at all three offsets. The v0.0.2 release did not retain a matching PDB, so these names come from the release linker map, unwind ranges and call instructions.

| Return RVA | Resolved function | Call at this frame |
| --- | --- | --- |
| `0x0009E61` | `to::UpdateHook::run` | The inlined update consumes the grab marker and calls `to::onMarker` with `Event::grab`. |
| `0x002DF60` | `to::onMarker` | The inlined `grab` reaches `RE::NiObject::CreateDeepCopy` with the world reference's 3D root. |
| `0x004A896` | `RE::NiObject::CreateDeepCopy` | The CommonLib wrapper calls the Skyrim function for Address Library ID `70191`. |

v0.0.2 removed collision from the copy only after `CreateDeepCopy` returned, leaving the original model's live collision in the serialization path. The player update consumed a pending grab marker. With the logged bindings, **D** was ordinary movement. The report does not show D initiating pickup.

### Changes

- The plugin assembles a new carry node from validated, collision-free static `BSTriShape` leaves using the engine's scene-object clone API. It walks original branches to compute the mesh transforms and leaves their collision objects alone. Pickup no longer invokes `CreateDeepCopy`.
- The pickup handler pins the target and its current 3D and checks the reference, cell, attachment, and vanilla grab state before parking it. If the source 3D or the carry geometry is unavailable or unsupported, the action cancels before the reference is disabled. Drop and throw use the original reference and physics solution.
- Pickup preparation runs after the original player update. The animation callback sets marker bits only. v0.0.4 removes v0.0.3's OS thread-ID check from input dispatch, because the user's log showed that Skyrim delivered input on a separate thread. World operations still run through the player update.
- The log flushes each pickup stage, including the source reference, render leaf, attachment, parking, cancellation and recovery. Unsupported visible models are reported explicitly.

### Verification status

The crash path was identified from the archived DLL and the supplied report. The portable tests exercise the render-tree selection and failure policy using synthetic scenes, including a convex-collision branch modeled after the report. The fixtures contain no actual game meshes. The runtime cases in `TESTING.md` are pending on Steam 1.6.1170.

Each symbols archive contains that release's DLL, matching PDB and both file hashes. Load the pair from the release that produced the crash report.

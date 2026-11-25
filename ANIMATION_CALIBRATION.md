# Anims9 skeletal comparison

## v0.0.6 shoulder placement

The supplied vanilla idle has 27.50 game units between its upper-arm joints. In v0.0.5, the old clavicle mapping re-aimed both collarbones to match the mannequin's neutral proportions. It added roughly 29° of local rotation, moving the shoulder pivots about 7.09 game units in chest space in Hold frame 0. The decoded shoulder scale was already the native value. Rotating the shoulder attachment points created the wider silhouette.

v0.0.6 keeps the idle's parent-local translations and scale at the clavicles and upper-arm sockets. It transfers the authored clavicle rotation relative to that layout, and the upper-arm, forearm, and wrist still resolve from the Anims9 directions. The exporter checks the new shoulder transforms after decoding across all 217 samples.

| Decoded sample | v0.0.5 shoulder span | v0.0.6 shoulder span |
| --- | ---: | ---: |
| Idle into Pick Up, frame 0 | 27.50 | 27.50 |
| Pick Up into Hold, frame 45 | 31.32 | 27.51 |
| Hold, frame 0 | 31.32 | 27.51 |
| Hold into Throw, frame 0 | 31.32 | 27.51 |

The measured arm, palm, and other retargeted directions stayed within 0.02° at full pose weight. The maximum decoded deviation from the authored clavicle rotation magnitude was 0.04°. The four idle and custom handoffs passed their decoded transform checks. Use `--compare` with the v0.0.5 export folder to reproduce the shoulder report. Each sample is included in the MO2 archive's `docs/hkx-qa.json`. Live clothing and skin deformation still need verification on the installed character.

## v0.0.5 skeletal retarget

The v0.0.4 HKX files were decoded against the supplied 99-bone third-person Skyrim skeleton. The Anims9 Blender actions were evaluated at the same 30 FPS timestamps. The comparison reconstructs parent-local HKX transforms in model space and measures segment directions and palm orientations. In the old Hold export, the forearm points back while the authored forearm reaches forward. The largest former upper-arm error during Throw was 123.21°. The old mapping applied rotation deltas relative to a posed vanilla idle and projected the right elbow into that idle's hinge. The discrepancies appear in the decoded clip itself, outside Skyrim's animation blending.

| Matching sample | Measurement | v0.0.4 | v0.0.5 |
| --- | --- | ---: | ---: |
| Hold frame 0 | Right forearm direction | 94.57° | <0.02° |
| Throw frame 15 | Right palm orientation | 167.53° | <0.02° |
| Pick Up frame 22 | Right upper-arm direction | 31.39° | <0.02° |
| Carry Walk | Largest right forearm error | 98.26° | <0.02° |
| Carry Jog | Largest right forearm error | 110.95° | <0.02° |
| Carry Sprint | Largest right forearm error | 116.49° | <0.02° |

The new mapping calibrates each segment from the Skyrim reference hierarchy and the source rig, then solves the desired model-space rotations from the evaluated Anims9 frames. Inverting the current target parent produces the local transform written to HKX. Skyrim keeps its own bone lengths and topology. True palm landmarks determine wrist orientation, and the source forearm roll is distributed to vanilla twist helpers. The source `.blend` and preview motion are unchanged.

The full comparison covers 217 samples of the six clips, including the deliberately blended endpoints. Across the 209 full-weight samples the measured angular error was at most 0.02°. The first four intervals of Pick Up and the last four intervals of Throw ease through the selected standing idle. Decoded Hold → Throw and Pick Up → Hold agree within 0.001 game units and 0.001° per local transform at the join. Pick Up enters from idle and Throw exits to idle with the same bound on the endpoint difference. Grab stays at frame 22 and release at frame 15. The MO2 archive includes the per-frame measurements in `docs/hkx-qa.json`.

At runtime, the carry replacement becomes eligible at the end of Pick Up and remains available under the Throw entry until release. This keeps the intended starting pose available for the framework's entry blend. Engine blending, skin weights, and playback against the player's installed skeleton still require live verification.

The exporter uses x64 Havok 2010 normal-blend clips bound by bone index. To regenerate the comparison, use `--compare` alongside the standard export arguments and point it to the v0.0.4 HKX folder. The original skeleton and idle are private inputs. [Bethesda Library HKX layout](https://baddogskyrim.github.io/BethesdaLibrary/file-formats/animations/) · [PyNifly V30.3.0](https://github.com/BadDogSkyrim/PyNifly/releases/tag/V30.3.0)

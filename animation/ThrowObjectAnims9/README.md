# ThrowObjectAnims9

Six original third-person animation actions with a right-hand grip authored at **30 FPS**. The project targets the vanilla human rig for **Skyrim Special Edition / Anniversary Edition 1.6.1170**. The preview uses an original mannequin and the same Blender actions used as inputs to the Skyrim retargeter.

| Clip | Frames, zero-based | Duration | Playback |
| --- | ---: | ---: | --- |
| `ThrowObject_PickUp` | 0–45 | 1.500 s | One shot into Hold Object |
| `ThrowObject_Hold` | 0–60 | 2.000 s | Loop in the playback framework |
| `ThrowObject_Throw` | 0–28 | 0.933333 s | One shot |
| `ThrowObject_CarryWalk` | 0–36 | 1.200 s | Slow walk loop |
| `ThrowObject_CarryJog` | 0–24 | 0.800 s | Default run loop |
| `ThrowObject_CarrySprint` | 0–18 | 0.600 s | Fast sprint loop |
| `ThrowObjectGrab` | **22** | **0.733333 s** | Annotation in Pick Up |
| `ThrowObjectRelease` | **15** | **0.500 s** | Annotation in Throw |

Frame 14 is the last sampled attached frame. At frame 15 the fingers are open and the object detaches from that frame’s world-space socket transform. The browser, Blender action marker, and HKX exporter use the same zero-based timing. PyNifly’s graphical reimport starts Blender actions on frame 1, so the same annotation can appear at Blender frame 16 after reimport.

Throw starts from frame 0 of the Hold pose, including its free arm. Hold uses the same weight shift and breathing motion with its left elbow set slightly farther back. The socket moves about 14 mm at most. Throw winds up over frames 1–11, accelerates through 12–15, follows through over 16–21, and settles at frame 28. The feet and actor root stay planted while the hips transfer weight from the right leg toward the left. The left arm counterbalances the throw.

## Anims9 Pick Up

Anims9 is a separate copy of Anims8. The Pick Up change is limited to the left upper arm, forearm, and wrist rotations. The right-hand reach and grip, crouch, foot placement, clip length, grab timing, and Hold transition retain their Anims8 motion.

The free arm follows the forward swing from Anims7 with a gradual forward and outward adjustment around the raised knee. Mesh checks at 181 quarter-frame samples found that the left arm clears both legs and that the left hand clears the preview floor. Armor fit and clearance on Skyrim's vanilla rig still need in-game verification.

Pick Up starts in the existing neutral idle. The character lowers into a crouch with both feet planted, reaches with the right hand and closes the grip at frame 22. The prop stays on the ground until that frame. The character rises to the exact frame-0 Hold pose, and the preview continues into the Hold Object loop when Pick Up finishes. Selecting Loop playback repeats Pick Up for review. The Grab marker and scrubber allow frame-by-frame inspection.

For Skyrim integration, use `ThrowObjectGrab` to attach the prop at frame 22 and transition into Hold at the end of Pick Up. The export and marker round trip on a synthetic rig passed. An installed game still needs to validate the runtime sequence.

## Carry locomotion

The three clips are original forward, in-place cycles. The right hand uses the same finger pose and item socket as Hold. The left arm swings with the stride. Walk stays grounded, Jog adds brief flight, and Sprint uses a stronger lean, higher knee lift and quicker cadence. The stance fraction decreases with speed, following the pattern measured in human gait. [Mann and Hagy, *Biomechanics of walking, running, and sprinting*](https://pubmed.ncbi.nlm.nih.gov/7416353/)

| Mode | Steps/min | Reference speed | Grounded fraction per foot |
| --- | ---: | ---: | ---: |
| Walk | 100 | 1.10 m/s | 62% |
| Jog | 150 | 2.65 m/s | 41% |
| Sprint | 200 | 4.43 m/s | 32% |

Reference speeds describe the original mannequin and the moving floor in the preview. Skyrim movement speed and stamina belong to the game’s locomotion states. A game integration should route forward walk, normal run and sprint state changes into the corresponding loops while an item is held. Calibrate foot contact against the actual graph speed and target proportions. The clips keep the root translation fixed. The exported tracks carry local body rise and weight transfer through the vanilla COM bone. A future behavior layer could blend the carry arm over other locomotion mods. These exports are authored as full-body clips.

## Files

- `generated/ThrowObject.blend` contains the mannequin, skin, hand socket and all six editable actions. Switch them in the Action Editor with the **Throw Object** rig selected. Use the frame bounds in the table above when previewing an action in Blender. Enable **View → Show Pose Markers** in the Action Editor to see the release marker.
- `generated/source-qa.json` records source-pose measurements. `generated/anatomy-qa.json` records the Hold and Throw joint checks. `generated/locomotion-qa.json` records the carry contact and joint checks. `preview/dist/assets/ThrowObject.glb` has all six named clips and the original skinned mannequin. `preview/dist/assets/timing.json` records the frame and attachment contract.
- `source/` contains the carry, throw and gait motion, finger poses, and mannequin generator. Rebuild with `scripts/build_preview.py`.
- `scripts/export_skyrim.py` reads a locally owned x64 Skyrim animation skeleton and standing idle, retargets the editable actions, and uses PyNifly’s native HKX codec with a read-back check.
- `mo2-smoke-test/` contains optional Animation Injector XML for manual playback in an isolated Mod Organizer 2 profile. It requires the locally exported HKX clips.

## Open the browser preview locally

From the extracted project directory, run either of the following and open **http://127.0.0.1:8000/**. The preview bundles Three.js, so it can run offline after extraction. Use a browser with WebGL 2.

```powershell
py -3 -m http.server 8000 --bind 127.0.0.1 --directory .\preview\dist
```

```bash
python3 -m http.server 8000 --bind 127.0.0.1 --directory ./preview/dist
```

Select **Pick Up** and press Play to watch it lead into Hold Object. Select **Throw Object** to preview a throw. Use the arrow buttons to step between frames 14 and 15, or click **Release event** to jump to frame 15. **Grip** follows the right hand for close inspection and **Side** shows the throwing arc. Select **Walk**, **Jog**, or **Sprint** to view a carry loop against the moving floor. The preview reconstructs the throw flight deterministically from the GLB socket at release and its preceding sample, with gravity and a small floor bounce. Each prop has its own geometry and can be swapped without changing a clip.

For restricted deployment, put the contents of `preview/dist/` on an authenticated static host with the access policy applied to **every** path, including `assets/`. Check in a signed-out session that HTML and GLB requests require sign-in. The packaged local preview runs directly from those static files.

## Rebuild the Blender and browser assets

Blender 4.3.2 was used in the cloud. The preview bundle was built with Three.js 0.183.2 and esbuild 0.27.3. No game files or Blender add-ons are needed for this step. Open a shell in the project directory. On Windows, substitute the full path to `blender.exe` if Blender is absent from PATH.

```bash
blender -b -t 2 --python-exit-code 1 -P scripts/build_preview.py -- generated
```

Copy `generated/ThrowObject.glb` and `generated/timing.json` over their equivalents in `preview/dist/assets/`. If you edit the viewer JavaScript, run `npm ci` then `npm run build` from `preview/`. If you change clip bounds or the release frame, update the viewer’s human-readable defaults along with the generated manifest.

## Export the Skyrim HKX clips on Windows or Linux

1. Use a Skyrim SE installation with unmodified third-person human assets. With the Creation Kit Archive Browser or your Bethesda archive extractor, search your local `Skyrim - Animations.bsa` and `Skyrim - Meshes*.bsa` for `meshes\actors\character\character assets\skeleton.hkx` and `meshes\actors\character\animations\mt_idle.hkx`. Extract those two files into a private folder. For a different standing stance, supply the corresponding vanilla third-person human idle from the same install. The parser requires the HKX skeleton, with the Havok animation track order and reference pose.
2. Download the PyNifly **V30.3.0** release ZIP. Extract it to a private tools folder so that `io_scene_nifly/hkx/anim_skyrim.py` exists. This path uses its pure Python packfile codec and runs on Windows or Linux. [PyNifly V30.3.0](https://github.com/BadDogSkyrim/PyNifly/releases/tag/V30.3.0) · [PyNifly Animation Guide](https://github.com/BadDogSkyrim/PyNifly/blob/main/ANIMATIONS.md)
3. From the extracted project folder, adjust the three absolute paths below to match your machine. This PowerShell command writes the six animations and a read-back report into the loose-file MO2 staging tree.

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.3\blender.exe' -b -t 2 .\generated\ThrowObject.blend --python-exit-code 1 -P .\scripts\export_skyrim.py -- `
  --pynifly 'C:\ModTools\PyNifly\io_scene_nifly' `
  --skeleton 'C:\SkyrimPrivate\skeleton.hkx' `
  --idle 'C:\SkyrimPrivate\mt_idle.hkx' `
  --out '.\mo2-smoke-test\meshes\actors\character\animations\ThrowObject'
```

On Linux, call `blender` with the same arguments and Linux paths. Use `--idle-frame N` for a different sample of the standing idle. The exporter calibrates source and target segment frames from their reference skeletons, evaluates the source poses at 30 FPS, and writes each desired rotation in its current Skyrim parent's space. The target's limb lengths and track order are retained. Pick Up frame 0 and Throw frame 28 match the selected idle. The exporter checks both custom handoffs after decoding and can compare a previous set with `--compare path/to/previous/exports`. Skyrim SE uses x64 Havok 2010 binaries. [Bethesda Library animation format analysis](https://baddogskyrim.github.io/BethesdaLibrary/file-formats/animations/) · [PyNifly HKX implementation](https://github.com/BadDogSkyrim/PyNifly/blob/V30.3.0/io_scene_nifly/hkx/anim_skyrim.py)

## MO2 playback test on 1.6.1170

1. Check the runtime executable version, install SKSE64 **2.2.6** for the Steam build of **1.6.1170** from the archives, and install the matching Address Library. In a vanilla-skeleton MO2 test profile, install an Animation Injector build supporting this runtime and the version 6 XML below. The framework author lists 1.6.1170 as tested. [SKSE downloads and archives](https://skse.silverlock.org/) · [SKSE release history](https://github.com/ianpatt/skse64/releases) · [Animation Injector requirements](https://github.com/skypia0147-dev/Animation-Injector) · [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
2. After exporting, install the **contents** of `mo2-smoke-test/` as a separate MO2 mod. Keep `meshes/` and `SKSE/` at its root. The included version 6 XML uses the documented player hotkey start, loop and transition syntax. It is a playback harness for Animation Injector 1.0.8 or compatible later builds. [Animation Injector XML guide](https://github.com/skypia0147-dev/Animation-Injector/blob/main/XML_GUIDE_EN.md)
3. Launch through SKSE, stand still in third person with both hands free, then press **F6** to hold, **F7** to throw from Hold, and **F8** to exit Hold early. Hold’s 30-second limit belongs to this test harness. Check the Injector log for XML parse or asset resolution errors. Restart Skyrim after editing HKX or XML files.
4. Verify the Hold seam and Hold → Throw transition, wrist roll and fingers, planted feet, tracking of the left arm, one-shot end, and blend back into the runtime idle. Repeat on both supported vanilla humanoid body variants and record the actual skeleton/runtime used. Check for hook routing by observing or tracing `ThrowObjectRelease` once per Throw before wiring gameplay. This test XML plays the clips with no prop attached.

## Integration contract and verification

- The right-hand attachment target is `NPC R Hand [RHnd]` on the runtime skeleton. Mesh offsets belong to a future per-object attachment profile. The browser’s normalized metre-space socket is `ThrowObjectSocket` and is specific to its original preview rig. Convert local axes and units, and calibrate against the vanilla hand before reusing those offsets.
- A future system needs to transition Hold → Throw once, handle early cancel and interruption, and use the active clip’s `ThrowObjectRelease` event to read the right-hand transform and release the mesh once. Event registration or routing depends on the selected graph/framework. Confirm the receiving hook on 1.6.1170 before making it responsible for object release. The nominal 0.500 s timestamp is clip time and changes in real time with playback speed. If an SKSE plugin is later needed, CommonLibSSE-NG provides engine bindings for animation graph events and other game systems. [Animation Injector event documentation](https://github.com/skypia0147-dev/Animation-Injector/blob/main/XML_GUIDE_EN.md) · [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)
- The exported animations use a full standing-base pose with right-arm and modest torso motion. Test this form standing in place. A future layered gameplay setup must explicitly preserve live locomotion and left-hand poses through its behavior layer or bone mask. The vanilla third-person skeleton is the only game target for this milestone.
- The Linux cloud verified 30 FPS source generation, Blender/GLB clip timing, the hand socket, the Hold seam, the grip and release poses in Blender renders, carry foot contact and loop seams, and an x64 HKX encode/decode round trip of all six clips on a synthetic fixture. That fixture exercised clip duration, ordered bindings, float slots, and both the `ThrowObjectRelease` and `ThrowObjectGrab` annotations. The cloud did not have an installed copy of Skyrim 1.6.1170 or its vanilla animation files, so vanilla-skeleton HKX export, armor fit, and runtime playback are pending. The release bundle contains original authored assets and source only.

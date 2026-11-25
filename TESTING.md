# Testing

## v0.0.15 held-item recovery

Portable tests run the production recovery policy for missing, deleted,
and base-mismatched references, valid recovery, and a retry after the
engine has left a valid reference disabled.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Upgrade with empty hands | Load a paired v0.0.14 save and co-save. Check G pickup, carry, throw and native hold-E. Verify existing NPC tolerance values and settings. |
| Upgrade with an item held | Load a paired v0.0.14 save and co-save made while carrying a disposable loose object. Confirm the original reference reappears, the player is empty-handed and G can pick it up again. |
| Stale reference | Load a disposable test save whose held-item recovery entry resolves to a missing, deleted or different-base reference. Check for the stale recovery message, verify that reference was untouched, and use G to pick up a different item. Save with empty hands and reload to verify the entry is gone. |
| Uninstall | Drop an item, confirm it is visible, and make a new manual save. Remove the mod in MO2, load that save, and check the item is still present and Skyrim's hold-E works in both camera views. |

## v0.0.14 tolerance meter display

Portable tests cover all four corner anchors and invalid viewport sizes.
Live verification requires Skyrim 1.6.1170.

| Case | Runtime check |
| --- | --- |
| Enable and inspect | Switch on Show tolerance meter and close the menu directly. Aim at Nazeem or Lydia and check the text. Confirm the log reports the HUD callback and its first drawn target. Restart to verify the checkbox was saved. |
| Corners and HUD replacement | Try all four placements. Compare default Skyrim and a HUD replacement, and check first and third person. The overlay should draw without stealing input. |
| Target changes | Look at a neutral actor, a friendly animal, a hostile actor and empty space. Confirm the meter follows the first two and hides for the others. Verify that opening a blocking menu hides it. |
| Upgrade | Load a v0.0.13 save and its SKSE co-save after two hits on a friendly actor. Verify the same hit count and limit in the meter. Throw once more and confirm the expected count and combat rules. |

## v0.0.13 NPC tolerance

Portable checks cover per-actor independent rolls, escalation, co-save
payload encoding, configuration bounds and the 6 AM boundary. The live
regression suite uses Skyrim 1.6.1170.

| Case | Runtime check |
| --- | --- |
| Default and menu bounds | Enable Debug logging. With the default 3–7 range, hit distinct friendly actors and check each logged threshold is in range. Set both sliders to 3 for a deterministic threshold and, separately, use 1–20. Apply and save, restart and verify that the settings persist. |
| Tolerance meter | Enable the meter and try every screen corner. Look at an untouched actor and check the name and 0/threshold. Confirm a valid hit displays 1/threshold. Look away, target a hostile, open a menu, and toggle the meter off. Check that it hides and the setting persists. |
| Adults, children and friendly animals | Hit a neutral adult, child, dog and horse on a fresh in-game day. Verify independent counts and that earlier hits do not add Throw Object bounty or start mod-driven combat. Check combat and the guard response on the threshold for eligible adults. Check that Skyrim's normal child and ally protections still apply. |
| Same-day travel and reload | With a fixed threshold of 3, hit a friendly NPC twice, leave the area, then return the same day. Repeat with a save and matching co-save. Verify the next confirmed hit reaches 3, with the previous roll retained. |
| 6 AM | Count hits before midnight, after midnight, and just before 6 AM. At or after 6 AM, wait for the next qualifying hit and verify that the count starts at one against a new roll. Change the bounds midday to verify that existing rolls retain their threshold. |
| Impact exclusions | Hit one actor twice with the same flying reference during a single throw, miss another, and hit a hostile target and yourself. Verify one count for the first actor and normal behavior for misses, hostiles and self-hits. |
| Save upgrade | Load existing v0.0.12 save pairs with empty hands and a held item. Verify normal recovery, new tolerance defaults, and save/reload after counting a hit. |

## v0.0.12 first-person clearance fallback

Portable tests verify centered priority, each fallback, target alignment from
an offset launch, and rejection when every candidate is blocked. Live
verification requires Skyrim 1.6.1170.

| Case | Runtime check |
| --- | --- |
| Previously failing first-person throw | In the same setup that gave the v0.0.11 obstruction message, throw a small object and a bucket toward open space. Check that the log shows a centered release or a first-person clearance point and that the object travels toward the aimed point. |
| Nearby scenery | Stand near a wall or door. Test a clear route and a path with all release positions blocked. The blocked attempt keeps the reference held. |
| Regression and upgrade | Load paired v0.0.11 save files. Check third-person pickup and throw, first-person drop, native hold-E and repeat pickup. |

## v0.0.11 first-person cursor and save upgrade

Portable tests cover screen-center rays for symmetric and asymmetric camera projection, facing direction, pitched launch speed, and ballistic reach. Live checks require Skyrim 1.6.1170.

| Case | Runtime check |
| --- | --- |
| Open space | In first person, hold a small item and a bucket in turn. Throw at a distant landmark at eye level and at steep angles. Verify that the item appears on the crosshair line rather than out to the right. |
| Target and obstruction | Aim at nearby wall marks and a reachable NPC from clear space. Confirm the trajectory converges on the aimed line and that expected impacts fire. Test a wall or door too close for the object to fit and verify that the item stays held. |
| Camera and timing | Turn during the wind-up and confirm release follows the current view at frame 15. Check first-person drop, third-person throw and drop, and native hold-E. |
| Existing save | Replace v0.0.10 in the same MO2 profile. Load paired .ess and .skse files saved with empty hands and while carrying. Verify held reference recovery, custom bindings, and Golden Frog repeat pickup. |

## v0.0.10 repeat pickup after movement

The v0.0.9 log records one successful Golden Frog throw and repeated range rejections for the same reference after release. The portable test checks that reach follows the settled body when the stored release position is far away and that it still rejects an actually distant body.

| Case | Runtime check |
| --- | --- |
| Golden Frog already thrown in v0.0.9 | Load the v0.0.9 save and matching co-save with v0.0.10. Walk within reach of the previously thrown frog, aim at it, press G, and throw. Repeat three times. Compare the reference ID across grabs and throws. Inspect the live and stored distances in `ThrowObject.log`. |
| Fresh and vanilla items | Throw a newly dropped frog and a vanilla bucket. From the release spot, aim at the landed item and check rejection when the body is beyond pickup range. Walk close and use G again. |
| Existing save, controls and impacts | Load a v0.0.9 save made while holding an item and verify the recovery of the original reference. Verify the same G and left mouse bindings, vanilla hold-E behavior, and health and combat reactions after a real NPC hit. |

## v0.0.9 repeat pickup, camera release, impacts, and save upgrade

Cloud validation covers launch exclusion and weight thresholds for return hits, the existing grip and throw model, Windows compilation, and the v0.0.8 save contract. These live checks require Skyrim 1.6.1170.

| Case | Runtime check |
| --- | --- |
| Loose inventory drops | Craft and drop a Golden Frog and also drop a vanilla miscellaneous item. Throw each, let it settle, and repeat three times. Compare the reference ID in `ThrowObject.log` across grabs and throws. Repeat with a frog thrown before the update. |
| Camera release | In both camera views throw a small object and a bucket into open space. In first person also check vertical aim and aiming next to an obstruction. Compare release and clearance entries in the log. Confirm E drop and native hold-E in both perspectives. |
| NPC impact | With Impact Damage enabled, throw at a neutral humanoid in a clear line. Compare health before and after and check combat reaction. Repeat with a teammate, an essential actor near 1 HP, and two distinct throws of the same object. |
| Near miss | Throw just past an idle NPC with reactions enabled, then repeat with a wall between the path and actor. Check for a flinch and eligible speech on the visible miss and no change to health or bounty. |
| Player impact | Throw forward and confirm there is no release self-hit in open space. Throw a light object upward and stand where it lands, then repeat with a heavy item. Compare `return hit` and knockdown log entries. An ordinary drop should create no Throw Object flight damage. |
| v0.0.8 saves | Load an existing `.ess` and matching `.skse` with empty hands, then load a second pair saved while carrying. Verify the original held reference is restored once, the previous keybinds load, and a new save reloads. |

## v0.0.8 carry, throws, damage, and save upgrade

Portable tests cover adjacent exterior cells, a missed pickup-end event after successful attachment, grip geometry, and the faster launch across all drag categories. The compatibility checks compare the v0.0.7 ESM, settings defaults, co-save record layout and animation files to v0.0.8. Live checks are pending.

| Case | Runtime check |
| --- | --- |
| Walk after pickup | Begin walking just after the item reaches the hand, then repeat after the pose settles. Walk across a known exterior cell boundary and verify that the same item can still be dropped or thrown. |
| Temporary hand loss | Switch camera or briefly enter an interrupting state with an item held. Verify that the visual resumes and the same reference stays parked meanwhile. |
| Grip and launch | Compare Salmon Meat, a Dwemer Plate and a bucket in third person. Verify the first two sit over the opening in the fingers, and a free throw leaves the revised socket faster on a flatter path. |
| NPC damage and theft | With Impact Damage enabled, throw directly at a living NPC and inspect the `health … -> …` log. Check a miss, a wall-blocked throw and repeated contact with the same NPC. Separately pick up a stolen item with G and verify the original reaction and bounty. |
| v0.0.7 save upgrade | Test two copies of v0.0.7 `.ess` and `.skse` pairs, one with empty hands and one with an item held. Load with v0.0.8 using the same MO2 load order. Verify normal controls, that the held reference is recovered once, and that the mod works after another save and reload. |

For future updates, verify the ESM and save record identifiers against the previous release and test both of the upgrade cases above whenever the recovery code changes.

## v0.0.7 hover and range

Portable tests compare v0.0.6 and v0.0.7 at matching angles and heights across all item drag categories. They verify twice the horizontal ground-contact distance and the same vertical arc in the flight model. Separate cases check a direct aim-assisted shot and the one-unit hover shift with retained hand clearance. Live Skyrim verification is pending.

| Case | Runtime check |
| --- | --- |
| Salmon Meat and Dwemer Plate | In third person, compare the same camera and character against v0.0.6. Check that the visual is slightly farther out toward the opening of the fist. Drop and throw from the updated hover position. |
| Range by item | Use the same aim and release height in an unobstructed area. With the same settings in both versions, compare ground-contact distances for a dense ingot, bucket, salmon, and plate. Check each item's ratio against its own v0.0.6 result. |
| Targeting and eligibility | Aim at a reachable NPC and confirm the throw still aims at the actor. Try a weapon and a piece of armor to confirm they remain ineligible. |

## v0.0.6 shoulders and hover

The decoded shoulder span at the start of Hold returned from 31.32 game units to 27.51, close to the 27.50-unit vanilla idle. The export also checks local shoulder translation, scale, and authored clavicle rotation for all six clips. Portable tests check a coin, upright bucket bounds, and tilted bucket bounds for a closer hover, outward placement, and clearance.

| Case | Runtime check |
| --- | --- |
| Shoulder shape in third person | Compare the same character, outfit, and camera immediately before Pick Up and while standing in Hold. Watch both shoulders during the transition, then check walk, jog, sprint, and throw. |
| Bucket over the palm | Check from the side and front. The bucket should sit lower, closer to the hand and farther forward/right of the head and shoulder. Try a coin and plate to check that small items stay near the palm. |
| Release position | Drop and throw the bucket with a clear path. Verify the original world reference starts at the updated hover socket. Grab stays at frame 22 and throw release at frame 15. |

These in-game cases are pending.

## v0.0.5 pose, release, and impact checks

Blender and decoded x64 HKX were compared for all six clips over 217 samples against the supplied 99-bone human skeleton. The full-weight error was at most 0.02° for the measured joints. Both custom handoffs and the neutral endpoints passed the decoded local-transform checks. Portable tests cover projected hover bounds and straight, offset, parallel and over-height capsule approaches. The Windows DLL and PDB were verified in the cloud. The following Skyrim checks are pending.

| Case | Runtime check |
| --- | --- |
| Six corrected poses | In third person, compare Pick Up, Hold, Walk, Jog, Sprint, and Throw against the Anims9 preview. Prioritize the Hold wrist, pickup crouch, throw wind-up, and frame 15 release. |
| Blending | Check the standing idle into Pick Up, Pick Up into Hold, Hold into Throw, and Throw into standing idle. Verify the carry pose still returns when a release is blocked. |
| Hover and same-point release | Use a coin, plate and bucket. Inspect from both sides. Confirm the bottom of the item clears the moving hand and that drop and throw begin at the logged hover socket. Check grab at frame 22 and throw release at frame 15. |
| NPC health | Throw a heavy eligible item directly at a living NPC with Impact Damage enabled. Compare their health before and after. Look for `queued ... HP` in `ThrowObject.log`. Repeat with damage disabled, a near miss, a wall between actor and flight, and repeated collisions from one throw. |
| Initial self-hit | Throw a bucket forward with open space. Then try aiming through the thrower's body or standing near a wall. Verify the blocked-release notification, retained item when blocked, and no initial overlap. Later native ricochets may still injure actors. |

## v0.0.4 input and native-grab regression

The v0.0.3 runtime log reports input dispatch on thread `15460` and the `Main::threadID` value as `2616`. The guard rejected the dispatch before reading the key. The portable v0.0.4 tests submit pickup from a separate worker and cover rebinds, Menu Framework blocking, native Activate pass-through, stale commands, and held-item drop. The in-game checks are pending.

| Case | Runtime check |
| --- | --- |
| G on a Dwemer Plate | In third person, aim at an eligible Dwemer Plate and press G. Look for `Queued G`, `Processing G`, `TO_Pickup` and `Grab frame 22` in `ThrowObject.log`. Confirm the crouch plays and the plate attaches at the marker. Repeat with Numpad 5 rebound to Pick Up. |
| Ordinary E | With empty hands, tap and hold E on untouched clutter in third person. Compare reach, motion and animation with v0.0.4 disabled. Repeat in first person. `Skyrim native grab` should identify the reference without a custom attachment, launch, or drop entry for it. |
| Native grab has priority | Hold E and press G as the native grab starts, and again while moving the item. The held object should remain under Skyrim's control. Repeat with a remapped native Activate key. |
| Custom E drop | Press G to carry the item, then press and release E. Confirm that the same item drops, the full E press is owned by the mod, and the next E press is native again. |
| Native grab after a throw | Throw an item, then catch it with native hold-E. Check that throw tracking for that reference stops and compare both camera views. |

## v0.0.3 crash regression

The cloud regression tests cover a synthetic convex-collision root, small and nested render meshes, cancelled pickups, default D and E routing, and 24 simulated pickup/drop cycles. The fixtures exercise the production selection policy and game state machine. The following runtime tests are pending on Steam 1.6.1170. Keep the flushed `ThrowObject.log` from each result.

| Case | Runtime check |
| --- | --- |
| Dwemer Plate in Redwater Den | With default bindings, aim at the plate from the report (`02013EA7`) and press G in third person. At frame 22 the hand should gain the plate and the original ground reference should disappear. Confirm the same reference is enabled after E drop. |
| Movement near the plate | Press D while aiming at the plate with empty hands and verify no pickup. Press G, then press and hold D before and through the grab marker. Compare the diagnostic sequence to the original crash. |
| Movement while held | Walk, jog, strafe and sprint with the plate. Check that the hand tracks it without an extra world copy. Throw and confirm release at frame 15 and a physical response from the same reference. |
| Small and complex collision meshes | Repeat pickup, movement, drop and throw with small clutter and items with complex single-body collision. For unsupported appearances, check for a diagnostic and a still-enabled original. |
| Repeated pickup and drop | Perform at least 20 loops, including a camera switch. Verify the world reference and ownership persist, that no extra copies appear, and that the normal stance resumes. |
| Vanilla activation and drag | With empty hands, tap E and hold E on fresh, untouched objects in first and third person. Confirm that the log has no custom attachment or physics operation for those references. While dragging with E, press G and verify vanilla retains control. |
| Missing or changing 3D | Select a reference, then remove its 3D before the grab marker with a test fixture or a cell transition. Confirm safe cancellation and normal movement. Repeat with the reference's visible mesh unsupported. |

## Executed in Codex Cloud

| Check | Result |
| --- | --- |
| Anims9 ZIP CRC, original Blender and viewer GLB equality | Passed. The copied animation source matches the existing Anims9 files byte for byte. |
| Linux C++ unit tests | Passed. State transitions, repeat presses, marker frame constants, equal launch speed, elevation, reachable aim correction, type and weight drag, impact damage and its cap, native E passthrough in unowned states, keyboard/mouse normalization, duplicate bindings, clearing, and capture press/release. |
| Skyrim SE plugin format | Passed with Ortham esplugin using a whole-file parse, ESM/ESL flags, valid light FormID range, master list, HEDR version and group count. |
| Animation Injector format 6 | Passed with the upstream `XmlV2::Compile` parser built on Linux. Both actions and both annotation rules compiled. |
| PyNifly codec and staging | Passed for all six x64 clips with an original synthetic reference rig. The staging script also wrote the four conditional locomotion copies and validated both annotation names. |
| Windows DLL | Passed. Clang 19 compiled and lld linked the AMD64 Windows DLL in Codex Cloud. PE tools confirmed both SKSE exports, the Address Library and post-1.6.629 flags, and the exact 1.6.1170.0 target. |
| Vanilla HKX export | Passed against the supplied 99-bone skeleton and idle. All six x64 files passed PyNifly binding, frame-count, timing, clip loop, and annotation read-back. Grab is 0.733333 s and Release is 0.500000 s. Pick Up → Hold and Hold → Throw agree within 0.001 game units and 0.001° across all 99 local transforms. |
| OAR staging and conflict diagnostic | Passed. Real exported clip bytes match each staged copy. A lower-priority fixture leaves priority uncontested. Equal maximum fixtures are reported only for overlapping modern and legacy OAR clip paths. |
| Skyrim runtime | Pending. The cloud lacks a running Steam Skyrim installation. |

## Manual acceptance checklist for Steam 1.6.1170

Run every item in an isolated MO2 profile with the staged vanilla-skeleton HKX, a fresh test save and debug logging enabled. Save the SKSE, Throw Object, Animation Injector and OAR logs with results. All in-game entries below are pending.

| # | Procedure and required observation |
| ---: | --- |
| 1 | Aim at a loose apple and press **G**. The crouch starts once. |
| 2 | Record third-person single-frame video. The apple remains grounded until grab frame 22. |
| 3 | Verify the apple appears in the right hand with a stable grip. |
| 4 | Stand still. Confirm Hold loops for at least ten minutes. |
| 5 | Use Skyrim's walk toggle. Confirm the CarryWalk blend while moving forward. |
| 6 | Use default movement speed. Confirm CarryJog. |
| 7 | Sprint and confirm CarrySprint and ordinary stamina use. |
| 8 | Press **E** while holding. Confirm a gravity-only drop from the hand. |
| 9 | Aim at the dropped apple and pick up the same reference again. |
| 10 | Throw directly forward. Check the visual release at frame 15 and record the original FormID in the log. |
| 11 | Throw above eye level. Observe an upward arc. |
| 12 | Throw toward a lower target. Observe a downward trajectory. |
| 13 | Aim at a distant target with an offset third-person camera. Confirm aim direction converges from the hand. |
| 14 | Throw toward a wall. Check collision, bounce and blocked release when the hand is against the wall. |
| 15 | Throw just past an eligible NPC. Check a vanilla warning, cooldown, zero damage and unchanged bounty. |
| 16 | Strike an NPC with light clutter. Record HP lost and genuine contact log. |
| 17 | Strike with an ingot from the same distance. Record HP lost. |
| 18 | Strike with a heavy Dwemer piece and record HP lost. |
| 19 | Compare losses at controlled impact speed. Heavier items should generally cause more damage. |
| 20 | Compare real contacts against near misses and ordinary drops. Only valid thrown impacts lower HP. |
| 21 | Bounce a single object against the same NPC. Check a single damage and crime result for that target. |
| 22 | Hit the same eligible civilian three times. Check the real violent bounty increases by 10 each time, with no additional 40-gold assault fine. |
| 23 | Repeat in another Hold. The correct faction must receive the charge. |
| 24 | Repeat near a detecting guard. Check vanilla confrontation, fine payment and arrest. |
| 25 | With a held item and guards hostile, drop it or throw it away. Draw and sheath a weapon. Check ordinary surrender. |
| 26 | Switch between first and third person while carrying. Check visible grip and stable reference ID. Repeat after a first-person pickup. |
| 27 | Spam pickup and throw input. Check singly registered actions and cleanup. |
| 28 | Save while holding, retain the co-save, then reload. Check the original ref is enabled at its world position with carry state cleared. |
| 29 | Test a loose mod-added misc item with a single dynamic rigid body. |
| 30 | Test loose weapons and armor. Neither should start mod pickup. |
| 31 | Compare reference IDs and counts before and after pickup, drop, launch, save and reload. |
| 32 | Interrupt with fast travel, cell change, dialogue, inventory, pause, weapon drawing, stagger, ragdoll, killmove, and death. Check reference and pose cleanup. |
| 33 | Change each configuration section, apply, exit and relaunch. Check saved values and in-game effects. |
| 34 | Open the ESM in SSEEdit with Skyrim.esm. Check the light flag, master flag, stable global FormID, and no unexpected overrides. |
| 35 | Load SKSE on 1.6.1170. Verify the DLL loads, metadata is accepted and no Address Library lookup fails. |
| 36 | Press **G** on a crosshair target, with no target, and on a target beyond range. Only the live eligible in-range object starts pickup. |
| 37 | Place an eligible item only in inventory. Press **G** with an empty crosshair. Nothing should appear in the hand. |
| 38 | Record owner FormID before an owned-object pickup and check the native theft event with a witness. |
| 39 | Drop the same owned object. Verify the owner ID and vanilla Steal prompt. |
| 40 | Throw the same owned object. Verify the owner ID and native theft state. |
| 41 | Recover it through vanilla Activate. Verify the stolen inventory designation when appropriate. |
| 42 | Enable trajectory logging. Throw apple, lockpick, potion, ingot and Dwemer metal with identical settings. Measure the initial Havok speed and the range in the same unobstructed setup. |
| 43 | Compare impact damage for those objects and check distinct weight-dependent results. |
| 44 | Rebind pickup. Confirm only the configured pickup code starts a valid pickup. |
| 45 | Rebind drop. Confirm the configured code drops and **E** passes through. |
| 46 | Rebind throw to a keyboard key or mouse button. Verify the new input and vanilla attack passthrough. |
| 47 | Restart Skyrim and verify all three bindings persist. Try duplicate codes and confirm Apply rejects them. |
| 48 | With empty hands, use **E** on a door, container and NPC. Vanilla activation should work. |
| 49 | With empty hands, attack with left mouse. Vanilla attack should work. |
| 50 | Throw, recover and aim at the same world reference. Press **G** and verify that it can be carried again. |


## v0.0.2 regression checks in the game

| Area | Procedure and expected result |
| --- | --- |
| Startup | Launch SKSE, open and close the settings menu, then read the flushed startup lines. Confirm all dependency checks and device-queue installation are present. |
| G and Numpad 5 | Target a loose object, press G and check the live crosshair FormID, pickup request, action-start and grab logs. Rebind to Numpad 5 and repeat; G should no longer start pickup. |
| Rebinding | Rebind all three actions with keyboard and mouse. Click Cancel during listening. Clear two bindings, attempt a duplicate, reset defaults, and restart. Check immediate display, persistence and that the binding press never performs a gameplay action. |
| Six animations | Test Pick Up → Hold, slow walk, ordinary run, sprint and Throw. Watch OAR's animation log for carry slots and the Injector/Throw Object logs for the one-shot actions. Repeat with an active ordinary animation replacer and then an intentional equal-maximum conflict. |
| Ownership and restoration | Confirm only the exact mod-controlled reference activates Carry. Drop, throw, cancel or reload; watch the OAR animation log return to the ordinary idle/movement replacements. Check player facial and unrelated states. |
| Native short E | With empty hands, tap E on a fresh item from both camera views. Compare with v0.0.2 disabled. Verify the vanilla activation/inventory behavior matches. |
| Native hold E | With empty hands, hold E and drag a fresh object in first and third person. Compare positioning, apparent reach, and motion with v0.0.2 disabled. Confirm the mod does not attach, claim, or update the object. Press G while still holding E to confirm Vanilla retains ownership. |
| Native re-grab | Throw a reference, then use native E-grab on the same reference as soon as possible. Confirm any pending release or flight record stops, and check a different vanilla-grabbed reference never begins mod tracking. |
| Normal control restoration | After mod E-to-drop or throwing, release the key, then use E on a door, a loose item and a container. Confirm ordinary activation and hold-to-grab after both releases. |

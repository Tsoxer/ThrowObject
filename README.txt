Throw Object v0.3.9 RC3 — 11/26/2026
=================================

Author/Director/Tester: Tsoxer
Programmer/Debugger: Codex (OpenAI)

Release for Skyrim SE/AE 1.6.1170 Steam. The native plugin uses CommonLibSSE-NG, SKSE64 and Address Library. The ESL-flagged ThrowObject.esm supplies the hold condition for Open Animation Replacer. Animation Injector handles third-person pickup and throwing. The project includes the six original ThrowObjectAnims9 actions and their exporter.

The cloud build includes the compiled DLL, validated ESL master, and six clips exported against the supplied vanilla 99-bone skeleton and standing idle. Both original game files are private inputs. The DLL checks dependencies and clip paths during data load.

Changes in v0.3.9 RC3
--------------------

- Crossbow pickup supports skinned world models with renderer data in their skin partitions. The held copy uses detached bone transforms for the visible model. Dropping and throwing continue with the original world reference.
- The plugin and save record checks cover upgrades from v0.0.15 and RC2. A live Skyrim check with the reported crossbows and an existing save is pending.

Changes in v0.3.9 RC2
--------------------

- Crossbows can carry a still copy of their world mesh when its visible geometry uses local transform controllers. The carried model keeps its current pose. The original reference and Havok body are used for drops and throws.
- Save nearby witnesses from a hidden sword, dagger, war axe, or mace throw and dispatch them through the same landing and investigation path as clutter. A bounce can move the weapon out of the original hearing radius after an NPC has already noticed it. The NPC struck by a weapon keeps the direct-hit combat response.
- The master, SKSE save records and settings support upgrades from v0.0.15 and v0.3.9 RC1. New gameplay needs a live Skyrim 1.6.1170 test.

Changes in v0.3.9 RC1
----------------

- Pick up, drop and throw loose backpacks and crossbows in first or third person. Backpack support includes Adventurer's Backpack and mod-added packs identified by backpack keywords or item and model metadata. Crossbows use the game's crossbow weapon type.
- Backpacks and crossbows deal damage from weight and impact. Crossbows tumble and use the existing weapon-hit reactions. The 50% melee-swing option still applies to one-handed melee weapons.
- While the sneak eye is closed, an eligible NPC can investigate a thrown one-handed melee weapon that comes to rest nearby. A direct hit keeps the weapon-hit combat reaction for that NPC.
- Two-handed melee weapons, bows, shields and full-body armor remain excluded. Prior supported items, keybindings, animations, and co-save data carry forward.

Changes in v0.3.8
-----------------

- Fit iron swords, daggers, maces, war axes and pickaxes to the established one-handed carry pose. Use the loaded model's shaft direction and center the handle near its grip pivot at full scale.
- Keep solid staff meshes when filtering particle effects during G pickup. The mod carries a render-only copy and releases the original staff reference when dropped or thrown.
- Carry forward third-person arrow and arrow stack drops, the existing ThrowObject.esm and the SKSE co-save format.

Changes in v0.3.7
-----------------

- Fit the third-person grip of iron swords, daggers, maces and war axes to the palm. The pickaxe grips the shaft below the head. Riekling spears are held around the middle with an upright, outward tilt. The fitted grips use the loaded model's shaft where geometry is available and retain its world scale.
- Pick up, drop and throw staves with compatible world models and Havok bodies. Staves tumble and deal weight-and-impact damage. A thrown staff does not cast its spell or spend charges. The 50% melee swing option applies to one-handed swords, daggers, maces and axes.
- Use oriented clearance for third-person ammunition drops, including single arrows and stacks. If the hand position is blocked, check for a clear spot near the player. Drop the original world reference with its stack count and item data.
- Keep the existing master and co-save format for upgrades from v0.3.6.

Changes in v0.3.6
-----------------

- The default kneeling range is 4–7 seconds, measured from the start of the kneeling hold. On upgrade, a saved 6–9 second pair from an older configuration adopts the new default. Other saved ranges carry forward. Saving a range in v0.3.6 marks it as a user selection, including 6–9 if chosen again.
- NPCs can leave furniture and package idles to investigate a hidden throw. Skyrim uses the same furniture system for chairs, benches, beds, cooking pots, smithing stations, grindstones, workbenches, alchemy labs, enchanters and invisible markers for leans. Reading, writing and lectern animations can also be interrupted when an NPC uses them as furniture or a package idle.
- Wait for Skyrim to release the occupied station before walking or kneeling. When the inspection ends, the NPC's own AI chooses its next action and can resume the former activity if it is still available. Quest scenes and combat retain their usual priority.
- The master and existing co-save formats carry forward from v0.3.5. Saving stops the investigation, including during the furniture exit or walk.

Changes in v0.3.5
-----------------

- After the NPC walks to the landing spot, use Throw Object's neutral Hold Position package. The vanilla guard package used in v0.3.4 ordered the NPC to draw a weapon.
- Wait until the NPC is stationary and their weapon is sheathed before playing Skyrim's kneeling entry. Start the configured 6–9 second default hold once the NPC's pose lowers. Try the vanilla instant kneel if the normal entry does not take.
- Finish with Skyrim's stand-up transition and restore the NPC's own AI. An NPC who already had their weapon drawn before the investigation can resume that stance.
- Install the complete v0.3.5 package. The updated master adds one hold package at local FormID 000830. All prior records and SKSE co-save formats carry forward.

Changes in v0.3.4
-----------------

- Replace the old movement request with an actual AI Travel package selected ahead of an eligible witness's ordinary schedules, including modded patrol and sandbox packages. On arrival, the investigation selects Skyrim's Hold Position package while the NPC inspects the object.
- Trigger Skyrim's kneeling entry directly after movement stops. After the configured hold, send the stand-up event, reset the owned idle if needed, and reevaluate the NPC's own AI. Actual combat and active scripted scenes keep control of the actor.
- Install the full v0.3.4 package. ThrowObject.esm adds private Travel packages while keeping its prior FormID, master list, and carry record. The DLL releases package claims and removes waypoints before Skyrim writes the save.

Changes in v0.3.3
-----------------

- Keep the investigation queued while an NPC recovers from a flinch and the object settles. A passing NPC can turn around and walk back even if they have already moved beyond kneeling range.
- Use Skyrim's kneeling entry and exit animations. The adjustable 6–9 second default uses an elapsed-time deadline. At the end, the NPC stands and the engine resumes their own AI package.
- Renew stalled or interrupted approaches within a fixed time limit. Only the investigation uses these movement requests. Active combat and scripted scenes still take precedence.

Changes in v0.3.2
-----------------

- Check that the player is crouched and the sneak eye is closed when the item is released. Standing throws and open-eye crouching throws keep their usual reactions.
- During an eligible throw, both a direct hit and a near-miss flinch queue a check of the object's live position. When it settles beside an NPC outside combat, they face it and kneel in place. Actors farther away can use the existing walking approach.
- Use the regular crouch animation if the actor's graph rejects the kneeling idle. The inspection uses the saved kneel range, which defaults to 6–9 seconds.

Changes in v0.3.1
-----------------

- Recognize stopped or sleeping Havok objects from their position and limit how long NPCs wait after a ground hit. Remember witnesses who noticed the throw while the player was hidden, including NPCs who voiced suspicion before the item landed.
- Set minimum and maximum kneel time under Throw Object → Settings → NPC reactions in SKSE Menu Framework, then choose Apply and save gameplay settings. The default is 6–9 seconds. The values are saved in ThrowObject.user.ini and take effect on the next kneel.
- Use the native walking request and kneeling idle temporarily, retry an interrupted approach once, and reevaluate the actor's own AI afterward, including modded packages. Combat and scripted scenes can still take precedence.
- ThrowObject.log reports whether a crouched throw captured witnesses, registered a landing, queued NPC movement, and started the kneel.

Changes in v0.3.0
-----------------

- Hidden sneak throws can distract several friendly or hostile humanoid NPCs outside combat. They can notice an eligible item passing, landing, or striking nearby from any direction. The existing Reaction distance sets the radius, and the NPC Reactions setting enables the feature.
- Once the item settles, NPCs use their own suspicion voices and walk to the same landing spot. They use Skyrim's kneeling idle to examine it for 3–6 seconds, then return to their normal routines. They leave the item in the world.
- One-handed weapons still use the established throw behavior. Throws cannot restart or redirect an active investigation. Vanilla detection and combat continue while an NPC investigates.
- Investigations end before saving and use no new co-save records. If a rolling object keeps a stale position, the mod uses a temporary vanilla navigation marker and removes it after use or before saving.

Changes in v0.2.3
-----------------

- Use the established vanilla collision lines such as “Be careful” and “Watch it” for detected crouching throws. Suspicion lines require the player to still be sneaking and undetected when the NPC reacts.

Changes in v0.2.2
-----------------

- Keep the original world model size when holding a one-handed weapon in third person. This includes swords, axes, maces, and daggers.
- Seat daggers in the finger opening and align the blade across the palm to clear the forearm.

Changes in v0.2.1
-----------------

- Place the handles of one-handed weapons in the right palm in third person. The weapon follows Skyrim's existing right-hand weapon node, with its blade or head angled up and slightly outward in the Hold pose.
- Third-person weapon drops start from the visible held placement when clear and can use the established near-hand placement when space is tight.

Changes in v0.2.0
-----------------

- Thrown one-handed weapons tumble end-over-end at a target release rate of 2.5 rotations per second in first and third person. This applies to eligible vanilla and modded swords, daggers, war axes, maces, and pickaxes.
- Spin uses Havok angular velocity. The v0.1.8 aiming, linear velocity, drag, damage calculations, drops, and save format carry forward.

Changes in v0.1.8
-----------------

- In third person, equipment drops start at the visible item's center when clear. This accounts for the scale difference between the carried visual and the original reference.
- If that point is blocked, try the closest clear positions beside the hand. The original item falls from hand height with zero launch velocity. If there is no clear opening near the hand, the item stays held.
- The v0.1.8 live report confirmed natural third-person equipment drops and all three previously working camera and item combinations.

Changes in v0.1.7
-----------------

- In first person, E releases the held item just below the center of your view when there is clearance. The item appears in front of you and falls under gravity with zero launch velocity.
- The release checks the item's oriented bounds against the camera's near plane, the player, and nearby geometry. Use the v0.1.6 near-feet placement when a visible release is blocked.
- Preserve third-person drops, throwing, suspicion dialogue, and the v0.1.6 save format.

Changes in v0.1.6
-----------------

- In third person, all items drop from the visible hand position and fall under gravity. Equipment uses its oriented bounds at the hand to check for clearance. If the spot is blocked, the item stays held.
- In first person, all items use the nearby floor placement introduced in v0.1.5. The drop starts near the player and uses the player's facing direction.
- Keep the v0.1.5 sneak suspicion dialogue that passed live testing, along with the existing throw behavior and save format.

Changes in v0.1.5
-----------------

- Equipment drops look for space near the character's feet in both camera modes. They try in front first, then nearby positions on either side. Clearance uses the item's oriented dimensions. The original item is released with zero launch velocity. Ordinary clutter uses its existing drop behavior.
- Restore suspicion voice reactions for unaware NPCs. The mod selects an existing recording for the NPC's voice type and plays it in Skyrim's audio engine, attached to the NPC's head or loaded root. Skyrim's 3D dialogue output model supplies direction and distance attenuation. Keep the original flinch and stealth behavior.
- Correct the engine interface used to assign the 3D dialogue output model. The crash in v0.1.3 matches the old CommonLib argument mismatch documented for Skyrim 1.6.1170. [Engine audio ABI notes](https://github.com/marthofdoom/MFO/blob/0bd3fee148fc2748101509c95451c8c6b0335ea9/Docs/ENGINE_NOTES.md#L2659-L2663)
- Keep the v0.1.4 pickaxe fix, master, animation files, settings, and save formats.

Changes in v0.1.4
-----------------

- Remove direct suspicion voice playback for sneak throws. The v0.1.3 log queued a voice clip just before the asynchronous Skyrim audio crash. Sneak hits and near misses can still flinch. Standing throw reactions still use the regular Skyrim dialogue path.
- Recognize Skyrim's standard Pickaxe (000E3C16) as an eligible one-handed tool. The v0.1.3 log confirmed that the mod found this form and rejected it during equipment classification, before it checked the mesh or physics. The pickaxe now reaches the same world-reference, range, physics, and quest checks as other eligible items. Skyrim pickaxe record (https://cslist.uesp.net/index.php?game=sr&formid=0x000e3c16)
- Keep the v0.1.3 master, animation files, settings, and save formats for an in-place upgrade.

Changes in v0.1.3
-----------------

- Fix the actor voice lookup that crashed v0.1.2 when a sneak throw passed near a Whiterun Guard. Suspicion playback reads the NPC's installed voice type from the same actor-base field SKSE uses. Eligible NPCs can still flinch without a recording. Sneak reactions no longer inject a detection event pointing to the player.
- Accept eligible one-handed weapons, including loose pickaxes, when their single loaded body is resting on Skyrim's weapon or clutter collision layer. Existing quest, placed-reference and geometry guards still apply. Skyrim pickaxe record (https://cslist.uesp.net/index.php?game=sr&formid=0x000e3c16)
- Preserve the tested third-person stair and clutter clearance, the weapon damage modes, MO2 metadata, and save formats.

Changes in v0.1.2
-----------------

- The ThrowObject.esm header credits Tsoxer and describes the default pickup, throw, and drop controls in MO2.
- Third-person throws for all eligible items can use nearby release points that fit the object's oriented bounds. Raised points help on stairs and beside clutter. The checks still reject releases into solid geometry or through the player.
- Sneak hits and close misses can play installed Skyrim suspicion lines in the unaware NPC's own voice. They use Skyrim's dialogue sound output and volume. If no matching recording plays, the previous native detection event is still available. Playback uses existing game audio and adds no subtitle or lip animation.
- The menu labels the weapon damage option “Melee damage for thrown one-handed weapons (50% of normal swing)”. It uses the same half-swing and weight-and-impact calculations as v0.1.1. A one-handed weapon hit still fills friendly tolerance in either mode.

Changes in v0.1.1
-----------------

- Sneak throws still cause a flinch. A hit or close miss near an unaware NPC now sends a Skyrim detection event at the disturbance. Skyrim chooses whether to search and which vanilla bark to play in the NPC's voice. Dialogue is subject to the NPC's current AI state and available lines. Sneak throws suppress Throw Object's standing warning barks.
- If a one-handed weapon's usual third-person hand release is blocked, Throw Object tests its oriented bounds and nearby release positions. It keeps world and player clearance checks and leaves the weapon held if the alternatives are blocked.
- The enabled-by-default melee mode now uses 50% of the ordinary physical swing damage on the first qualifying hit with a one-handed weapon. Charged enchantments retain their v0.1.0 behavior. Turning melee mode off restores physics damage.
- A one-handed weapon hit immediately fills a friendly target's daily tolerance and initiates the existing threshold response, crouched or standing and with either damage formula. A target that becomes hostile can briefly show the completed meter when aimed at. Other objects still count one hit at a time.

Changes in v0.1.0
-----------------

- NPCs still flinch when hit by a sneak throw. Nearby unaware NPCs use suitable vanilla discovery or search dialogue in their own voice when an applicable line is available. Normal Throw Object warning barks are suppressed for sneak throws. NPC detection, combat, and crime rules still apply.
- Added support for loose one-handed swords, daggers, war axes, maces, helmets, gloves, and boots with compatible world models and physics. Two-handed weapons, bows, staves, shields, body armor, and quest-bound or unsupported objects are excluded.
- In the menu, enable or disable Melee damage for thrown one-handed weapons under Damage and crime, then Apply and save gameplay settings. It starts enabled. The first valid NPC hit uses Skyrim's ordinary weapon damage for that individual item, including the player's weapon skill, applicable perks and tempering. Target resistance and difficulty apply on unprotected hits. Standard charged on-hit enchantments use the original weapon's charge. Sneak and power-attack multipliers are excluded.
- Turning the weapon toggle off uses the original weight-and-impact damage model for those weapons. Armor pieces always use that model. Friendly tolerance and protected-target safeguards take precedence over enchantment effects.

Changes in v0.0.15
------------------

- A missing, deleted or base-mismatched reference in the held-item recovery entry is cleared when the player returns to gameplay. Subsequent G pickups can proceed, and the stale entry is omitted from the next save. Matching references can still be restored from the co-save.
- The master, animations, settings and SKSE co-save formats are compatible with v0.0.14.

Installation
============

Requirements
------------

- Skyrim SE/AE Steam executable 1.6.1170.
- SKSE64 compatible with 1.6.1170, minimum 2.2.6. Use the Steam build from the official release archives. SKSE downloads (https://skse.silverlock.org/) · SKSE release history (https://github.com/ianpatt/skse64/releases)
- Address Library AE with the actual 1.6.1170 database. Address Library (https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Animation Injector 1.0.8 or newer with support for 1.6.1170. Animation Injector (https://github.com/skypia0147-dev/Animation-Injector)
- Open Animation Replacer with support for 1.6.1170. Open Animation Replacer (https://github.com/ersh1/OpenAnimationReplacer)
- SKSE Menu Framework 3 is optional and provides the settings page and tolerance meter. SKSE Menu Framework 3 (https://github.com/QTR-Modding/SKSE-Menu-Framework-3)

Install with MO2
----------------

1. Install v0.3.9 RC3 as a separate MO2 mod. Enable one Throw Object version in the active profile. Keep the prior version as a disabled MO2 entry for rollback. The archive includes the six existing clips and their OAR copies. The optional symbols archive is for debugging outside MO2.
2. Enable ThrowObject.esm in the plugins list. Launch the Steam 1.6.1170 game through the SKSE loader in MO2. Look for Throw Object ready for Skyrim Steam 1.6.1170 in Documents/My Games/Skyrim Special Edition/SKSE/ThrowObject.log. The fallback log is Data/SKSE/Plugins/ThrowObject.log.
3. With weapons sheathed and the player still, aim at an apple and press G. In the menu framework, open Throw Object → Settings to customize bindings. Click Rebind beside an action and press the key or mouse button. Changes save immediately to SKSE/Plugins/ThrowObject.user.ini, which may appear in MO2’s Overwrite. Copy any customized v0.0.1 ThrowObject.ini there as ThrowObject.user.ini to migrate old settings.

Uninstalling
------------

Before uninstalling, drop any held object, confirm it's visible or pick it up, and make a new manual save.

If you pick it up after dropping it, use Skyrim's normal Activate action to take it into your inventory. Make the save with Throw Object's hand empty, then close Skyrim and remove the mod in MO2. Keep the matching .skse co-save for any save made while carrying an item. The mod ships no Papyrus scripts.

Upgrade an existing Throw Object save
--------------------------------

Close Skyrim, install v0.3.9 RC3, and activate it in place of the previous version in the same MO2 profile. Keep the existing ThrowObject.esm entry and the paired .ess and .skse files. Load the existing save directly. v0.3.9 RC3 keeps the existing master name and carry FormID and uses the same serialized plugin ID, HELD v1 and TOLR v1 records. Keep your existing ThrowObject.user.ini. The weapon_melee_damage setting retains its saved value and defaults to on for players upgrading without that setting. If the save was made while holding an item, recovery re-enables the matching original reference in the world and clears the carry pose. If the saved reference is gone, deleted or belongs to a different base object, the recovery entry is cleared on the first playable update. The package and save format have been checked in the cloud. The existing tolerance record keeps same-day rolls and hit counts, including when the meter was previously off. A live reload on Skyrim 1.6.1170 remains part of the acceptance test.

Rebuild the six animations
--------------------------

Use the source archive and supply the two original files from an owned Skyrim install as private build inputs.

| File | Path inside Skyrim's game archives |
| --- | --- |
| skeleton.hkx | meshes/actors/character/character assets/skeleton.hkx |
| mt_idle.hkx | meshes/actors/character/animations/mt_idle.hkx |

The exporter runs in Blender 4.3.2 with the x64 codec from PyNifly V30.3.0. The source archive has the original .blend. On a cloud machine with the inputs already extracted, run from the source directory

anims=animation/ThrowObjectAnims9
blender -b -t 2 "$anims/generated/ThrowObject.blend" --python-exit-code 1 \
  -P "$anims/scripts/export_skyrim.py" -- \
  --pynifly /path/to/io_scene_nifly --skeleton /private/skeleton.hkx \
  --idle /private/mt_idle.hkx --out ./exported
python3 tools/stage_animations.py ./exported --mod ./mod

On Windows use the same arguments with Windows paths and call blender.exe. The staging command copies the six files into the install tree and maps the four carry clips into OAR's conditional directory. Keep the two game-local input files private. PyNifly V30.3.0 (https://github.com/BadDogSkyrim/PyNifly/releases/tag/V30.3.0)

Compatibility
=============

| Component | Scope |
| --- | --- |
| Runtime | Windows x64 SKSE DLL. The load check accepts Steam Skyrim 1.6.1170.0 only. |
| Plugin records | ESL-flagged ThrowObject.esm with the carry global at local ID 0x800 and 33 investigation packages. Skyrim.esm is its only master. The plugin contains only its own records. |
| Rig | Original Anims9 actions were exported against the supplied vanilla 99-bone third-person humanoid, using sample 0 of the supplied standing idle. First person has a held visual and uses the shared timing without new HKX clips. |
| Movement | Open Animation Replacer evaluates a player-only condition. The four carry clips cover the standing idle, walk/run direction slots, and forward sprint in the standard third-person graph. Vanilla speed and stamina remain in control. |
| Animation actions | Animation Injector v6 XML starts two actions and routes the original annotations to SKSE. Third-person actions use the injector's normal full-body idle routing. |
| Reference support | A crosshair-targeted inventory-type world reference with one loaded Havok body. Eligible resting weapons on weapon or clutter layers can resume dynamic motion at release. Supported equipment includes one-handed melee weapons, staves, crossbows, backpacks, and head, hand, and foot armor. Quest objects, blocked references, and multi-body objects are skipped. |
| Input | The device dispatch hook chains the existing handlers. Its call site is the one used by Menu Framework 3 on 1.6.1170. A validated PlayerControls virtual hook serves as a fallback. Animation Injector provides the one-shot start and marker callbacks. |
| Saves | Current hold becomes a safe recovery at the original position when loading the cosave. Saves taken with the mod active should retain its SKSE co-save. |

OAR priority and ties
---------------------

The Throw Object / Carry submod uses OAR's maximum signed 32-bit priority, 2147483647. Conditions require player reference 000014 and the ThrowObject.esm carry global to equal 1. Blend-on-interrupt and re-evaluation on loop/echo allow other active candidates to resume when the condition clears. Pickup and Throw instead enter custom Animation Injector actions with uniquely named HKX files. OAR priority and interruptible setting (https://www.nexusmods.com/skyrimspecialedition/mods/92109) · Animation Injector v6 guide (https://github.com/skypia0147-dev/Animation-Injector/blob/main/XML_GUIDE_EN.md)

OAR sorts candidates by integer priority and gives no exclusive reservation or supported tie override. Use OAR’s animation log to identify the actual selected submod, or run python tools/check_oar_priority.py <Data> from the source archive against the merged Data tree. Run it through MO2 when MO2 supplies the merge. The checker reports equal/higher priorities only where the clip paths overlap, including legacy conditional replacers. If an equal-ranked submod is active there, set its OAR priority below 2147483647 in the profile. The package makes no changes to third-party mods. Behavior graphs that introduce completely different locomotion clip slots can fall outside the shipped replacements. OAR source (https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/ReplacerMods.cpp)

The ESM layout follows the Skyrim SE xEdit definitions for TES4 flags, HEDR 1.71 and GLOB fields. The stable local ID belongs to the OAR condition and the DLL. xEdit TES5 definitions (https://github.com/TES5Edit/TES5Edit/blob/0df45f4654260c90dd324e23cc3ceac07fd6d3a1/Core/wbDefinitionsTES5.pas) · esplugin (https://github.com/Ortham/esplugin)

Known issues and runtime gates
==============================

- v0.0.5 matches the checked Anims9 joint directions and palm orientations after HKX decoding. Skyrim's behavior blending and the installed character's skinning still need live verification. Hover clearance uses bounding geometry, so the amount of visible gap can vary by mesh.
- Launch clearance rejects initial paths through the thrower. A throw stays held if there is no safe release point. Custom self-damage can occur when a launched object leaves the player's space and then returns to hit them. Skyrim still processes its own collision physics. The cause of earlier unexplained self-hits needs a matching runtime trace.

- The cloud exported all six clips with the supplied vanilla skeleton.hkx and mt_idle.hkx. Each output read back as an x64 packfile with the correct 99-bone binding. The cloud has no running Skyrim installation, so playback and all gameplay checks require a runtime test. The original inputs and synthetic fixtures are excluded from all packages.
- The pickup clip reaches toward the ground. A shelf or table reference within range is technically eligible, and its visual may jump to the hand at grab because the authored reach is fixed. The held position, size corrections, and first-person offsets need checks on real meshes and armor.
- v0.0.3 carries static render meshes. A visible skinned, animated or otherwise unsupported leaf, a leaf with its own collision attached, or a missing renderer causes pickup to cancel. Static meshes may still carry successfully when the world collision is on an enclosing node. The preexisting single-body eligibility rule still applies.
- The v0.0.3 log confirmed the input thread rejection for G and Numpad 5. It did not establish why the reported third-person native grab flung objects. v0.0.4 reserves Skyrim's Activate binding when the player has no Throw Object item and logs native grabs. Third-person positioning and physics still need live verification.
- A third-person annotation is delivered by Animation Injector and consumed on the main gameplay update. A ref that needs a fresh Havok body after re-enabling gets up to 500 ms to initialize. A timeout leaves that same reference enabled at the release location. Contact and orientation behavior need verification on both camera perspectives.
- Anims9 has forward full-body carry cycles. The standard directional walk/run slots reuse those clips. Strafing and backing may show foot sliding. Custom behavior graphs with new clip names and same-maximum OAR submods need the checks in the Compatibility section above.
- Carry continues across exterior cell boundaries in the same worldspace. Temporary equipment, animation, and 3D interruptions can hide the visual until the hand is available again. Loading a different interior or worldspace restores the parked reference. Loading with the matching SKSE co-save recovers it and clears the carry pose. Discarding the co-save or uninstalling during a hold can strand a disabled reference.
- The native integration listens for rigid-body contacts and character proxy object interactions. Confirm coverage for living actors, ragdolls, and different skeletons on 1.6.1170 before relying on NPC hits. Drop never registers a flight for damage or bounty.
- The mod uses the real violent bounty field and does not send a second vanilla assault alarm. A guard's response, witness handling and 10-gold thresholds still depend on the game and other crime mods. Actual guard approach, arrest and surrender need verification. Bethesda Skyrim game manual (https://assets.ctfassets.net/rporu91m20dc/1poliwondakI2QwKQMY2QI/b79ecee4541dd1ba7c7a85b453f0e9b2/manual_skyrim-le_pc_en-us.pdf)
- Sneak suspicion audio requires an installed matching recording and a loaded NPC. The clip uses Skyrim's 3D dialogue output. It can play without lip animation or subtitles. An actor with no matching recording can still flinch. Standing warnings use the existing vanilla dialogue topic. The normal weapon, tolerance, bounty, and escalation checks still apply to hits.

- The v0.0.1 code could keep applying flight drag to a just-thrown reference that Skyrim grabbed. v0.0.2 exits that path. The reported third-person behavior with ordinary E and an untouched object has no confirmed cause from the cloud. The regression checklist compares a fresh object's native behavior with the mod enabled and disabled in both perspectives.

In-game design
--------------

- G selects an eligible live world reference from the crosshair or a camera collision pick. The ground item stays visible until ThrowObjectGrab at frame 22.
- The mod parks the same reference while a render-only representation hovers over the right hand. Object bounds and per-user socket options determine its placement. The real reference retains ownership, scale, count, and its other reference data. Owned objects use the engine theft alarm with the original owner.
- The authored clips replace the standing idle and covered walk, run, and sprint slots during carry. The vanilla locomotion graph continues to select movement and stamina behavior. There is no timed hold expiration.
- E drops with zero launch velocity. Items release from the hand in third person, with a nearby hand-height fallback for equipment. First-person drops prefer a visible point beneath the crosshair and fall back to a clear spot near your feet. Left mouse releases at ThrowObjectRelease on frame 15. Third-person launch prefers the hand and can use a nearby clear origin. First-person launch prefers the center of the view and uses a clear forward or hand position when needed. The aim ray sets the target, and a low ballistic arc compensates for gravity when the target is reachable. Normal activation and attack bindings pass through while the mod has no eligible action.
- First person uses the hand node when available and a camera-relative socket otherwise. The matching 30 FPS timings drive first-person grab and release without adding first-person clips.
- The game update processes hits from the Havok listener, character proxy, or a sweep of the object's actual travel. Hits are deduplicated per throw and target. Damage is capped and scales with impact speed and item weight. Assault modifies the victim's eligible real crime faction, defaulting to 10 gold. Guard recognition and surrender still depend on vanilla behavior.
- Near-miss flinches and warnings depend on closest approach, line of sight, actor state and the reaction cooldown. The default voice line uses a vanilla DialogueGeneric disturbance topic. Vanilla topic conditions determine whether the NPC has a suitable line.
- The SKSE Menu Framework 3 page saves each keybinding independently along with physics, damage, crime and handling settings. The INI remains available if the menu DLL is absent.

Throw range and item weight
---------------------------

The default base throw speed setting is 13 m/s, and the v0.0.8 launch tuning applies a 1.25 multiplier before the existing horizontal range extension. The same settings apply to every item. The flight update evaluates drag in base-velocity space and applies the v0.0.8 drag reduction, so compact, dense objects still tend to travel farther. Ballistic aim accounts for the new motion when targeting an actor. NASA Glenn flight equations with drag (https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/flight-equations-with-drag/)

Quadratic drag uses an approximate cross-sectional area, an item-type coefficient, and inverse gameplay weight. Compact, dense objects tend to coast farther than light or bulky ones. Havok still handles gravity and collisions. Actual landing distances depend on release height, terrain, and obstacles. The release physics applies only to eligible items picked up through Throw Object. Head, hand, and foot armor get launch and drag adjustments based on their shape and material, while one-handed weapons use a compact item profile. Staves, crossbows, and backpacks use their own launch and drag adjustments. The crossbow itself is the thrown object, and its impact uses the item's weight and speed. Existing loose clutter keeps its original tuning.

Changes in v0.0.14
------------------

- Show tolerance meter and Tolerance meter corner apply when changed. Close the settings menu and aim at a nearby non-hostile actor to see their name and hits over threshold. The saved default is off.
- The meter is a non-interactive Menu Framework overlay that works independently of the vanilla HUD menu flag. If the crosshair has no actor handle, a camera collision query can resolve the target. ThrowObject.log identifies HUD registration and the first draw.
- The v0.0.13 save and tolerance record formats carry forward. See CHANGELOG.txt for the complete release history.

Changes in v0.0.13
------------------

- Nearby non-hostile adults, children, followers and animals count one confirmed hit per throw. Each rolls a default daily threshold of 3–7. The counter advances on impact and resets at 6 AM in Skyrim. Same-day rolls persist across travel and save reloads.
- Hits before the threshold can do nonlethal damage without attacker credit, assault bounty or a combat request from Throw Object. The threshold brings back the existing assault and combat response, subject to the game's rules for guards, children and allies. Hostile actors, self-hits and theft keep their existing behavior.
- Change the bounds in Throw Object → Settings → NPC tolerance using SKSE Menu Framework, then choose Apply and save gameplay settings. Minimum daily tolerance supports 1–10. Maximum supports values up to 20 and stays at least as high as the minimum. New rolls use the saved bounds.
- Show tolerance meter is off by default. Turn it on and select a corner under NPC tolerance. While looking at an eligible actor, the HUD shows confirmed hits and the threshold, for example Nazeem TM: 0/7 or Lydia TM: 3/4. Displaying a roll never adds a hit.
- The counter uses loaded actors in the same interior, the same enclosed city worldspace, or within 4,096 game units in a shared exterior worldspace. It needs no background script or standing NPC scan. The full release history is in CHANGELOG.txt.

Changes in v0.0.12
------------------

- First-person throws try the centered release first and use an earlier forward or hand launch position if the centered path will not fit. An offset launch still aims toward the crosshair. Player-body identification avoids counting the player's own capsule as scenery. If every candidate is blocked, the item stays held.
- Save data and the master are compatible with v0.0.11. See CHANGELOG.txt for the complete release history.

Changes in v0.0.11
------------------

- First-person throws start on the rendered crosshair ray. The release checks along that ray for camera, player and scenery clearance. A reachable target can use the low ballistic arc, and a blocked release keeps the item held.
- The six animations, third-person hand release, drop and carry behavior, save format and impact rules carry forward from v0.0.10.
- CHANGELOG.txt covers the original mannequin and preview, Anims1 through Anims9 and every gameplay release.

Changes in v0.0.10
------------------

- The v0.0.9 runtime log shows that G still found the same Golden Frog after release, and the old range check blocked every retry. Pick Up now measures against the live Havok body at keypress and at the grab marker. The range limit still applies, with a small allowance for the object's size. The log records the live and stored distances when either is outside the configured range.
- The same check applies to every eligible object. The v0.0.9 plugin data, save format, controls, grip, throw physics, and NPC reactions carry forward.

Changes in v0.0.9
-----------------

- Golden Frog 1.0.1 defines a weight-1 miscellaneous inventory item with one convex Havok body. Pick Up supports eligible runtime-spawned loose items when their loaded physics permits movement and uses a camera collision pick when the crosshair has no eligible reference. Picking up the item ends tracking of its previous throw.
- First-person release checks the existing socket, then tries nearby clear positions in front of the player when necessary. The v0.0.8 third-person grip, default throw tuning, six animations and marker frames carry over.
- Actual NPC hits use Skyrim's native actor damage with player attribution. A living NPC hit can flinch and enter combat. Visible near misses can trigger a lighter flinch and a warning without damage or bounty. CommonLibSSE-NG Actor::DoDamage (https://github.com/alandtse/CommonLibVR/blob/ng/src/RE/A/Actor.cpp) · Skyrim Actor.StartCombat (https://papyrus.bellcube.dev/skyrimse/script/actor/function/startcombat/)
- A throw can hurt the player after it leaves their space and comes back with meaningful speed. A sufficiently fast impact with a metal item of weight 4 or more, or any item of weight 8 or more, can also knock the player down. Impact Damage controls both self and NPC damage.
- Existing v0.0.8 saves use the same master, form IDs, settings and SKSE recovery record. See Installation below and TESTING.md in the source archive.

Changes in v0.0.8
-----------------

- Carry continues across exterior cell seams in the same worldspace. A missed pickup-end event now completes the carry if the item was already attached. A temporarily unavailable hand or blocked player state pauses the visual and allows it to reattach.
- Third-person placement follows the opening formed by the thumb, index, and middle fingers. Larger items keep a limited outward clearance. Throws and drops start from the updated grip position.
- All throws start with 25% more speed and 40% less custom drag at the same settings. Free throws follow the aim directly, and shots at an actor retain ballistic compensation.
- NPC hits use native health damage with immediate health readback. Contact speed can fall back to the previous flight sample after a collision has slowed the rigid body. The flight checks use the raw motion transform and skip the thrown body's own collider. Stealing still uses the original pickup and bounty path. CommonLib native actor-value implementation (https://github.com/alandtse/CommonLibVR/blob/cce63b4c8aacd2766c21f44b408df92a3ad9e95c/src/RE/A/ActorValueOwner.cpp)
- v0.0.7 saves use the same plugin and SKSE co-save format. Loading a save made during carry recovers the original world reference through the existing recovery record. See Installation below for the upgrade procedure and TESTING.md in the source archive for the in-game checks.

Changes in v0.0.7
-----------------

- The third-person hover moves one more game unit outward along the existing forward-right offset, with the same clearance calculation. Drop and throw start from the revised socket.
- Every eligible throwable uses twice the v0.0.6 horizontal range in unobstructed flight. Each object retains its weight, shape and drag behavior. Direct shots at actors still use ballistic aim assistance.

Changes in v0.0.6
-----------------

- All six clips now preserve Skyrim's neutral shoulder sockets. The exporter applies the Anims9 clavicle motion around those sockets and keeps the arm and palm directions. In Hold frame 0, decoded shoulder span is 27.51 game units, down from 31.32 in v0.0.5 and within 0.01 units of vanilla idle. See ANIMATION_CALIBRATION.md for the frame comparison.
- The third-person hover sits closer to the palm and shifts forward and to the character's right. The amount of shift scales with the held mesh, and clearance follows the palm, fingers, and tilted object bounds. Throw and drop start at the updated hover socket.

Changes in v0.0.5
-----------------

- Re-exported all six Anims9 clips with model-space anatomical retargeting and parent-local Skyrim transforms. The per-frame comparison in ANIMATION_CALIBRATION.md documents the former pose errors and both custom handoffs.
- The held visual hovers over the right palm. Clearance accounts for rotated object bounds and available finger landmarks. Drop and throw move the same world reference to that exact socket. A launch that starts too close to the thrower's body cancels safely.
- NPC impacts now have a fallback that checks actual launched-object travel against eligible actor capsules and requires a clear path. Each throw can damage a given target once through Skyrim's Actor.DamageActorValue call. Papyrus Index (https://papyrus.bellcube.dev/skyrimse/script/actor/function/damageactorvalue/)

Changes in v0.0.4
-----------------

- The reported v0.0.3 log showed that Skyrim delivered keyboard input on a separate thread and the plugin discarded it. The input callback now queues button requests. The player update checks the live target and starts the animation before handling the original grab marker.
- Native activation passes through with empty hands in either camera view. The Skyrim Activate binding takes precedence over a conflicting custom Pick Up binding. During a native grab, custom carry poses and actions are inactive. The physics contact hook is deferred until Throw Object launches an item.
- The collision-free carry visual from v0.0.3 is retained. New log entries distinguish button receipt, player-update execution, native grab, and successful hand attachment.

Changes in v0.0.3
-----------------

- Pickup builds a collision-free hand visual from static render geometry. If visualization cannot be prepared, the original world reference stays available. The original reference still handles drop and throw.
- The archived v0.0.2 crash addresses resolve to the old pickup's NiObject::CreateDeepCopy call. CRASH_ANALYSIS.md records the symbolication, crash path, and verification status. The v0.0.3 build has separate matching PDB symbols.

Changes in v0.0.2
-----------------

- Keyboard and mouse gameplay actions run from the ordered SKSE input dispatch path. Startup logs identify the event hook, exact missing dependencies, current bindings, crosshair eligibility, and Animation Injector's response. Startup and button diagnostics are flushed as they are written.
- Rebind, Cancel, Clear Binding, and Reset to Defaults replace the numeric key fields. Changes apply immediately and persist in ThrowObject.user.ini. A captured button's press and release stay inside the settings interface.
- OAR applies the four carry clips at signed 32-bit priority 2147483647 while the player has a Throw Object reference. Dedicated Animation Injector actions retain the original pickup and throw timing. The active state clears on release, cancel, or load.
- Holding E to drag an object remains a Skyrim action in either camera view. Throw Object declines its own pickup during a native grab. If Skyrim grabs a recently thrown reference, pending release physics and flight tracking stop for that reference.

Source and verification
-----------------------

- src/ contains native gameplay, configuration, flight and menu code.
- mod/ is the MO2 staging tree. tools/make_esm.py builds its sole new record, ThrowObjectCarryEnabled at local FormID 0x800.
- animation/ThrowObjectAnims9/ contains the original editable Blender actions, preview, and PyNifly export pipeline.
- tools/stage_animations.py checks six HKX packfiles and installs the corresponding conditional locomotion copies. tools/check_oar_priority.py reports overlapping priorities in a visible Skyrim Data tree.
- The source archive includes TESTING.md with cloud checks and in-game acceptance tests, plus ANIMATION_CALIBRATION.md and CRASH_ANALYSIS.md. See Installation, Compatibility, and Known issues and runtime gates in this README.

Build the DLL and run the portable tests
----------------------------------------

CMake 3.25 or later pins CommonLibSSE-NG, spdlog, rapidcsv and the menu SDK by revision. On an x64 Windows build runner with current Visual Studio C++ tools, use

cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix mod --component ThrowObjectRuntime
python tools/make_esm.py

On a Linux build agent, use the supplied cmake/CloudWindows.cmake with LLVM 19, Ninja, and Microsoft CRT/Windows SDK headers and libraries staged by xwin. Point THROW_OBJECT_LLVM to the directory containing clang-cl, lld-link, llvm-lib and llvm-rc. Point THROW_OBJECT_XWIN to xwin's splat root. Then run

cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DCMAKE_TOOLCHAIN_FILE=cmake/CloudWindows.cmake
cmake --build build-win --target ThrowObject
cmake --install build-win --prefix mod --component ThrowObjectRuntime
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure

After staging the six exports, package the MO2 archive, sources and paired Windows symbols with python tools/package_release.py releases --pdb build-win/ThrowObject.pdb. For a Visual Studio build from the commands above, use build/Release/ThrowObject.pdb.

The Windows workflow in .github/workflows/windows.yml is configured for x64 builds on request. The cloud used Linux cross-compilation. The dependency sources and Microsoft SDK are fetched separately. CommonLibSSE-NG (https://github.com/CharmedBaryon/CommonLibSSE-NG) · xwin (https://github.com/Jake-Shadle/xwin) · SKSE Menu Framework API (https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/Usage.md)

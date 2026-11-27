# Testing

## v0.3.9 RC3 skinned crossbows

- Repeat the failed RC2 pickup with the Iron Crossbow and the modded crossbow. The RC2 log identified the main skinned BSTriShape leaves and showed no standalone renderer. In RC3 check that both crossbows appear when picked up in first and third person. Switch views, then drop, throw, and pick the original reference up again.
- Reopen a paired v0.0.15 save and SKSE co-save. Verify the prior held-reference recovery and tolerance counts. Save and reload while holding a crossbow in RC3 and verify that the original item is restored to the world.
- The host regression checks cover a detached skinned geometry leaf, missing partition data, and a collision-bearing leaf. The renderer and game save reload require Skyrim 1.6.1170.

## v0.3.9 RC2 crossbows and one-handed near misses

- RC1 log reproduction: pick up the objects with base forms 0200D0C3 and 223DD3B6 in the reporting load order. Both reached the grab frame and failed in render staging. In RC2 verify the full model appears in either camera view. Drop, throw, and pick up the original crossbow again.
- Throw a sword, dagger, war axe, and mace past an eligible NPC while hidden. Verify that a guard who hears the pass still investigates if the weapon bounces away from the original hearing radius. Test a first landing near an NPC with no close pass, standing throws, and throws with the sneak eye open.
- Repeat with a direct hit or a bounce that strikes a witness. Verify that the hit NPC aggroes, and the NPC who was missed can investigate.
- Load existing v0.0.15 and RC1 saves with their original co-saves. Confirm that tolerance data loads and a held object is restored. Save and reload while carrying a crossbow, then compare charge, tempering, and ownership on the restored reference.
- The RC1 log showed a successful suspicion voice followed by zero weapon witnesses at rest. The host regression checks cover the pass radius and direct-hit exclusion, plus the controlled render-leaf policy. In-game behavior requires Skyrim.

## v0.3.9 RC1 release checks

- Pick up a Creation Club backpack and a mod-added backpack with G. Try both cameras and re-pick up the same object after dropping or throwing it. Check that a slot-47 cloak or lantern stays ineligible.
- Repeat with a Dawnguard crossbow, an enchanted or tempered crossbow, and a mod-added crossbow. Verify the thrown weapon returns as the original reference with its extras. Compare with a bow and two-handed melee weapon to verify the eligibility boundary.
- With the 50% melee-swing option on and off, throw either new category at an eligible target. Their impact should use the weight-and-impact calculation. A backpack can distract during a hidden throw. A crossbow uses the existing thrown-weapon reaction.
- With the sneak eye closed, land a sword, dagger, war axe, and mace near an NPC. Verify suspicion, the walk and kneel, and resumed AI. Repeat with a direct hit, including a bounce into the original target, and verify combat for the NPC who was hit. Try a near miss while detected and verify it starts no inspection.
- Run the v0.0.15 checks for carry across cells, ordinary Activate, theft, impact, same-day tolerance, first- and third-person release, and save/load while holding. Load a v0.0.15 save and a v0.3.8 save. Check the original held object and tolerance counts after recovery.
- The master and existing save payloads are fixture-checked in the build workspace. Pickup, clearance, appearance, and save migration still need Skyrim 1.6.1170.

## v0.3.8 one-handed grips and staff pickup

- In third person pick up an iron sword, dagger, war axe, mace, and pickaxe. Check the palm around the handle, the facing of the blade or head, and the original model size. Compare an iron mesh replacer with a working non-iron weapon.
- Pick up the Dragon Priest staff (base 00029B82) and a regular staff with G in first and third person. Verify ThrowObject.log reports a cloned solid render leaf and a completed grab. Drop, throw, and pick it up again. Confirm its original charge and physical throw damage.
- Drop a single arrow and an arrow stack in third person and verify their counts. Load a v0.3.7 save. Save while holding a staff and verify recovery of the original reference after reloading.

The grip tests cover a replacer rotated away from the record's axis, an
offset handle, and the fallback for models without readable triangles.

## v0.3.7 grips, staves, and ammunition drops

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Iron grips | Carry iron swords, daggers, maces, and war axes in third person. Check that their handles meet the palm while standing and walking. Compare ordinary steel or Nordic grips and a woodcutter's axe, fork, and knife with v0.3.6. |
| Pickaxe and spear | Confirm the pickaxe haft crosses the palm. Confirm the Riekling spear is held by the shaft with its point away from the arm. Check full world model scale before and after dropping. |
| Staff | Pick up, drop, throw, and recover a regular staff and a staff with attached magic effects. Compare throw damage with the melee toggle on and off. The staff should deal the same physical damage for equivalent hits. |
| Arrow drops | Drop a single Steel Arrow and a stack in third person. Verify the original amount can be picked up after the drop. Repeat against a wall or next to furniture where the ground fallback may be needed. |
| Save upgrade | Load a v0.3.6 save. Save while holding an arrow stack or staff and reload. Check recovery of the original reference, stack count, charge, and existing tolerance data. |

The host suite checks fitted grip points and the shape-aware clearance
case for a long arrow bundle. The grip alignment, asset compatibility and
save/reload checks require Skyrim.

## v0.3.6 leaving furniture and package idles

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Furniture | While hidden, toss an object near an NPC cooking, smithing, using a workbench, or sitting. Verify that they exit the station, approach the item, kneel for 4–7 seconds, stand up and resume their normal routine. |
| Invisible markers and idles | Repeat with a table or rail lean, and with a reading or writing idle outside a scripted scene. Test an NPC with a nearby landing and another who needs to walk after exiting. |
| Fallback and cancellation | Try a slow furniture exit. Verify the actor is free before approaching. Pick up the thrown item during an exit and check that the actor can resume their schedule. |
| Upgrade and save | Reopen a v0.3.5 save. Save during a furniture exit, during the walk and during the kneel. Reload and check the prior carry/tolerance data and that the NPC is again controlled by its own schedule. |
| Settings and eligibility | Test the new default, a migrated legacy 6–9 pair, and a saved custom range. Confirm that standing and detected throws do not start a distraction. |

- [Skyrim Creation Kit furniture record](https://tesck.ru/index.php/Furniture)
- [Vanilla lean and craft markers](https://skyrimeasy.blog-rpg.com/ckworks/furniture01)
- [Native furniture get-up and quick exit bindings](https://ng.commonlib.dev/Actor_8h_source.html)

## v0.3.5 kneel after approach

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Guard with a sheathed weapon | Toss an ingot while hidden. Confirm the guard walks to the item, keeps the weapon sheathed, and kneels at the landing spot. |
| NPC with a drawn weapon | Verify that the NPC finishes sheathing before kneeling. Verify their previous stance and routine resume on release. |
| Hold and stand | Confirm `kneeling pose reached` in ThrowObject.log. Time the hold from the kneeling pose. Check the stand-up and the return to normal AI. |
| Interrupt | Start actual combat or pick the object up before the hold ends. Confirm the investigation releases the NPC. |
| Upgrade and save | Load a v0.3.4 save. Save and reload during the walk and again during the kneel. Verify the NPC's normal AI and the existing carry and tolerance data. |

Startup should report `32/32 travel packages` and a nonzero neutral hold
package ID. The neutral hold is a private instance of Skyrim's
HoldPosition template. Its general flags leave both weapon flags clear.
The host tests cover pose height confirmation. The visible animation
still requires a game check.

- [HoldPosition template data and vanilla Weapon Drawn flag](https://github.com/bryantmh/tes4skyrim/blob/master/tes5_import/packages/stand_still.py)
- [Kneeling graph events](https://elderscrolls.fandom.com/wiki/Console_Commands_(Skyrim)/Idle_Animations)

## v0.3.4 priority investigation packages

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Setup | Use the ESM and DLL from the same release. ThrowObject.log should show `32/32 travel packages` and a nonzero hold package ID. |
| Walking witness | While hidden, throw a gold ingot past walking Commander Caius so it stops away from his route. Look for `travel package selected`. Verify that he turns, walks to the actual landing point, kneels, and resumes his route. Repeat with a hit below the tolerance limit. |
| Stationary witness | While hidden, use a near miss beside a Dragonsreach entrance guard. Verify the down-to-the-knees pose, the hold at the landing point, the stand-up, and the guard's return to their station. |
| Multiple NPCs | Distract two eligible NPCs at once, then throw a second object while they inspect. Their initial investigation should continue. |
| Normal priority and exceptions | Repeat with a customized patrol, a guard in combat, and an active scripted scene. The patrol yields temporarily. Combat and the scene keep control. |
| Save upgrade | Load a v0.3.3 save. Save during the walk and during the kneel, reload, and verify the NPC resumes their usual package. Check the carry global, existing settings and tolerance counts. |

The startup check resolves the Travel template Location input and the
vanilla Hold Position package before installing the selection hook.
The `kneel graph event accepted` message reports the engine's response
to the event. Verify the visible pose in Skyrim.

References

- [Native package selection and movement research](https://github.com/marthofdoom/APMF/blob/main/native/channels/Travel.cpp)
- [Kneeling graph entry and reset report](https://www.reddit.com/r/skyrimmods/comments/jfot2d/)
- [SKSE save notification order](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_SaveLoad.cpp)

## v0.3.3 walking witnesses and kneeling exit

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Walking witness | Crouch with the sneak eye closed and hit walking Commander Caius with a gold ingot. If he walks past the object during the flinch, look for `approaching object` in ThrowObject.log and verify he returns to it. |
| Stationary guard | Hit a stationary Dragonsreach entrance guard below the tolerance limit. Verify that they go down on their knees and stand after the logged hold duration without leaving the cell. |
| Competing AI | Try a modded patrol or sandbox package and throw a second object during the investigation. Verify that the NPC finishes inspecting the first one and resumes their own routine. |
| Save and detection | Save during a kneel, then load again. Repeat with a closed-eye near miss, an open sneak eye, while standing, and with a one-handed weapon. Only eligible hidden throws should start investigations. |

The installed vanilla IDLE records used by this build are
`IdleKneelingEnter` (000E8E52) and `IdleKneelingExit` (000E8E53).
The runtime checks require Skyrim. The host tests exercise the
moving-witness route and bounded stagger wait.

## v0.3.2 investigate beside the NPC

Core tests cover the closed-eye restriction, proximity on one floor,
minimum recovery time after a flinch, and the quiet period before kneeling.

| Case | Runtime check |
| --- | --- |
| Direct hit | While hidden, hit a neutral guard with an ingot below their tolerance limit so it lands at their feet. Check `reaction queued a nearby check`, then `trying inspection in place` in ThrowObject.log. |
| Near miss | While hidden, throw past the guard so they flinch and the item lands beside them. Check that they face it and kneel in place. |
| Eye open and standing | Repeat the same throws with the sneak eye open and while standing. Verify there is no nearby investigation, even if the guard flinches. |
| Resume | Leave the guard for the configured kneel range. Verify their own AI takes over and the item remains available for pickup. |

## v0.3.1 investigation recovery and kneel settings

The v0.3.0 log showed a gold ingot eliciting suspicion dialogue and
hitting a guard without any investigation requests. Core tests now cover
a sleeping body without a contact, rolling contact, the flight-end
fallback, and sanitizing the kneel range.

| Case | Runtime check |
| --- | --- |
| Gold ingot | Repeat a hidden near miss and a direct hit below the tolerance limit. Follow `Distraction` and `Investigation` entries from witness through landing, walk and kneel. |
| Customized AI | Try a guard with a modded patrol or sandbox package. Verify temporary inspection and a return to the actor's usual routine. Throw a second object while they investigate. |
| Settings | Confirm the 6–9 second default, set both sliders to 12 seconds, apply and restart. Verify the next kneel is about 12 seconds and the values persisted. |
| Save | Save during walking and kneeling, then reload. Verify actors resume their packages and any temporary marker is cleaned up. |

## v0.3.0 environmental stealth distractions

The core test covers hidden and detected launches, excluded one-handed
weapons, a pass on either side of the trajectory, landing range, and
the quiet period before investigating. Skyrim runtime checks remain open.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Landing and near miss | While hidden, throw a bottle behind a guard, then try a faster pass that settles farther away. Verify the guard walks to the resting item, kneels, leaves it in place and returns to their post. |
| Multiple witnesses | Throw an item near several guards facing different directions. They should investigate concurrently. Try a second throw while the first group is still walking or kneeling. |
| Hostiles and direct hits | Test a non-combat bandit and a friendly NPC hit below their tolerance limit. Then test an NPC already in combat and a hit that triggers combat. Combat takes precedence. |
| Exclusions | Test a one-handed weapon, a throw with the eye open, and a standing throw. Confirm the v0.2.3 reactions. |
| Cleanup | Save while an actor walks or kneels, reload a v0.2.3 save, and pick up a distraction object during the approach. Confirm normal AI resumes and no temporary marker remains active. |

Native walking uses Skyrim's latent Actor.PathToReference function.
The kneeling idle requires a compatible actor animation graph. A
blocked navmesh path or incompatible idle ends the investigation.

## v0.2.3 sneak reaction dialogue

The core test covers hidden and detected crouching throws, detection by
another observer, standing before impact, and unavailable detection data.

| Case | Runtime check |
| --- | --- |
| Eye open | Crouch in front of a non-hostile NPC. Throw an ordinary item past them and then try a direct hit after the reaction cooldown. Listen for the existing collision lines. |
| Eye open from a bystander | Stay behind the reacting NPC while a second NPC can see you. Check that the reacting NPC uses normal collision dialogue. |
| Eye closed | Crouch out of sight and repeat with an NPC whose voice supports suspicion lines. Compare with the eye opening before the reaction. |

## v0.2.2 full-size weapons and dagger alignment

The core test checks that a scaled hand bone preserves the original
weapon size and that the dagger shaft stays perpendicular to the forearm.

| Case | Runtime check |
| --- | --- |
| Original size | Pick up the same sword, axe, mace, and dagger in third person and compare their scale before pickup and after drop. Include a long sword and a scaled world reference. |
| Dagger grip | Orbit vanilla and modded daggers in the Hold pose. The handle should sit inside the right palm with the blade clear of the wrist and sleeve. Walk and then drop or throw it. |
| Baseline | Confirm non-weapon third-person carry, first-person carry, throw spin, and v0.2.1 save loading. |

## v0.2.1 third-person weapon grip

The core test covers standard, reversed, centered and offset mesh pivots.
The existing clips were decoded against the Skyrim skeleton to verify the
right-hand weapon node during the hold, pickup, throw and carry cycles.

| Case | Runtime check |
| --- | --- |
| Weapon grip | Hold an axe, sword and dagger, including a modded weapon. Orbit the camera and check that the handle meets the palm. Check Hold, walking and sprinting. |
| Weapon release | Drop a one-handed weapon in open space and near obstructions. Check the hand-height fall and try a throw in each camera mode. |
| Existing carry placement | Hold and drop a helmet, gloves, boots and loose clutter. Switch between first and third person while holding a weapon. |

## v0.2.0 thrown weapon spin

The core test covers the angular rate, weapon length along each mesh
axis, a sideways release, and vertical aim. Live checks use Skyrim Steam
1.6.1170 with trajectory logging enabled for measured spin at release.

| Case | Runtime check |
| --- | --- |
| End-over-end spin | Throw a sword, dagger, war axe, mace, and pickaxe in both camera modes. Repeat with a modded weapon. Verify the blade or head travels around the center and the log reports around 2.5 rotations per second at release. |
| Existing releases | Drop a weapon in both views. Throw and drop a cabbage and helmet. Compare their behavior with v0.1.8. |
| Upgrade | Load a v0.1.8 save pair and confirm existing settings and same-day tolerance counts. |

## v0.1.8 third-person equipment drops

Tsoxer confirmed v0.1.7 first-person drops for equipment and ordinary
items and third-person drops for ordinary items. Third-person equipment
still produced the hand obstruction message. The portable regression
tests exercise nearby hand-height clearance for vertical and horizontal
weapons, helmets, boots, and gloves.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Open floor | Drop a long one-handed weapon, a pickaxe, boots, gloves, and a helmet. In third person verify that each original reference falls near the hand without a launch impulse. Try a scaled carry visual. |
| Obstruction | Repeat by a doorway or staircase. Clear alternatives should stay near hand height. A genuine wall or player overlap can leave the item held. The log records the chosen offset or a count of blocked positions. |
| Baseline | Check a first-person weapon drop and both camera modes with a cabbage. Confirm throwing, suspicion dialogue and v0.1.7 save loading. |

## v0.1.7 visible first-person drops

The portable tests cover a normal cabbage drop, near-plane clearance
for a long item, angled and invalid views, and a fall path through the
player capsule.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Visible first-person drop | Pick up a cabbage with G. In first person, look forward and press E. The item should appear just below the crosshair, fall without a push, and remain available for pickup. Repeat with a pickaxe and gloves. |
| Clearance and fallback | Drop next to a wall and look sharply up or down. Confirm that the item stays clear of the camera and the player. Check that a blocked visible origin uses the nearby floor placement when clear. |
| Baseline and upgrade | Check the original third-person release from the hand, both throw modes, and sneak suspicion. Load a v0.1.6 save pair and verify recovery and settings. |

## v0.1.6 camera-specific drops

Tsoxer confirmed v0.1.5 suspicion dialogue in live Skyrim and verified
that the nearby equipment drop worked. That placement made equipment
appear to jump from the hand in third person. The old first-person
clutter offset also put some drops farther away than expected.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Third-person hand drop | Drop a sword, pickaxe, boots, helmet, gloves, and a gold ingot. Compare the first visible falling frame with the item's held position. Try a long weapon on open ground and beside a wall. A true obstruction should leave the item held. |
| First-person floor drop | Drop the same items while looking level, up, and down. Verify the original reference settles near the player. Repeat by a wall and on Dragonsreach stairs. |
| Other behavior | Repeat the confirmed sneak suspicion case and both throw camera modes. Load a v0.1.5 save pair and verify held-item recovery, settings, and tolerance. |

## v0.1.5 equipment drops and spatial suspicion audio

The user confirmed the v0.1.4 pickaxe pickup and throw in Skyrim.
Ordinary item drops were also confirmed in both views. The v0.1.5
portable tests cover equipment release footprints and malformed voice
containers. The parser was also checked against a real compressed
xWMA sample wrapped in a test FUZ file.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Equipment drops | Drop a sword, dagger, pickaxe, helmet, boots, and gloves in both views. Repeat while looking up, looking down, rotating the camera, or facing a nearby wall. Confirm the equipment appears by the character on a clear surface and starts without throw velocity. Try the Dragonsreach stairs and a cluttered corner. |
| Ordinary items | Drop a gold ingot and a bucket in both views and verify their existing placement. Compare equipment throws with v0.1.4. |
| Positional suspicion | Repeat the reported Dragonsreach gold ingot throw while sneaking. Try a near miss and a direct hit below the tolerance threshold. Listen with the NPC on alternate sides and at different distances. Check for flinch, the NPC's own bark at their position, and continued play during and after playback. |
| Voice lifetime | Try a second throw while the clip plays. Leave the area or load a save mid-clip. Check that playback stops. Verify regular warning dialogue on standing throws. |
| Upgrade | Load paired v0.1.4 saves and co-saves, including a save with a held item. Verify recovery, bindings and the tolerance record. |

## v0.1.4 sneak stability and exact pickaxe eligibility

The supplied v0.1.3 log identified the standard Pickaxe as the rejected
base form. The portable tests cover that exact form ID, the unaffected
two-handed rules, and the existing resting weapon policy.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Sneak throw audio crash | Repeat the gold ingot throw near the NPCs in Dragonsreach. Try a near miss and a direct hit. Verify a flinch where applicable, no Throw Object suspicion voice, and continued play. Verify that an unaware NPC below the tolerance threshold does not enter combat from an ordinary hit. |
| Same placed pickaxe | Aim at reference 000DF1DE if present and press G in both camera modes. Confirm Pickup selected uses base 000E3C16, then drop, pick up again, and throw. Repeat with a freshly dropped standard pickaxe. |
| Controls | Throw a normal one-handed axe, check an excluded two-handed weapon, and compare standing reactions, Dragonsreach stairs, and native hold-E with v0.1.3. |
| Save upgrade | Replace the mod while Skyrim is closed, then load the paired v0.1.3 ESS and SKSE co-save. Check custom bindings, tolerance history, and held-item recovery. |

## v0.1.3 sneak throw crash and pickaxe pickup

The supplied v0.1.2 checklist passed seven of ten cases. Dragonsreach
stairs, clutter, bulky objects, modded one-handed weapons, MO2 metadata,
the toggle label, and existing saves all passed. The crash near the guard
blocked the three sneak reaction checks.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Guard crash reproduction | Sneak and throw clutter just past the Dragonsreach guard. Repeat with a direct clutter hit below the tolerance limit. Check for a flinch, the guard's own suspicion voice if available, and no forced player detection. Confirm the log shows a matching voice path when audio plays. |
| Missing recording | Repeat near an unaware actor whose voice has no matching Skyrim recording. Check that the flinch still plays and the game continues. Repeat standing for the regular reaction. |
| Pickaxe | Pick up, drop and throw a loose standard Pickaxe (base 000E3C16), including a newly dropped one. Pick up the same object again after it settles. If pickup still rejects a particular reference, the log now includes both reference and base IDs. Repeat with a normal axe, a two-handed weapon, and a quest-locked reference. |
| Verified v0.1.2 paths | Repeat a throw from the stairs and with a bulky item near clutter. Load a v0.1.2 save and its SKSE co-save. Check the carry recovery, settings, and existing tolerance. |

## v0.1.2 third-person release, suspicion audio, and weapon option label

Tsoxer confirmed nine v0.1.1 checks, including half-swing damage in
gameplay. Sneak suspicion dialogue still failed. Both v0.1.1 damage
formulas carry forward.

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Stairs and clutter | In third person, throw a small object up and down the stairs to the Dragonsreach throne. Try a bucket or other bulky loose object beside furniture. Check the bounds clearance log on fallback and confirm that a fully blocked origin leaves the item held. Repeat with the same object in first person. |
| Sneak suspicion | With reactions on, hit an unaware NPC with ordinary clutter, then test a close miss. Confirm the flinch, their own hearing or dismissive audio, and no ordinary standing warning. Check the log for the matching voice file. Repeat with two different voices, a creature without a recorded line, and a standing throw. |
| Weapon options | Confirm the updated label. Compare each damage mode against v0.1.1 with the same weapon, target, and settings. Verify weapon hits fill tolerance with both settings. |
| Upgrade and metadata | In MO2 verify author Tsoxer and the description in the plugin details. Load a v0.1.1 ESS/SKSE save pair and check input bindings, rolls, and recovery of a held item. Verify vanilla hold-E and dropped-item behavior in both cameras. |

## v0.1.1 sneak detection, release clearance, and tolerance

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Sneak detection | With reactions enabled, throw near and hit an unaware bandit from cover. Check for a flinch, the sneak detection event in ThrowObject.log, and the NPC's own search or dismissive bark when the engine selects a line. Repeat for an alerted actor, a town guard, and a standing throw. |
| Weapon release | Pick up ordinary and unusually long or wide mod-added one-handed weapons. Throw them in third person from open ground and in clear indoor spaces. Check for the third-person weapon clearance log when fallback was needed. Repeat beside a wall, behind a doorway, aiming into a ceiling, and in first person. The original reference should stay held if the release has no clear path. |
| Guard tolerance | Start with a friendly guard at 0/6. Hit the guard once with a one-handed weapon. Confirm the meter reaches 6/6 and the usual threshold response occurs in both damage modes. Repeat from a fresh save while sneaking and with another eligible friendly creature. Confirm that a non-weapon hit goes to 1/6. |
| Half swing | Compare a normal physical swing with an identical thrown weapon against comparable targets on the same difficulty. The throw should do half the physical damage, excluding enchantment damage. Sneaking should never add a damage multiplier. Test protected actors and a charged weapon separately. |
| Upgrade | Load prior v0.1.0 and v0.0.15 ESS and SKSE co-save pairs with the same MO2 profile. Check input settings, tolerance history, and recovery from an empty or held-item save. |

## v0.1.0 sneak reactions and thrown equipment

| Case | Runtime check on Steam 1.6.1170 |
| --- | --- |
| Sneak voice | Throw near and hit an unaware vanilla bandit with reactions on. Check that the NPC flinches and, if a line is available, speaks discovery or search dialogue in their existing voice. Repeat on actors with different voice types, a creature without a line, a target that already detected the player, and a standing throw. |
| Equipment | Carry and throw loose swords, daggers, war axes, maces, helmets, gloves and boots in both camera views. Compare leather and metal pieces. Try a greatsword, bow, staff, shield, cuirass, quest weapon and jewelry. Compare pickup/drop ownership and repeated pickup. |
| Weapon swing damage | Compare a normal melee swing and a throw of the same one-handed weapon with identical target, difficulty, skills, and perks. Repeat with a tempered weapon, a charged base enchantment, a custom enchantment, and a spent enchantment. Inspect remaining charge and recover the same object. Measure crouched and standing throws with melee sneak perks unlocked. |
| Damage toggle | Turn off Melee damage for thrown one-handed weapons. Verify normal physics damage and that the enchantment is not applied or drained. Repeat with global Impact damage disabled. |
| Tolerance | Confirm that a friendly NPC remains protected by tolerance before their threshold, including from the enchantment. Verify escalation, bounty and health safeguards. |
| Upgrade | Load the same v0.0.15 ESS and SKSE co-save pair with the new DLL. Verify custom keys, tolerance counts, and valid/stale carry recovery. Repeat with an empty hand, an object held, and a dropped weapon. |

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

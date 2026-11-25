"""Run with Blender 4.3+: blender -b -t 2 --python-exit-code 1 -P scripts/build_preview.py -- ./generated"""
import json
from math import degrees
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'source'))
from anatomy_qa import track_motion
from locomotion import GAITS, generate_carry_actions
from locomotion_qa import check_locomotion
from mannequin import build_mannequin
from motion import FPS, HOLD_END, RELEASE, RELEASE_EVENT, SOCKET, SOCKET_OFFSET, THROW_END
from motion import generate_actions, grip_fingers, setup_rig
from pickup import ACTION, GRAB_EVENT, GRAB_FRAME, PICKUP_END, generate_pickup_action

out = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.frame_start = 0
scene.frame_end = HOLD_END
rig = setup_rig()
model, socket = build_mannequin(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
actions = generate_actions(rig)
carry_actions, legs, ground = generate_carry_actions(rig, model)
actions.update(carry_actions)
actions['PickUp'] = generate_pickup_action(rig,actions,legs)
animation = rig.animation_data
metrics = {}


def attach_action(label, frame):
    action, slot = actions[label]
    if animation.action != action:
        animation.action = None
        for bone in rig.pose.bones:
            bone.matrix_basis = Matrix.Identity(4)
        grip_fingers(rig,.30,'L')
        animation.action = action
        if slot is not None:
            animation.action_slot = slot
    scene.frame_set(int(frame), subframe=frame % 1)
    bpy.context.view_layer.update()


def snapshot(label, frame):
    attach_action(label, frame)
    bones = {bone.name: bone.matrix.copy() for bone in rig.pose.bones}
    location = socket.matrix_world.translation.copy()
    pivot = rig.pose.bones['Hand.R'].matrix @ Vector(SOCKET_OFFSET)
    assert (location - pivot).length < 1e-6, "Socket has shifted from its specified local offset"
    return bones, location


def rotation_difference(a, b):
    return min(a.rotation_difference(b).angle, 6.283185307 - a.rotation_difference(b).angle)


hold_first, hold_pos = snapshot('Hold', 0)
hold_last, hold_last_pos = snapshot('Hold', HOLD_END)
throw_first, throw_pos = snapshot('Throw', 0)
assert (hold_pos - hold_last_pos).length < 1e-6
assert (hold_pos - throw_pos).length < 1e-6
for bone, start in hold_first.items():
    assert (start.translation - hold_last[bone].translation).length < .00001
    assert rotation_difference(start.to_quaternion(), hold_last[bone].to_quaternion()) < .001
    assert (start.translation - throw_first[bone].translation).length < .00001
    assert rotation_difference(start.to_quaternion(), throw_first[bone].to_quaternion()) < .001
pickup_last,pickup_pos = snapshot('PickUp',PICKUP_END)
assert (pickup_pos-hold_pos).length < 1e-6
for bone,start in hold_first.items():
    assert (start.translation-pickup_last[bone].translation).length < .00001
    assert rotation_difference(start.to_quaternion(),pickup_last[bone].to_quaternion()) < .001
pickup_first,_ = snapshot('PickUp',0)
pickup_grasp,grab_pos = snapshot('PickUp',GRAB_FRAME)
pickup_lift = snapshot('PickUp',GRAB_FRAME+4)[1]
assert pickup_grasp['Pelvis'].translation.z < pickup_first['Pelvis'].translation.z - .48
assert .037 < grab_pos.z-ground < .065
assert pickup_lift.z > grab_pos.z + .10
max_pickup_foot_drift = 0.0
max_pickup_elbow_step = 0.0
previous_elbow = None
previous_lift = None
for sample_index in range(4*PICKUP_END+1):
    pose,socket_pos = snapshot('PickUp',sample_index/4)
    if sample_index >= 4*GRAB_FRAME:
        if previous_lift is not None:
            assert socket_pos.z >= previous_lift-.001, "The prop dips during the lift"
        previous_lift = socket_pos.z
    if sample_index % 4 == 0:
        elbow = pose['UpperArm.R'].to_quaternion().inverted() @ pose['Forearm.R'].to_quaternion()
        if previous_elbow is not None:
            step = degrees(rotation_difference(previous_elbow,elbow))
            max_pickup_elbow_step = max(max_pickup_elbow_step,step)
        previous_elbow = elbow
    assert pose['Root'].translation.length < 1e-6
    for foot in ('Foot.L','Foot.R'):
        error = (pose[foot].translation-pickup_first[foot].translation).length
        max_pickup_foot_drift = max(max_pickup_foot_drift,error)
assert max_pickup_foot_drift < .005
assert max_pickup_elbow_step < 4, "The pickup elbow changes too abruptly"
metrics.update(
    pickup_grab_position_m=list(grab_pos),
    pickup_ground_y_m=ground,
    pickup_max_planted_foot_drift_m=max_pickup_foot_drift,
    pickup_max_elbow_step_degrees=max_pickup_elbow_step,
)
shoulder = hold_first['UpperArm.R'].translation
elbow = hold_first['Forearm.R'].translation
wrist = hold_first['Hand.R'].translation
metrics['hold_elbow_flexion_degrees'] = degrees((elbow-shoulder).angle(wrist-elbow))
assert 35 < metrics['hold_elbow_flexion_degrees'] < 65
metrics['hold_socket_max_travel_m'] = max((snapshot('Hold', f)[1] - hold_pos).length for f in range(HOLD_END+1))
points = {f:snapshot('Throw', f)[1] for f in range(THROW_END+1)}
metrics['release_velocity_m_s'] = list((points[RELEASE]-points[RELEASE-1]) * FPS)
assert metrics['release_velocity_m_s'][1] > 2, "Release velocity should point forward"
_, idle_pos = snapshot('Throw', THROW_END)
metrics['throw_ending_socket_m'] = list(idle_pos)
for frame in (0, 8, 13, 14, 15, 16, THROW_END):
    bones, _ = snapshot('Throw', frame)
    for name in ('Root','Foot.L','Foot.R'):
        assert (bones[name].translation - throw_first[name].translation).length < .00005
    metrics.setdefault('palm_space_fingertips', {})[str(frame)] = [
        list(bones['Hand.R'].inverted() @ (bones[name] @ Vector((0,rig.data.bones[name].length,0))))
        for name in ('Thumb3.R','Index3.R','Middle3.R','Ring3.R','Little3.R')]
anatomy_report = track_motion(rig, snapshot)
(out/'anatomy-qa.json').write_text(json.dumps(anatomy_report,indent=2)+'\n',encoding='utf-8')
locomotion_report = check_locomotion(rig, snapshot, legs, ground)
(out/'locomotion-qa.json').write_text(json.dumps(locomotion_report,indent=2)+'\n',encoding='utf-8')
attach_action('Hold', 0)
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_distance = 3.7
            area.spaces.active.region_3d.view_location = (0,0,1.0)
            from mathutils import Euler
            from math import radians
            area.spaces.active.region_3d.view_rotation = Euler((radians(77),0,radians(148)), 'XYZ').to_quaternion()
            area.spaces.active.shading.color_type = 'MATERIAL'
            area.spaces.active.overlay.show_overlays = False
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'ThrowObject.blend'))
# Export each action as a named GLB clip. No browser arm animation is authored separately.
bpy.ops.export_scene.gltf(
    filepath=str(out/'ThrowObject.glb'), export_format='GLB', export_animations=True,
    export_animation_mode='ACTIONS', export_frame_range=False,
    export_force_sampling=True, export_frame_step=1,
    export_optimize_animation_size=False, export_extras=False
)

manifest = {
    "fps": FPS,
    "clips": {
        "hold": {"name": "ThrowObject_Hold", "endFrame": HOLD_END, "loop": True},
        "throw": {"name": "ThrowObject_Throw", "endFrame": THROW_END, "loop": False},
        "pickup": {"name": ACTION, "label": "Pick Up", "endFrame": PICKUP_END, "loop": False, "next": "hold"},
        **{key: {"name": "ThrowObject_"+gait.action, "label": gait.title, "endFrame": gait.end, "loop": True,
                 "travelM": gait.travel, "speedMps": gait.speed, "stanceFraction": gait.stance}
           for key,gait in GAITS.items()},
    },
    "locomotion": {"groundY": ground, "mode": "in-place", "cycleStart": "right-foot contact"},
    "pickup": {"frame": GRAB_FRAME, "seconds": GRAB_FRAME / FPS, "event": GRAB_EVENT, "groundY": ground},
    "release": {"frame": RELEASE, "seconds": RELEASE / FPS, "event": RELEASE_EVENT},
    "attachment": {"previewNode": SOCKET, "gameBone": "NPC R Hand [RHnd]", "socketOffsetM": SOCKET_OFFSET},
}
(out/'timing.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(out/'source-qa.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8')

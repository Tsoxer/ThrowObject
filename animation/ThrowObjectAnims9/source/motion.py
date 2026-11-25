"""Original 30 FPS motion for Throw Object. Coordinates are right, forward, up."""
from dataclasses import dataclass
from math import cos, degrees, pi, radians, sin

import bpy
from mathutils import Matrix, Quaternion, Vector

from kinematics import joint_basis, knee_solution, leg_reference

FPS = 30
HOLD_END = 60
THROW_END = 28
RELEASE = 15
RELEASE_EVENT = "ThrowObjectRelease"
SOCKET = "ThrowObjectSocket"
SOCKET_OFFSET = (0.005, 0.093, 0.065)
DIGITS = ("Thumb", "Index", "Middle", "Ring", "Little")
UP = Vector((0, 0, 1))
DOWN = -UP
FORWARD = Vector((0, 1, 0))

UPPER_LENGTH = .295
FOREARM_LENGTH = .256
ELBOW_LIMITS = (8, 135)
FOREARM_ROLL_LIMIT = 72
WRIST_FLEX_LIMIT = 32
WRIST_DEVIATION_LIMIT = 14
SHOULDER_ELEVATION_LIMIT = 123
SHOULDER_AXIAL_LIMITS = (-65, 173)
X, Y, Z = (Vector(axis) for axis in ((1,0,0), (0,1,0), (0,0,1)))

# Preserve the original carry and idle landmarks, hand orientation and socket.
HOLD = ((.25, .19, -.95), (-.10, .80, -.59), (-.08, .94, -.34), (-.66, .30, .69))
IDLE = ((.18, .02, -.98), (.01, .14, -.99), (-.01, .22, -.97), (-.97, .13, .01))


@dataclass
class Arm:
    upper: Quaternion
    forearm: Quaternion
    elbow: Vector
    wrist: Vector
    flexion: float


def axes(direction, palm):
    """Build a right-handed orientation. Local +Y points down the segment."""
    y = Vector(direction).normalized()
    x = y.cross(Vector(palm)).normalized()
    return joint_basis(x, y)


def limit(value, minimum, maximum):
    return min(maximum, max(minimum, value))


def neutral_upper(upper, lower):
    return joint_basis(upper.cross(lower).normalized(), upper)


def arm_from_rotation(upper, flexion):
    forearm = upper @ Quaternion(X, flexion)
    elbow = upper @ (Y * UPPER_LENGTH)
    wrist = elbow + forearm @ (Y * FOREARM_LENGTH)
    return Arm(upper, forearm, elbow, wrist, flexion)


def solve_arm(direction, axial, flexion):
    """Swing and rotate the humerus, then bend about its anatomical X hinge."""
    direction = direction.normalized()
    if direction.angle(DOWN) > radians(SHOULDER_ELEVATION_LIMIT) + 1e-6:
        raise ValueError('Shoulder direction exceeds the authored elevation limit')
    neutral = neutral_upper(*[Vector(v).normalized() for v in IDLE[:2]])
    swing = (neutral @ Y).rotation_difference(direction)
    axial = radians(limit(axial, *SHOULDER_AXIAL_LIMITS))
    flexion = radians(limit(flexion, *ELBOW_LIMITS))
    upper = swing @ neutral @ Quaternion(Y, axial)
    return arm_from_rotation(upper, flexion)


def wrist_rotation(flexion, deviation):
    """Limit wrist swing in flexion/deviation and leave axial roll at the forearm."""
    flexion = limit(flexion, -WRIST_FLEX_LIMIT, WRIST_FLEX_LIMIT)
    deviation = limit(deviation, -WRIST_DEVIATION_LIMIT, WRIST_DEVIATION_LIMIT)
    swing = X*radians(flexion) + Z*radians(deviation)
    return Quaternion(swing.normalized(), swing.length) if swing.length > 1e-8 else Quaternion()


def matching_rest(landmarks, breath=0.0):
    """Match the prior carry/idle palm using forearm roll plus wrist swing."""
    upper, lower, wrist, palm = landmarks
    upper = Vector(upper).normalized()
    lower = (Vector(lower) + UP * (.024 * breath)).normalized()
    arm = arm_from_rotation(neutral_upper(upper, lower), upper.angle(lower))
    desired = axes(wrist, palm)
    local = arm.forearm.inverted() @ desired
    _, roll = local.to_swing_twist('Y')
    roll = limit(roll, -radians(FOREARM_ROLL_LIMIT), radians(FOREARM_ROLL_LIMIT))
    swing = Quaternion(Y, -roll) @ local
    return arm, roll, swing


def throw_key(frame, yaw, pitch, lift, direction, axial, elbow, roll, wrist_flex, deviation, grip):
    return frame, yaw, pitch, lift, Vector(direction).normalized(), axial, elbow, roll, wrist_flex, deviation, grip


def end_key(frame, landmarks, grip):
    arm, roll, swing = matching_rest(landmarks)
    neutral, _, _ = matching_rest(IDLE)
    transport = (neutral.upper @ Y).rotation_difference(arm.upper @ Y) @ neutral.upper
    _, axial = (transport.inverted() @ arm.upper).to_swing_twist('Y')
    direction, angle = swing.to_axis_angle()
    rotation_vector = direction * degrees(angle)
    return throw_key(frame, 0, 0, 0, arm.upper @ Y, degrees(axial), degrees(arm.flexion),
                     degrees(roll), rotation_vector.x, rotation_vector.z, grip)


# Torso, shoulder swing direction and axial rotation, elbow flexion, forearm
# pronation/supination, wrist swing, and grip. Rotation is authored per joint.
THROW = (
    end_key(0, HOLD, 1),
    throw_key(3, -3, 1, 2, (.52,.06,-.86), 42, 55, -22, 8, -8, 1),
    throw_key(6, -9, 2, 5, (.88,-.14,-.45), 92, 90, -39, -8, -5, 1),
    throw_key(8, -14, 3, 7, (.96,-.26,.10), 115, 87, -47, -15, -4, 1),
    throw_key(10,-17, 4, 8, (.91,-.38,.15), 148, 94, -49, -19, -4, 1),
    throw_key(11,-16, 4, 8, (.90,-.43,.15), 169, 96, -45, -18, -4, 1),
    throw_key(12,-10, 3, 8, (.94,-.25,.24), 162, 93, -38, -15, -3, 1),
    throw_key(13, -3, 1, 8, (.86,.19,.47), 123, 80, -25, -11, -1, 1),
    throw_key(14,  4, 0, 7, (.73,.46,.52), 80, 60, -23, -6, 2, .96),
    throw_key(15, 10,-2, 6, (.43,.76,.49), 38, 30, -35, 12, 4, .03),
    throw_key(16, 15,-4, 4, (.19,.95,.24), -3, 27, -48, 23, 5, 0),
    throw_key(18, 17,-6, 2, (-.03,.81,-.58), -35, 34, -55, 16, 5, .04),
    throw_key(20, 13,-5, 1, (-.08,.57,-.82), -37, 33, -51, 11, 2, .12),
    throw_key(22,  8,-3, 0, (.04,.35,-.94), -30, 22, -43, 7, -1, .20),
    throw_key(25,  3,-1, 0, (.15,.135,-.98), -12, 13, -33, 4, -3, .27),
    end_key(28, IDLE, .30),
)


def sample(frame, field, keys=THROW):
    for i, (left, right) in enumerate(zip(keys, keys[1:])):
        if frame <= right[0]:
            t = (frame - left[0]) / (right[0] - left[0])
            a = left[field]
            b = right[field]
            before = keys[max(0, i - 1)]
            after = keys[min(len(keys) - 1, i + 2)]
            scale = right[0] - left[0]
            da = (b - before[field]) * (scale / (right[0] - before[0])) if i else a * 0
            db = (after[field] - a) * (scale / (after[0] - left[0])) if i < len(keys) - 2 else b * 0
            return (2*t**3 - 3*t*t + 1)*a + (t**3 - 2*t*t + t)*da + (-2*t**3 + 3*t*t)*b + (t**3 - t*t)*db
    return keys[-1][field]


def solve_frame(label, frame):
    if label == 'Hold':
        breath = sin(pi * frame / HOLD_END)**2
        arm, roll, swing = matching_rest(HOLD, breath)
        return 0, 0, .43 * breath, arm, roll, swing, 1
    if frame == 0 or frame == THROW_END:
        arm, roll, swing = matching_rest(HOLD if frame == 0 else IDLE)
        return 0, 0, 0, arm, roll, swing, 1 if frame == 0 else .30
    yaw, pitch, lift, direction, axial, elbow, roll, flexion, deviation, grip = (
        sample(frame, field) for field in range(1, 11)
    )
    roll = radians(limit(roll, -FOREARM_ROLL_LIMIT, FOREARM_ROLL_LIMIT))
    return yaw, pitch, lift, solve_arm(direction, axial, elbow), roll, wrist_rotation(flexion, deviation), limit(grip, 0, 1)


def stance_key(frame, displacement, yaw, roll, free_reach, free_elbow):
    return frame, Vector(displacement), yaw, roll, free_reach, free_elbow


# The throw shifts from the loaded right leg toward the left as the hips initiate.
# Both ankle targets stay planted and the pelvis returns to reference at the end.
STANCE = (
    stance_key(0,  (.017,-.002,-.026), -1.4, .7, .04, 10),
    stance_key(3,  (.024,-.009,-.036), -3, 1.2, .16, 15),
    stance_key(6,  (.041,-.025,-.047), -6, 1.7, .23, 22),
    stance_key(9,  (.055,-.039,-.050), -9, 2.3, .27, 28),
    stance_key(11, (.049,-.021,-.053), -7, 2, .27, 31),
    stance_key(12, (.033,-.010,-.046), -2, 1, .22, 34),
    stance_key(14, (.001,.017,-.027),  7, -.6, .09, 38),
    stance_key(16, (-.044,.043,-.045), 12, -2, -.20, 28),
    stance_key(18, (-.055,.047,-.051), 11, -2.1, -.26, 14),
    stance_key(21, (-.036,.029,-.037), 7, -1.5, -.16, 9),
    stance_key(24, (-.013,.010,-.018), 3, -.6, -.04, 8),
    stance_key(26, (-.003,.003,-.006), .8, -.12, .02, 7),
    stance_key(28, (0,0,0), 0, 0, .035, 6),
)


def swing_left_arm(rig, chest, direction, flexion, wrist=0, roll=0):
    bones = rig.pose.bones
    upper = direction.normalized()
    hinge = upper.cross(FORWARD).normalized()
    fore = Quaternion(hinge,radians(flexion)) @ upper
    set_global_rotation(bones['UpperArm.L'], chest @ joint_basis(hinge,upper))
    fore_rotation = joint_basis(hinge,fore) @ Quaternion(Y,radians(roll))
    set_global_rotation(bones['Forearm.L'], chest @ fore_rotation)
    hand = Quaternion(hinge,radians(wrist)) @ fore
    hand_rotation = axes(hand,(1,.10,.03)) @ Quaternion(Y,radians(roll*.55))
    set_global_rotation(bones['Hand.L'], chest @ hand_rotation)


def standing_pose(rig, legs, label, frame, yaw, pitch, lift):
    bones = rig.pose.bones
    pelvis = bones['Pelvis']
    if label == 'Hold':
        phase = 2*pi*frame/HOLD_END
        breath = .5*(1-cos(phase))
        weight = sin(phase)*(.75+.25*cos(phase))
        displacement = STANCE[0][1] + Vector((.009*weight,.004*breath,.0035*breath))
        hip_yaw = -1.4 + .55*weight
        hip_roll = .7 + .37*weight
        free_reach = .04+.014*(sin(phase-.3)+sin(.3))
        free_elbow = 10+.65*breath
        free_wrist = .85*(sin(phase-.25)+sin(.25))
        torso_pitch = pitch + .45*breath
    else:
        displacement, hip_yaw, hip_roll, free_reach, free_elbow = (
            sample(frame,index,STANCE) for index in range(1,6)
        )
        free_wrist = sample(frame,1,((0,0),(11,-3),(17,5),(24,1),(28,0)))
        torso_pitch = pitch
    pelvis.location = pelvis.bone.matrix_local.to_quaternion().inverted() @ displacement
    rotate_in_world(pelvis, Quaternion(UP,radians(hip_yaw)) @ Quaternion(Y,radians(hip_roll)))
    # The pelvis leads; distribute the rest of the turn over the middle and upper back.
    relative_yaw = yaw-.64*hip_yaw
    spine = Quaternion(UP,radians(relative_yaw*.37)) @ Quaternion(X,radians(torso_pitch*.39))
    chest = Quaternion(UP,radians(relative_yaw*.63)) @ Quaternion(X,radians(torso_pitch*.61))
    rotate_in_world(bones['Spine'],spine)
    rotate_in_world(bones['Chest'],chest)
    rotate_in_world(bones['Clavicle.R'],Quaternion(Y,radians(-lift)))
    rotate_in_world(bones['Clavicle.L'],Quaternion(Y,radians(3*free_reach if frame!=THROW_END or label=='Hold' else 0)))
    bpy.context.view_layer.update()
    chest_pose = bones['Chest'].matrix.to_quaternion() @ bones['Chest'].bone.matrix_local.to_quaternion().inverted()
    head = bones['Head']
    head_pose = Quaternion(UP,radians(yaw*.21)) @ Quaternion(X,radians(torso_pitch*.20))
    set_global_rotation(head, head_pose @ head.bone.matrix_local.to_quaternion())
    if label == 'Throw' and frame == THROW_END:
        for leg in legs:
            for name in (leg.thigh,leg.shin,leg.foot):
                bones[name].rotation_quaternion = Quaternion()
        for name in ('UpperArm.L','Forearm.L','Hand.L'):
            bones[name].rotation_quaternion = Quaternion()
    else:
        for leg in legs:
            hip = bones[leg.thigh].head.copy()
            try:
                thigh, shin = knee_solution(hip,leg.rest_ankle,leg,minimum=1,pole=leg.stance_pole)
            except ValueError as error:
                raise ValueError(f'{label} frame {frame}: {error}') from error
            set_global_rotation(bones[leg.thigh],thigh)
            set_global_rotation(bones[leg.shin],shin)
            set_global_rotation(bones[leg.foot],leg.rest_rotation)
        # Carry the idle free-arm offset into Throw, with zero blend velocity
        # at the start and at frame 6, where it rejoins the original wind-up.
        progress = 0.0 if label == 'Hold' else min(frame/6,1.0)
        carry = (1-progress)**3 * (1+3*progress+6*progress**2)
        free_reach -= .18*carry
        free_elbow += 14*carry
        swing_left_arm(rig, chest_pose, Vector((-.18,free_reach,-.98)), free_elbow, free_wrist)
    return chest_pose


def setup_rig():
    data = bpy.data.armatures.new("Throw Object authoring skeleton")
    rig = bpy.data.objects.new("Throw Object", data)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    rig.show_in_front = True
    data.display_type = 'STICK'
    bpy.ops.object.mode_set(mode='EDIT')

    def bone(name, head, tail, parent=None, normal=(0, -1, 0)):
        item = data.edit_bones.new(name)
        item.head, item.tail = head, tail
        item.align_roll(Vector(normal))
        if parent:
            item.parent = data.edit_bones[parent]
        return item

    bone('Root', (0, 0, 0), (0, .2, 0), normal=UP)
    bone('Pelvis', (0, 0, .91), (0, 0, 1.015), 'Root')
    bone('Spine', (0, 0, 1.015), (0, 0, 1.23), 'Pelvis')
    bone('Chest', (0, 0, 1.23), (0, 0, 1.445), 'Spine')
    bone('Neck', (0, 0, 1.445), (0, .009, 1.57), 'Chest')
    bone('Head', (0, .009, 1.57), (0, .015, 1.77), 'Neck')
    for s, sign in (('R', 1), ('L', -1)):
        tag = f'.{s}'
        shoulder = Vector((sign * .208, 0, 1.441))
        hand_parent = 'Forearm'+tag
        if s == 'R':
            neutral, roll, swing = matching_rest(IDLE)
            elbow = shoulder + neutral.elbow
            wrist = shoulder + neutral.wrist
            upper_rot = neutral.upper
            forearm_rot = neutral.forearm
            roll_rot = forearm_rot @ Quaternion(Y, roll)
            hand_rot = roll_rot @ swing
        else:
            upper, lower, hand = map(Vector, ((-.18, .035, -.98), (-.01, .16, -.99), (.035, .22, -.97)))
            elbow = shoulder + upper.normalized() * UPPER_LENGTH
            wrist = elbow + lower.normalized() * FOREARM_LENGTH
            upper_rot, forearm_rot, hand_rot = axes(upper, FORWARD), axes(lower, FORWARD), axes(hand, (1, .13, .01))
        bone('Clavicle'+tag, (sign*.045, .015, 1.432), shoulder, 'Chest', UP)
        bone('UpperArm'+tag, shoulder, elbow, 'Clavicle'+tag, upper_rot @ Z)
        bone('Forearm'+tag, elbow, wrist, 'UpperArm'+tag, forearm_rot @ Z)
        if s == 'R':
            hand_parent = 'ForearmRoll.R'
            bone(hand_parent, elbow, wrist, 'Forearm.R', roll_rot @ Z)
        bone('Hand'+tag, wrist, wrist + (hand_rot @ Y)*.10, hand_parent, hand_rot @ Z)
        for j, label in enumerate(DIGITS):
            side = sign
            if label == 'Thumb':
                p = Vector((side*.037, .029, .007))
                vectors = [(side*.67, .51, .21), (side*.42, .64, .23), (side*.21, .76, .16)]
                lengths = (.032, .025, .020)
            else:
                p = Vector((side*(.033-(j-1)*.0215), .093 - .004*abs(j-2), 0))
                v = side * (.055-(j-1)*.047)
                vectors = [(v, 1, -.04)] * 3
                lengths = ((.039,.025,.020), (.044,.029,.022), (.040,.025,.020), (.032,.020,.016))[j-1]
            for k, (v, length) in enumerate(zip(vectors, lengths)):
                tip = p + Vector(v).normalized() * length
                parent = 'Hand'+tag if k == 0 else f'{label}{k}'+tag
                bone(f'{label}{k+1}'+tag, wrist + hand_rot@p, wrist + hand_rot@tip, parent, hand_rot@UP)
                p = tip

        hip = Vector((sign*.105, 0, .935))
        knee = Vector((sign*.137, .026, .503))
        ankle = Vector((sign*.151, -.003, .097))
        toe = Vector((sign*.16, .147, .063))
        bone('Thigh'+tag, hip, knee, 'Pelvis')
        bone('Shin'+tag, knee, ankle, 'Thigh'+tag)
        bone('Foot'+tag, ankle, toe, 'Shin'+tag, UP)

    bpy.ops.object.mode_set(mode='OBJECT')
    for item in rig.pose.bones:
        item.rotation_mode = 'QUATERNION'
    pelvis = rig.pose.bones['Pelvis']
    pelvis['description'] = 'Weight transfer over a fixed actor root.'
    rig.pose.bones['Forearm.R']['description'] = 'Elbow hinge. Keys rotate around local X only.'
    rig.pose.bones['ForearmRoll.R']['description'] = 'Pronation/supination. Keys rotate around local Y only.'
    rig.pose.bones['Hand.R']['description'] = 'Wrist flexion and deviation. The forearm carries axial roll.'
    return rig


def set_global_rotation(bone, desired):
    parent = bone.parent
    if parent:
        rest_relative = parent.bone.matrix_local.to_quaternion().inverted() @ bone.bone.matrix_local.to_quaternion()
        basis = parent.matrix.to_quaternion() @ rest_relative
    else:
        basis = bone.bone.matrix_local.to_quaternion()
    rotation = basis.inverted() @ desired
    if rotation.dot(bone.rotation_quaternion) < 0:
        rotation.negate()
    bone.rotation_quaternion = rotation
    bpy.context.view_layer.update()


def rotate_in_world(bone, rotation):
    local = bone.bone.matrix_local.to_quaternion()
    bone.rotation_quaternion = local.inverted() @ rotation @ local


def grip_fingers(rig, grip, side='R'):
    for j, digit in enumerate(DIGITS):
        for k in range(3):
            item = rig.pose.bones[f'{digit}{k+1}.{side}']
            if j == 0:
                curl = ((8, 38), (2, 29), (1, 20))[k]
                twist = ((-9, 33), (0, 7), (0, 5))[k]
            else:
                curl = ((3, 23), (5, 42), (2, 40))[k]
                twist = (0, 0)
            angle = curl[0] + (curl[1] - curl[0]) * grip
            spread = twist[0] + (twist[1] - twist[0]) * grip
            item.rotation_quaternion = Quaternion((0,0,1), radians(spread)) @ Quaternion((1,0,0), radians(angle))


def action_curves(action, slot):
    if slot is not None and action.is_action_layered:
        return action.layers[0].strips[0].channelbag(slot).fcurves
    return action.fcurves


def generate_actions(rig):
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1
    animation = rig.animation_data_create()
    actions = {}
    neutral, neutral_roll, neutral_swing = matching_rest(IDLE)
    legs = [leg_reference(rig,side) for side in ('R','L')]
    right_chain = ('UpperArm.R', 'Forearm.R', 'ForearmRoll.R', 'Hand.R')
    fingers = tuple(f'{digit}{k+1}.R' for digit in DIGITS for k in range(3))
    for label, end in (('Hold', HOLD_END), ('Throw', THROW_END)):
        for b in rig.pose.bones:
            b.matrix_basis = Matrix.Identity(4)
        grip_fingers(rig, .30, 'L')
        action = bpy.data.actions.new('ThrowObject_'+label)
        action.use_fake_user = True
        animation.action = action
        bones = rig.pose.bones
        animated = right_chain + fingers + ('Pelvis','Clavicle.R','Clavicle.L','Spine','Chest','Head',
                                              'UpperArm.L','Forearm.L','Hand.L') + tuple(
            name for leg in legs for name in (leg.thigh,leg.shin,leg.foot)
        )
        # Both actions share the same relaxed hand at their common start pose.
        animated += tuple(f'{digit}{joint}.L' for digit in DIGITS for joint in range(1,4))
        for frame in range(end + 1):
            scene.frame_set(frame)
            yaw, pitch, lift, arm, roll, swing, grip = solve_frame(label, frame)
            chest_pose = standing_pose(rig,legs,label,frame,yaw,pitch,lift)
            set_global_rotation(bones['UpperArm.R'], chest_pose @ arm.upper)
            bones['Forearm.R'].rotation_quaternion = Quaternion(X, arm.flexion - neutral.flexion)
            bones['ForearmRoll.R'].rotation_quaternion = Quaternion(Y, roll - neutral_roll)
            bones['Hand.R'].rotation_quaternion = neutral_swing.inverted() @ swing
            grip_fingers(rig, grip)
            free_grip = .30 + .025*sin(pi*frame/HOLD_END)**2 if label == 'Hold' else (
                .30 - .12*sin(pi*frame/THROW_END)**2
            )
            grip_fingers(rig,free_grip,'L')
            for name in animated:
                bones[name].keyframe_insert(data_path='rotation_quaternion', frame=frame, group=name)
            bones['Pelvis'].keyframe_insert(data_path='location',frame=frame,group='Pelvis')
        for curve in action_curves(action, getattr(animation, 'action_slot', None)):
            for key in curve.keyframe_points:
                key.interpolation = 'LINEAR'
        if label == 'Throw':
            marker = action.pose_markers.new(RELEASE_EVENT)
            marker.frame = RELEASE
        action['fps'] = FPS
        action['loop'] = label == 'Hold'
        actions[label] = (action, getattr(animation, 'action_slot', None))
    animation.action = None
    return actions

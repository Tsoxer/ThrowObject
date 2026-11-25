"""In-place forward carry locomotion with contact-controlled two-bone legs."""
from dataclasses import dataclass
from math import cos, degrees, pi, radians, sin

import bpy
from mathutils import Matrix, Quaternion, Vector

from kinematics import knee_solution, leg_reference
from motion import DIGITS, FPS, HOLD, IDLE, UP, X, Y
from motion import action_curves, grip_fingers, limit, matching_rest, swing_left_arm
from motion import rotate_in_world, set_global_rotation, solve_arm, wrist_rotation
from running import RUNS, running_arms, running_body, running_foot


@dataclass(frozen=True)
class Gait:
    action: str
    title: str
    end: int
    stance: float
    travel: float
    plant_y: float
    toe_lift: float
    hip_drop: float
    hip_bob: float
    load_phase: float
    lean: float
    hip_yaw: float
    free_swing: float
    free_elbow: float
    carry_elbow: float
    strike_pitch: float
    flat_pitch: float
    push_pitch: float
    recover_pitch: float
    stance_width: float
    sway: float
    pitch_wave: float

    @property
    def speed(self):
        return self.travel * FPS / self.end


GAITS = {
    'walk': Gait('CarryWalk', 'Carry Walk', 36, .62, 1.32, .33, .064,
                 .055, .025, .035, 3, 5, .34, 24, 47, 15, 0, -32, -40, .132, .033, 1.0),
    'jog': Gait('CarryJog', 'Carry Jog', 24, .41, 2.12, .33, .221,
                .079, .036, .10, 9, 7, .58, 67, 64, -7, -1, -39, -51, .108, .019, 1.9),
    'sprint': Gait('CarrySprint', 'Carry Sprint', 18, .32, 2.66, .31, .329,
                   .073, .042, .10, 19, 9, .78, 77, 79, -14, -5, -45, -56, .095, .015, 2.7),
}


def smooth(t):
    return t*t*(3-2*t)


def foot_pitch(phase, poses):
    """Interpolate through moving pitch keys without stopping at each key."""
    for i, ((left,value),(right,next_value)) in enumerate(zip(poses,poses[1:])):
        if phase <= right:
            duration = right-left
            t = (phase-left)/duration
            previous = poses[max(i-1,0)]
            following = poses[min(i+2,len(poses)-1)]
            enter = (next_value-previous[1])*duration/(right-previous[0]) if i else 0
            leave = (following[1]-value)*duration/(following[0]-left) if i+2<len(poses) else 0
            if value == next_value:
                return value
            if i and previous[1] == value:
                enter = 0
            if following[1] == next_value:
                leave = 0
            return (2*t**3-3*t*t+1)*value + (t**3-2*t*t+t)*enter + (-2*t**3+3*t*t)*next_value + (t**3-t*t)*leave
    return poses[-1][1]


def gait_leg(rig, model, side):
    leg = leg_reference(rig,side)
    group_index = model.vertex_groups[leg.foot].index
    leg.shoe = tuple(vertex.co-leg.rest_ankle for vertex in model.data.vertices
                     if any(group.group == group_index and group.weight > .99 for group in vertex.groups))
    return leg


def foot_target(gait, leg, phase, ground):
    if gait.action in RUNS:
        return running_foot(gait,leg,phase,ground)
    contact = phase < gait.stance
    if contact:
        progress = phase/gait.stance
        pitch = foot_pitch(progress, ((0,gait.strike_pitch),(.24,gait.flat_pitch),
                                     (.60,gait.flat_pitch),(1,gait.push_pitch)))
        roll = smooth(limit((progress-.12)/.82,0,1))
        travel = 0
        clearance = 0
        lateral = 0
        toe_out = -leg.side * (6 if gait.lean < 5 else 3)
    else:
        progress = (phase-gait.stance)/(1-gait.stance)
        pitch = foot_pitch(progress, ((0,gait.push_pitch),(.23,gait.recover_pitch),
                                     (.65,-11),(.86,6 if gait.lean < 5 else -1),(1,gait.strike_pitch)))
        roll = 1-smooth(progress)
        # Match trailing stance speed at lift-off and the next plant.
        travel = gait.travel * smooth(progress)
        crest = max(0, sin(pi*progress))
        clearance = gait.toe_lift * crest**1.20 * (1+.16*sin(2*pi*progress+.25))
        lateral = .016 * crest**2
        toe_out = -leg.side * ((6 if gait.lean < 5 else 3)-5*crest**2)
    correction = Quaternion(UP,radians(toe_out)) @ Quaternion(X,radians(pitch))
    # Roll over the heel and forefoot while the actor travels over the planted shoe.
    pivot = Vector((0, -.057+.216*roll, -.060))
    y = gait.plant_y - gait.travel*phase + travel + pivot.y - (correction@pivot).y
    ankle_z = ground + clearance - min((correction@point).z for point in leg.shoe)
    return Vector((leg.side*(gait.stance_width+lateral), y, ankle_z)), correction@leg.rest_rotation


def body_motion(rig, gait, phase):
    bones = rig.pose.bones
    stride = 2*pi*phase
    step = 2*stride
    hips = bones['Pelvis']
    bob = -gait.hip_drop - gait.hip_bob*cos(step-4*pi*gait.load_phase)
    lateral = gait.sway*sin(stride-.40) + .0025*sin(2*stride+.45)
    fore = .010*sin(step-.80) + (.012 if gait.lean > 10 else 0)
    shift = Vector((lateral,fore,bob))
    hips.location = hips.bone.matrix_local.to_quaternion().inverted() @ shift
    pelvis_yaw = gait.hip_yaw*cos(stride+.17)
    pelvis_roll = 2.6*sin(stride-.33)
    wave = gait.pitch_wave*sin(step-.72)
    rotate_in_world(hips, Quaternion(UP,radians(pelvis_yaw)) @
                    Quaternion(X,radians(-gait.lean*.24-.25*wave)) @ Quaternion(Y,radians(pelvis_roll)))
    rotate_in_world(bones['Spine'], Quaternion(UP,radians(-gait.hip_yaw*.76*cos(stride-.07))) @
                    Quaternion(X,radians(-gait.lean*.39-.34*wave)) @ Quaternion(Y,radians(-.52*pelvis_roll)))
    rotate_in_world(bones['Chest'], Quaternion(UP,radians(-gait.hip_yaw*.78*cos(stride-.29))) @
                    Quaternion(X,radians(-gait.lean*.37-.41*wave)) @ Quaternion(Y,radians(-.70*pelvis_roll)))
    rotate_in_world(bones['Clavicle.R'], Quaternion(Y,radians(.4-1.0*sin(stride+.20))))
    rotate_in_world(bones['Clavicle.L'], Quaternion(Y,radians(2.1*cos(stride-.12)+.5*sin(step))))
    bpy.context.view_layer.update()
    head = bones['Head']
    head_pitch = -gait.lean*.24 + .65*sin(step-.40)
    upper_heading = -.85*cos(stride-.22)
    head_motion = Quaternion(UP,radians(upper_heading)) @ Quaternion(X,radians(head_pitch))
    set_global_rotation(head, head_motion @ head.bone.matrix_local.to_quaternion())


def carry_arms(rig, gait, phase, baseline):
    bones = rig.pose.bones
    stride = 2*pi*phase
    free_swing = cos(stride-.20) + .13*cos(2*stride+.18)
    carry_swing = -cos(stride+.32) + .17*sin(2*stride-.2)
    chest = bones['Chest'].matrix.to_quaternion() @ bones['Chest'].bone.matrix_local.to_quaternion().inverted()
    neutral, neutral_roll, neutral_wrist = baseline
    _, carry_roll, carry_wrist = matching_rest(HOLD)
    right_direction = Vector((.245-.004*(gait.lean-3), .18+.10*carry_swing, -.96))
    arm = solve_arm(right_direction, 34+2.4*sin(stride+.15), gait.carry_elbow+3.4*sin(stride+.48))
    wrist_compensation = 1.8*sin(2*stride-.72)
    set_global_rotation(bones['UpperArm.R'], chest@arm.upper)
    bones['Forearm.R'].rotation_quaternion = Quaternion(X,arm.flexion-neutral.flexion)
    bones['ForearmRoll.R'].rotation_quaternion = Quaternion(Y,carry_roll-neutral_roll)
    wrist_axis,wrist_angle = carry_wrist.to_axis_angle()
    wrist_vector = wrist_axis*degrees(wrist_angle)
    wrist_swing = wrist_rotation(wrist_vector.x+wrist_compensation, wrist_vector.z)
    bones['Hand.R'].rotation_quaternion = neutral_wrist.inverted()@wrist_swing

    free_direction = Vector((-.15-.03*sin(stride-.06), gait.free_swing*free_swing-.035, -1))
    elbow = gait.free_elbow + (11 if gait.lean < 5 else 15)*cos(stride-.67)
    swing_left_arm(rig,chest,free_direction,elbow,
                   wrist=4.5*sin(stride-.83), roll=7*sin(stride-.55))
    grip_fingers(rig, 1, 'R')
    grip_fingers(rig, .22+.07*sin(stride-.16), 'L')


def generate_carry_actions(rig, model):
    scene = bpy.context.scene
    animation = rig.animation_data
    legs = [gait_leg(rig,model,side) for side in ('R','L')]
    ground = min(leg.rest_ankle.z+point.z for leg in legs for point in leg.shoe)
    baseline = matching_rest(IDLE)
    animated = ('Pelvis','Spine','Chest','Head','Clavicle.R','Clavicle.L',
                'UpperArm.R','Forearm.R','ForearmRoll.R','Hand.R',
                'UpperArm.L','Forearm.L','Hand.L') + tuple(
        bone for leg in legs for bone in (leg.thigh,leg.shin,leg.foot)
    ) + tuple(f'{digit}{joint}.{side}' for digit in DIGITS for joint in range(1,4) for side in ('R','L'))
    actions = {}
    for gait in GAITS.values():
        body, arms = (running_body,running_arms) if gait.action in RUNS else (body_motion,carry_arms)
        for bone in rig.pose.bones:
            bone.matrix_basis = Matrix.Identity(4)
        action = bpy.data.actions.new('ThrowObject_'+gait.action)
        action.use_fake_user = True
        action['fps'] = FPS
        action['loop'] = True
        action['pose_space'] = 'In place. Use motion outside the animation clip.'
        action['reference_speed_m_s'] = gait.speed
        animation.action = action
        for frame in range(gait.end+1):
            phase = frame/gait.end if frame < gait.end else 0
            scene.frame_set(frame)
            body(rig,gait,phase)
            for leg, offset in zip(legs, (0,.5)):
                foot, rotation = foot_target(gait,leg,(phase+offset)%1,ground)
                hip = rig.pose.bones[leg.thigh].head.copy()
                try:
                    thigh, shin = knee_solution(hip,foot,leg)
                except ValueError as error:
                    raise ValueError(f'{gait.title} frame {frame}: {error}') from error
                set_global_rotation(rig.pose.bones[leg.thigh],thigh)
                set_global_rotation(rig.pose.bones[leg.shin],shin)
                set_global_rotation(rig.pose.bones[leg.foot],rotation)
            arms(rig,gait,phase,baseline)
            for bone in animated:
                rig.pose.bones[bone].keyframe_insert(data_path='rotation_quaternion',frame=frame,group=bone)
            rig.pose.bones['Pelvis'].keyframe_insert(data_path='location',frame=frame,group='Pelvis')
        slot = getattr(animation,'action_slot',None)
        for curve in action_curves(action,slot):
            for key in curve.keyframe_points:
                key.interpolation = 'LINEAR'
        actions[gait.action] = (action,slot)
    animation.action = None
    # Keep unkeyed channels in the same neutral pose as the two original actions.
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    grip_fingers(rig,.30,'L')
    return actions,legs,ground

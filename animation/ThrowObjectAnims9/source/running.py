"""Contact, recovery and whole-body timing for the jog and sprint cycles."""
from dataclasses import dataclass
from math import cos, degrees, pi, radians, sin

import bpy
from mathutils import Quaternion, Vector

from motion import HOLD, UP, X, Y, grip_fingers, matching_rest, rotate_in_world
from motion import sample, set_global_rotation, solve_arm, swing_left_arm, wrist_rotation


@dataclass(frozen=True)
class Run:
    stance_pitch: tuple
    swing_pitch: tuple
    swing_advance: tuple
    swing_height: tuple
    pelvis_height: tuple
    ball_start: float
    toe_out: float
    ankle_arc: float
    carry_swing: float
    arm_forward: float
    arm_back: float


RUNS = {
    'CarryJog': Run(
        stance_pitch=((0,-6),(.18,-2),(.43,-1),(.72,-13),(1,-41)),
        swing_pitch=((0,-41),(.18,-46),(.40,-29),(.69,-7),(.85,1),(1,-6)),
        swing_advance=((0,0),(.17,.043),(.38,.296),(.62,.698),(.83,.910),(1,1)),
        swing_height=((0,0),(.15,.096),(.33,.229),(.49,.193),(.68,.105),(.88,.030),(1,0)),
        pelvis_height=((-.10,-.066),(0,-.077),(.15,-.103),(.26,-.109),(.49,-.083),(.73,-.063),(.90,-.066),(1,-.077),(1.15,-.103)),
        ball_start=.056, toe_out=4, ankle_arc=.014,
        carry_swing=.074, arm_forward=.53, arm_back=.39,
    ),
    'CarrySprint': Run(
        stance_pitch=((0,-12),(.17,-5),(.39,-7),(.70,-24),(1,-49)),
        swing_pitch=((0,-49),(.15,-58),(.36,-40),(.62,-17),(.84,-3),(1,-12)),
        swing_advance=((0,0),(.15,.029),(.36,.273),(.59,.664),(.82,.898),(1,1)),
        swing_height=((0,0),(.13,.135),(.31,.338),(.46,.306),(.65,.178),(.84,.063),(1,0)),
        pelvis_height=((-.18,-.053),(0,-.069),(.12,-.096),(.21,-.111),(.41,-.087),(.64,-.058),(.82,-.053),(1,-.069),(1.12,-.096)),
        ball_start=.092, toe_out=2.5, ankle_arc=.012,
        carry_swing=.066, arm_forward=.81, arm_back=.66,
    ),
}


def curve(phase, keys):
    return sample(phase,1,keys)


def running_foot(gait, leg, phase, ground):
    run = RUNS[gait.action]
    toe_out = run.toe_out
    if phase < gait.stance:
        contact = phase/gait.stance
        pitch = curve(contact,run.stance_pitch)
        # Support goes from the outer mid/forefoot onto the ball at push-off.
        bend = contact*contact*(3-2*contact)
        pivot_y = run.ball_start + (.163-run.ball_start)*bend
        clearance = advance = lateral = 0
    else:
        swing = (phase-gait.stance)/(1-gait.stance)
        pitch = curve(swing,run.swing_pitch)
        crest = sin(pi*swing)
        clearance = max(0,curve(swing,run.swing_height))
        advance = gait.travel*curve(swing,run.swing_advance)
        pivot_y = .163-(.163-run.ball_start)*swing*swing*(3-2*swing)
        lateral = run.ankle_arc*crest**2
        toe_out -= 4*crest**2
    roll = Quaternion(UP,radians(-leg.side*toe_out)) @ Quaternion(X,radians(pitch))
    pivot = Vector((0,pivot_y,-.058))
    y = gait.plant_y-gait.travel*phase+advance+pivot.y-(roll@pivot).y
    ankle_height = ground+clearance-min((roll@point).z for point in leg.shoe)
    return Vector((leg.side*(gait.stance_width+lateral),y,ankle_height)),roll@leg.rest_rotation


def running_body(rig, gait, phase):
    run = RUNS[gait.action]
    bones = rig.pose.bones
    stride = 2*pi*phase
    step = (2*phase)%1
    hips = bones['Pelvis']
    height = curve(step,run.pelvis_height)
    # The body lowers over the landing leg and rises through push-off and flight.
    sway = gait.sway*.72*sin(stride-.30)+.002*sin(2*stride+.25)
    fore = .013 + .010*sin(2*stride-.48)
    hips.location = hips.bone.matrix_local.to_quaternion().inverted() @ Vector((sway,fore,height))
    pelvis_turn = gait.hip_yaw*.83*cos(stride+.24)
    pelvic_roll = 2.3*sin(stride-.15)
    drive = gait.pitch_wave*sin(2*stride-.40)
    tilt = gait.lean*.48+.24*drive
    rotate_in_world(hips,Quaternion(UP,radians(pelvis_turn)) @
                    Quaternion(X,radians(-tilt)) @ Quaternion(Y,radians(pelvic_roll)))
    rotate_in_world(bones['Spine'],Quaternion(UP,radians(-gait.hip_yaw*.62*cos(stride-.03))) @
                    Quaternion(X,radians(-gait.lean*.30-.35*drive)) @ Quaternion(Y,radians(-.46*pelvic_roll)))
    rotate_in_world(bones['Chest'],Quaternion(UP,radians(-gait.hip_yaw*.78*cos(stride-.30))) @
                    Quaternion(X,radians(-gait.lean*.22-.41*drive)) @ Quaternion(Y,radians(-.70*pelvic_roll)))
    rotate_in_world(bones['Clavicle.R'],Quaternion(Y,radians(.75-.82*sin(stride+.27))))
    rotate_in_world(bones['Clavicle.L'],Quaternion(Y,radians(1.60*cos(stride-.22)+.65*sin(2*stride-.53))))
    bpy.context.view_layer.update()
    head = bones['Head']
    nod = -.18*gait.lean + .59*sin(2*stride-.60)
    turn = -1.07*cos(stride-.29)
    set_global_rotation(head,Quaternion(UP,radians(turn)) @ Quaternion(X,radians(nod))
                        @ head.bone.matrix_local.to_quaternion())


def running_arms(rig,gait,phase,baseline):
    run = RUNS[gait.action]
    bones = rig.pose.bones
    stride = 2*pi*phase
    chest = bones['Chest'].matrix.to_quaternion() @ bones['Chest'].bone.matrix_local.to_quaternion().inverted()
    neutral,neutral_roll,neutral_wrist = baseline
    _,carry_roll,carry_wrist = matching_rest(HOLD)
    # The carry arm yields slightly after each impact while maintaining its grip.
    rest_reach = .19-.0018*gait.lean
    reach = rest_reach-run.carry_swing*cos(stride+.26)+.014*sin(2*stride-.91)
    tuck = .224-.003*gait.lean+.009*sin(stride-.23)
    elbow = gait.carry_elbow+4.5*cos(stride-.53)+1.7*sin(2*stride-1.05)
    arm = solve_arm(Vector((tuck,reach,-.97)),34+3*sin(stride+.18),elbow)
    set_global_rotation(bones['UpperArm.R'],chest@arm.upper)
    bones['Forearm.R'].rotation_quaternion = Quaternion(X, arm.flexion-neutral.flexion)
    bones['ForearmRoll.R'].rotation_quaternion = Quaternion(Y,carry_roll-neutral_roll+radians(1.5*sin(stride-.69)))
    axis,angle = carry_wrist.to_axis_angle()
    wrist = axis*degrees(angle)
    swing = wrist_rotation(wrist.x+2.3*sin(2*stride-.82),wrist.z)
    bones['Hand.R'].rotation_quaternion = neutral_wrist.inverted()@swing

    pump = cos(stride-.24)
    # Upper arm leads, the elbow opens behind the rib cage and flexes on recovery.
    midpoint = (run.arm_forward-run.arm_back)*.5
    range_of_motion = (run.arm_forward+run.arm_back)*.5
    free_reach = midpoint+range_of_motion*pump+.078*cos(2*stride-.44)
    direction = Vector((-.142-.028*sin(stride-.10),free_reach,-1))
    flexion = gait.free_elbow+18*cos(stride-.78)+5*sin(2*stride-.67)
    swing_left_arm(rig,chest,direction,flexion,
                   wrist=5.6*sin(stride-1.04),roll=8.5*sin(stride-.61))
    grip_fingers(rig,1,'R')
    grip_fingers(rig,.21+.075*sin(stride-.44),'L')

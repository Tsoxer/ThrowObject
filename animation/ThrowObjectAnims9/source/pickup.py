"""Crouch, close the right hand around a ground prop, and rise into Hold."""
from math import acos, copysign, cos, degrees, radians, sqrt

import bpy
from mathutils import Matrix, Quaternion, Vector

from kinematics import joint_basis, knee_solution
from motion import (
    ELBOW_LIMITS, FOREARM_LENGTH, FOREARM_ROLL_LIMIT, FPS, HOLD, IDLE, SOCKET_OFFSET,
    THROW_END, UPPER_LENGTH, UP, WRIST_DEVIATION_LIMIT, WRIST_FLEX_LIMIT, X, Y,
    action_curves, axes, grip_fingers, limit,
    matching_rest, rotate_in_world, sample, set_global_rotation, swing_left_arm,
    wrist_rotation,
)

PICKUP_END = 45
GRAB_FRAME = 22
GRAB_EVENT = 'ThrowObjectGrab'
ACTION = 'ThrowObject_PickUp'


def body_key(frame, xyz, angles):
    return frame, Vector(xyz), Vector(angles)


# Angles encode pelvis pitch, spine pitch, chest pitch and body yaw.
BODY = (
    body_key(0, (0,0,0), (0,0,0,0)),
    body_key(5, (.009,-.029,-.067), (-6,-4,-1,-1)),
    body_key(10,(.025,-.074,-.203), (-16,-10,-4,-2.6)),
    body_key(15,(.048,-.122,-.372), (-27,-15,-7,-4)),
    body_key(19,(.060,-.149,-.490), (-35,-19,-10,-3.7)),
    body_key(22,(.063,-.157,-.515), (-38,-20,-10,-3)),
    body_key(24,(.062,-.151,-.501), (-37,-20,-10,-2.4)),
    body_key(28,(.049,-.121,-.398), (-28,-17,-8,-1.5)),
    body_key(33,(.033,-.071,-.225), (-18,-10,-5,-1.3)),
    body_key(39,(.019,-.021,-.067), (-4,-3,-1,-1.4)),
    body_key(45,(.017,-.002,-.026), (0,0,0,-1.4)),
)


# Restored counterbalance arc from Anims7. The free hand moves forward
# with the torso as the knees bend.
LEFT_ARM_REACH = (
    (0, .035), (5, .10), (10, .18), (15, .25), (19, .31),
    (22, .32), (24, .31), (28, .25), (33, .14), (39, -.08), (45, -.14),
)


def reach_key(frame, socket, direction, palm, pole, grip):
    return frame, *(Vector(value) for value in (socket,direction,palm,pole)), grip


# Hand socket trajectory in rig space. The object remains on the ground until
# frame 22, when the closing fingers and palm meet it.
REACH = (
    reach_key(0, (.200,.075,.810), IDLE[2], IDLE[3], (.32,-.55,-.22), .30),
    reach_key(6, (.269,.152,.719), (.11,.25,-.96), (-.96,.24,-.07), (.49,-.55,-.10), .11),
    reach_key(11,(.328,.248,.526), (.10,.27,-.96), (-.93,.31,-.07), (.53,-.51,-.09), .04),
    reach_key(16,(.378,.377,.263), (.08,.21,-.97), (-.95,.28,-.06), (.54,-.47,-.09), .02),
    reach_key(19,(.386,.430,.133), (.06,.16,-.99), (-.94,.26,-.04), (.53,-.48,-.08), .07),
    reach_key(21,(.386,.451,.085), (.06,.15,-.99), (-.94,.25,-.04), (.52,-.50,-.08), .79),
    reach_key(22,(.386,.455,.072), (.06,.15,-.99), (-.94,.25,-.04), (.52,-.50,-.08), 1),
    reach_key(24,(.385,.453,.119), (.065,.16,-.99), (-.94,.25,-.035), (.52,-.51,-.12), 1),
    reach_key(28,(.363,.420,.322), (.045,.39,-.92), (-.94,.28,.18), (.42,-.46,-.36), 1),
    reach_key(33,(.302,.389,.584), (-.025,.75,-.66), (-.81,.32,.52), (.32,-.43,-.78), 1),
    reach_key(39,(.242,.365,.901), HOLD[2], HOLD[3], (.29,-.32,-.94), 1),
    reach_key(45,(.220,.365,1.007), HOLD[2], HOLD[3], (.24,-.26,-.95), 1),
)


def set_legs(rig, legs):
    bones = rig.pose.bones
    for leg in legs:
        thigh,shin = knee_solution(bones[leg.thigh].head,leg.rest_ankle,leg,
                                    minimum=1,pole=leg.stance_pole)
        set_global_rotation(bones[leg.thigh],thigh)
        set_global_rotation(bones[leg.shin],shin)
        set_global_rotation(bones[leg.foot],leg.rest_rotation)


def crouch(rig, frame):
    bones = rig.pose.bones
    displacement, angles = (sample(frame,i,BODY) for i in (1,2))
    pelvis_pitch, spine_pitch, chest_pitch, yaw = angles
    pelvis = bones['Pelvis']
    pelvis.location = pelvis.bone.matrix_local.to_quaternion().inverted() @ displacement
    yaw_rotation = Quaternion(UP,radians(yaw))
    rotate_in_world(pelvis,yaw_rotation @ Quaternion(X,radians(pelvis_pitch)))
    rotate_in_world(bones['Spine'],Quaternion(X,radians(spine_pitch)))
    rotate_in_world(bones['Chest'],Quaternion(X,radians(chest_pitch)))
    turn = -pelvis_pitch/38
    rotate_in_world(bones['Clavicle.R'],Quaternion(Y,radians(-3*turn)))
    rotate_in_world(bones['Clavicle.L'],Quaternion(Y,radians(1.4*turn)))
    bpy.context.view_layer.update()
    head = bones['Head']
    gaze = yaw_rotation @ Quaternion(X,radians(-36*turn))
    set_global_rotation(head,gaze @ head.bone.matrix_local.to_quaternion())
    left_reach = sample(frame,1,LEFT_ARM_REACH)
    free_arm_body = yaw_rotation @ Quaternion(X,radians((pelvis_pitch+spine_pitch)*.25))
    # Guide the elbow forward and out around the raised knee.
    clearance = smoothstep(limit(turn,0,1))
    left_reach += .30*clearance
    outward_reach = .20 + .26*clearance
    swing_left_arm(rig,free_arm_body,Vector((-outward_reach,left_reach,-.98)),18+15*turn,-4*turn)


def soft_limit(value, maximum, margin):
    start = maximum-margin
    amount = limit((abs(value)-start)/(2*margin),0,1)
    if amount == 0:
        return value
    return copysign(start+margin*amount*(2-amount),value)


def wrist_target(rig,frame):
    target, direction, palm = (sample(frame,i,REACH) for i in (1,2,3))
    desired = axes(direction,palm)
    return target - desired @ Vector(SOCKET_OFFSET) - rig.pose.bones['UpperArm.R'].head, desired


def elbow_profile(rig):
    # Derive the bend at contact from the exact ground target. Elsewhere the
    # bend follows one curve through the reach and lift.
    crouch(rig,GRAB_FRAME)
    contact_line,_ = wrist_target(rig,GRAB_FRAME)
    cosine = (contact_line.length_squared-UPPER_LENGTH**2-FOREARM_LENGTH**2)/(2*UPPER_LENGTH*FOREARM_LENGTH)
    grab = limit(degrees(acos(limit(cosine,-1,1))),*ELBOW_LIMITS)
    return (
        (0,12),(6,21),(11,23),(16,18),(19,14),
        (GRAB_FRAME,grab),(24,grab+2),(28,24),(33,32),(38,41),(45,46.5),
    )


def reach(rig, frame, neutral, bends, idle_pole, hold_pole):
    bones = rig.pose.bones
    pole, grip = (sample(frame,i,REACH) for i in (4,5))
    if frame < 9:
        pole = idle_pole.lerp(pole.normalized(),smoothstep(frame/9))
    elif frame > 36:
        pole = pole.normalized().lerp(hold_pole,smoothstep((frame-36)/9))
    line, desired = wrist_target(rig,frame)
    aim = line.normalized()
    flexion = radians(sample(frame,1,bends))
    distance = sqrt(UPPER_LENGTH**2+FOREARM_LENGTH**2+2*UPPER_LENGTH*FOREARM_LENGTH*cos(flexion))
    along = (UPPER_LENGTH**2-FOREARM_LENGTH**2+distance**2)/(2*distance)
    bend = (pole - aim*aim.dot(pole)).normalized()
    elbow = aim*along + bend*sqrt(max(0,UPPER_LENGTH**2-along**2))
    upper = elbow.normalized()
    lower = (aim*distance-elbow).normalized()
    hinge = upper.cross(lower).normalized()
    forearm = joint_basis(hinge,lower)
    neutral_arm, neutral_roll, neutral_wrist = neutral
    set_global_rotation(bones['UpperArm.R'],joint_basis(hinge,upper))
    bones['Forearm.R'].rotation_quaternion = Quaternion(X,flexion-neutral_arm.flexion)
    hand = forearm.inverted() @ desired
    _, roll = hand.to_swing_twist('Y')
    roll = radians(soft_limit(degrees(roll),FOREARM_ROLL_LIMIT,5))
    bones['ForearmRoll.R'].rotation_quaternion = Quaternion(Y,roll-neutral_roll)
    swing = Quaternion(Y,-roll) @ hand
    vector, angle = swing.to_axis_angle()
    swing_vector = vector*degrees(angle)
    swing = wrist_rotation(soft_limit(swing_vector.x,WRIST_FLEX_LIMIT,5),
                           soft_limit(swing_vector.z,WRIST_DEVIATION_LIMIT,1.5))
    bones['Hand.R'].rotation_quaternion = neutral_wrist.inverted() @ swing
    grip_fingers(rig,limit(grip,0,1),'R')
    grip_fingers(rig,.30,'L')


def capture(rig, action, frame):
    windup, slot = action
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    rig.animation_data.action = windup
    if slot is not None:
        rig.animation_data.action_slot = slot
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    bones = rig.pose.bones
    shoulder = bones['UpperArm.R'].head
    elbow = bones['Forearm.R'].head-shoulder
    aim = (bones['Hand.R'].head-shoulder).normalized()
    bend = (elbow-aim*elbow.dot(aim)).normalized()
    pose = {bone.name:(bone.rotation_quaternion.copy(),bone.location.copy()) for bone in bones}
    return pose,bend


def smoothstep(value):
    return value*value*(3-2*value)


def generate_pickup_action(rig, actions, legs):
    idle,idle_pole = capture(rig,actions['Throw'],THROW_END)
    hold,hold_pole = capture(rig,actions['Hold'],0)
    neutral = matching_rest(IDLE)
    animation = rig.animation_data
    action = bpy.data.actions.new(ACTION)
    action.use_fake_user = True
    action['fps'] = FPS
    action['loop'] = False
    action['next_action'] = 'ThrowObject_Hold'
    marker = action.pose_markers.new(GRAB_EVENT)
    marker.frame = GRAB_FRAME
    animation.action = None
    bends = elbow_profile(rig)
    animation.action = action
    bones = rig.pose.bones
    previous = {}
    for frame in range(PICKUP_END+1):
        bpy.context.scene.frame_set(frame)
        base = hold if frame == PICKUP_END else idle
        for bone in bones:
            bone.rotation_quaternion, bone.location = base[bone.name]
        bpy.context.view_layer.update()
        if 0 < frame < PICKUP_END:
            crouch(rig,frame)
            reach(rig,frame,neutral,bends,idle_pole,hold_pole)
            if frame < 5 or frame > 36:
                target = idle if frame < 5 else hold
                weight = smoothstep(1-frame/5 if frame < 5 else (frame-36)/9)
                for bone in bones:
                    rotation, location = target[bone.name]
                    bone.rotation_quaternion = bone.rotation_quaternion.slerp(rotation,weight)
                    bone.location = bone.location.lerp(location,weight)
            bpy.context.view_layer.update()
            set_legs(rig,legs)
        for bone in bones:
            rotation = bone.rotation_quaternion
            if bone.name in previous and rotation.dot(previous[bone.name]) < 0:
                rotation.negate()
            previous[bone.name] = rotation.copy()
            bone.keyframe_insert(data_path='rotation_quaternion',frame=frame,group=bone.name)
        bones['Pelvis'].keyframe_insert(data_path='location',frame=frame,group='Pelvis')
    slot = getattr(animation,'action_slot',None)
    for curve in action_curves(action,slot):
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    return action, slot

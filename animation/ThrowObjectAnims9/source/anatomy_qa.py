"""Measure the baked right-arm hierarchy in both animation clips."""
from math import atan2, degrees

from mathutils import Vector

from kinematics import leg_reference

from motion import DOWN, ELBOW_LIMITS, FOREARM_LENGTH, FOREARM_ROLL_LIMIT, HOLD_END, IDLE, SHOULDER_AXIAL_LIMITS, THROW_END
from motion import SHOULDER_ELEVATION_LIMIT, UPPER_LENGTH, WRIST_DEVIATION_LIMIT, WRIST_FLEX_LIMIT
from motion import X, Y, matching_rest


def vector_angle(a, b):
    return degrees(atan2(a.cross(b).length, a.dot(b)))


def rotation_angle(a, b):
    relative = a.inverted() @ b
    return degrees(2*atan2(Vector((relative.x, relative.y, relative.z)).length, abs(relative.w)))


def track_motion(rig, sample):
    reference, _, _ = matching_rest(IDLE)
    chest_rest = rig.data.bones['Chest'].matrix_local.inverted()
    result = {}
    motion_bones = ('UpperArm.R', 'Forearm.R', 'ForearmRoll.R', 'Hand.R')
    planted_feet = ('Foot.L','Foot.R')
    legs = [leg_reference(rig,side) for side in ('R','L')]
    for name, end in (('Hold', HOLD_END), ('Throw', THROW_END)):
        integer_frames = []
        worst_step = dict.fromkeys(motion_bones, 0.0)
        previous = None
        foot_drift = 0.0
        knee_heading_error = 0.0
        knee_hinge_error = 0.0
        first, hand_start = sample(name, 0)
        for i in range(4*end + 1):
            frame = i / 4
            poses, socket = sample(name, frame)
            upper, fore, roll, hand = (poses[bone] for bone in motion_bones)
            q_upper, q_fore, q_roll, q_hand = [bone.to_quaternion() for bone in (upper, fore, roll, hand)]
            upper_axis, fore_axis = q_upper @ Y, q_fore @ Y
            hinge = q_upper @ X
            flexion = degrees(atan2(upper_axis.cross(fore_axis).dot(hinge), upper_axis.dot(fore_axis)))
            fore_roll = degrees((q_fore.inverted() @ q_roll).to_swing_twist('Y')[1])
            hinge_error = vector_angle(hinge, q_fore @ X)
            roll_error = vector_angle(q_roll @ Y, fore_axis)
            wrist = q_roll.inverted() @ q_hand
            if wrist.w < 0:
                wrist.negate()
            wrist_axis, wrist_angle = wrist.to_axis_angle()
            wrist_vector = wrist_axis*degrees(wrist_angle)
            chest = (poses['Chest'] @ chest_rest)
            chest_upper = chest.to_quaternion().inverted() @ q_upper
            elevation = vector_angle(chest_upper @ Y, DOWN)
            transported = (reference.upper @ Y).rotation_difference(chest_upper @ Y) @ reference.upper
            shoulder_axial = degrees((transported.inverted() @ chest_upper).to_swing_twist('Y')[1])
            label = f'{name} frame {frame:g}'
            assert abs((fore.translation-upper.translation).length-UPPER_LENGTH) < 1e-4, label
            assert abs((hand.translation-fore.translation).length-FOREARM_LENGTH) < 1e-4, label
            assert hinge_error < .01 and roll_error < .01, f'{label} off-axis elbow or forearm'
            assert ELBOW_LIMITS[0]-.05 <= flexion <= ELBOW_LIMITS[1]+.05, label
            assert abs(fore_roll) <= FOREARM_ROLL_LIMIT+.05, label
            assert abs(wrist_vector.x) <= WRIST_FLEX_LIMIT+.05 and abs(wrist_vector.z) <= WRIST_DEVIATION_LIMIT+.05, label
            assert abs(wrist_vector.y) < .04, f'{label} excess wrist axial twist'
            assert elevation <= SHOULDER_ELEVATION_LIMIT+.10, label
            assert SHOULDER_AXIAL_LIMITS[0]-.05 <= shoulder_axial <= SHOULDER_AXIAL_LIMITS[1]+.05, label
            assert (poses['Root'].translation-first['Root'].translation).length < 1e-6, label
            for bone in planted_feet:
                foot_drift = max(foot_drift,(poses[bone].translation-first[bone].translation).length)
            assert foot_drift < .005, f'{label} lost its planted stance'
            for leg in legs:
                thigh, shin, foot = (poses[bone] for bone in (leg.thigh,leg.shin,leg.foot))
                axis = (foot.translation-thigh.translation).normalized()
                knee_aim = shin.translation-thigh.translation
                actual = (knee_aim-axis*knee_aim.dot(axis)).normalized()
                expected = (leg.stance_pole-axis*leg.stance_pole.dot(axis)).normalized()
                knee_heading_error = max(knee_heading_error,vector_angle(actual,expected))
                upper_hinge = thigh.to_quaternion() @ leg.upper_correction.inverted() @ X
                lower_hinge = shin.to_quaternion() @ leg.lower_correction.inverted() @ X
                knee_hinge_error = max(knee_hinge_error,vector_angle(upper_hinge,lower_hinge))
            assert knee_heading_error < 2, f'{label} knee twisted away from the planted foot'
            assert knee_hinge_error < 1, f'{label} knee bend slipped off the hinge'

            # The right forearm centerline and wrist must clear the body and head.
            chest_inverse = chest.inverted()
            head_inverse = poses['Head'].inverted()
            clearances = []
            for point in (fore.translation, fore.translation.lerp(hand.translation, .5), hand.translation):
                neutral_point = chest_inverse @ point
                head_point = head_inverse @ point
                if 1.025 < neutral_point.z < 1.448 and abs(neutral_point.x) < .27:
                    torso_clearance = (neutral_point.x/.225)**2 + ((neutral_point.y+.010)/.132)**2
                    assert torso_clearance > 1.0, f'{label} forearm enters torso'
                    clearances.append(torso_clearance)
                head_clearance = (head_point.x/.114)**2 + ((head_point.y-.100)/.145)**2 + ((head_point.z+.006)/.115)**2
                assert head_clearance > 1.0, f'{label} forearm enters head'
                clearances.append(head_clearance)

            if i % 4 == 0:
                current = [q_upper, q_fore, q_roll, q_hand]
                if previous:
                    for bone, a, b in zip(motion_bones, previous, current):
                        worst_step[bone] = max(worst_step[bone], rotation_angle(a,b))
                previous = current
                integer_frames.append({
                    'frame': i//4,
                    'shoulder_elevation_deg': round(elevation,3),
                    'shoulder_axial_deg': round(shoulder_axial,3),
                    'elbow_flexion_deg': round(flexion,3),
                    'elbow_hinge_axis_error_deg': round(hinge_error,5),
                    'forearm_roll_deg': round(fore_roll,3),
                    'wrist_flexion_deg': round(wrist_vector.x,3),
                    'wrist_deviation_deg': round(wrist_vector.z,3),
                    'wrist_axial_error_deg': round(wrist_vector.y,5),
                    'min_head_or_torso_clearance': round(min(clearances),3),
                    'socket_position_m': [round(value,7) for value in socket],
                    'pelvis_position_m': [round(value,7) for value in poses['Pelvis'].translation],
                })
        assert worst_step['UpperArm.R'] < 57, f'{name} upper arm changes too abruptly'
        assert all(change < 75 for change in worst_step.values()), f'{name} arm changes too abruptly'
        result[name] = {
            'samples_checked': 4*end+1,
            'max_planted_ankle_error_m': foot_drift,
            'max_knee_heading_error_deg': knee_heading_error,
            'max_knee_hinge_error_deg': knee_hinge_error,
            'max_world_rotation_degrees_per_frame': {bone:round(value,3) for bone,value in worst_step.items()},
            'max_socket_offset_from_first_frame_m': max(
                (Vector(item['socket_position_m'])-hand_start).length for item in integer_frames
            ),
            'frames': integer_frames,
        }
    return result

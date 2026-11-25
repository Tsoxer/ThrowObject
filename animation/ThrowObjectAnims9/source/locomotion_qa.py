"""Validate the baked carry clips and their prop socket."""
from math import degrees

from mathutils import Vector

from anatomy_qa import rotation_angle, vector_angle
from locomotion import GAITS, foot_target
from motion import DIGITS, X, Y


def check_locomotion(rig, sample, legs, ground):
    results = {}
    shoe_space = {leg.foot: rig.data.bones[leg.foot].matrix_local.inverted() for leg in legs}
    fingertip_names = [f'{digit}3.R' for digit in DIGITS]
    held, _ = sample('Hold',0)

    def fingertips(poses):
        palm = poses['Hand.R'].inverted()
        return [palm @ (poses[name] @ Vector((0,rig.data.bones[name].length,0)))
                for name in fingertip_names]

    grip = fingertips(held)
    for gait in GAITS.values():
        start, socket_start = sample(gait.action,0)
        finish, socket_end = sample(gait.action,gait.end)
        assert (socket_start-socket_end).length < 1e-6
        for name in start:
            assert (start[name].translation-finish[name].translation).length < 1e-6
            assert rotation_angle(start[name].to_quaternion(),finish[name].to_quaternion()) < .001
        summary = {'min_knee_flexion_deg':180,'max_knee_flexion_deg':0,
                   'worst_ground_penetration_m':0,'worst_stance_height_error_m':0,
                   'max_baked_ankle_target_error_m':0,'max_grip_error_m':0,
                   'samples_with_both_feet_airborne':0,'samples_checked':4*gait.end+1}
        frames = []
        for i in range(4*gait.end+1):
            frame = i/4
            phase = frame/gait.end if frame < gait.end else 0
            poses, socket = sample(gait.action,frame)
            assert poses['Root'].translation.length < 1e-7, 'Locomotion root translated'
            upper, elbow, roll = [poses[b].to_quaternion() for b in ('UpperArm.R','Forearm.R','ForearmRoll.R')]
            assert vector_angle(upper@X,elbow@X) < .01
            assert vector_angle(elbow@Y,roll@Y) < .01
            grip_error = max((a-b).length for a,b in zip(grip,fingertips(poses)))
            assert grip_error < 1e-6, 'Right-hand grip changed during locomotion'
            summary['max_grip_error_m'] = max(grip_error,summary['max_grip_error_m'])
            frame_values = {'frame':i//4,'socket_position_m':list(socket)}
            heights = []
            for leg,offset in zip(legs,(0,.5)):
                foot = poses[leg.foot]
                transform = foot@shoe_space[leg.foot]
                shoe_height = min((transform@(leg.rest_ankle+point)).z for point in leg.shoe)-ground
                phase_offset = (phase+offset)%1
                if phase_offset < gait.stance:
                    summary['worst_stance_height_error_m'] = max(abs(shoe_height),summary['worst_stance_height_error_m'])
                summary['worst_ground_penetration_m'] = max(-shoe_height,summary['worst_ground_penetration_m'])
                heights.append(shoe_height)
                upper = poses[leg.shin].translation-poses[leg.thigh].translation
                lower = foot.translation-poses[leg.shin].translation
                knee = degrees(upper.angle(lower))
                assert 6 <= knee <= 145, f'{gait.title} knee {knee:.2f}° at frame {frame:g}'
                summary['min_knee_flexion_deg'] = min(knee,summary['min_knee_flexion_deg'])
                summary['max_knee_flexion_deg'] = max(knee,summary['max_knee_flexion_deg'])
                if i%4 == 0:
                    target,_ = foot_target(gait,leg,phase_offset,ground)
                    target_error = (target-foot.translation).length
                    summary['max_baked_ankle_target_error_m'] = max(target_error,summary['max_baked_ankle_target_error_m'])
                    frame_values[leg.foot] = {'ankle_m':list(foot.translation),'shoe_clearance_m':shoe_height,
                                              'knee_flexion_deg':knee,'planned_stance':phase_offset<gait.stance}
            if min(heights) > .018:
                summary['samples_with_both_feet_airborne'] += 1
            if i%4 == 0:
                frames.append(frame_values)
        assert summary['max_baked_ankle_target_error_m'] < .00005, (gait.title,summary)
        assert summary['worst_stance_height_error_m'] < .020, (gait.title,summary)
        assert summary['worst_ground_penetration_m'] < .020, (gait.title,summary)
        assert bool(summary['samples_with_both_feet_airborne']) == (gait.stance < .5), (gait.title,summary)
        results[gait.action] = {'summary':summary,'frames':frames}
    return results

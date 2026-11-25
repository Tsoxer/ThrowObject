"""Retarget the authored Blender actions onto a local Skyrim SE skeleton and write HKX."""
import argparse
import json
from math import degrees, pi
from pathlib import Path
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'source'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from locomotion import GAITS
from motion import FPS, HOLD_END, RELEASE, RELEASE_EVENT, THROW_END
from pickup import GRAB_EVENT, GRAB_FRAME, PICKUP_END
from bone_retarget import NAMES, Retarget, palm_frame, rotation, world_pose


def load_codec(addon_folder):
    # The pinned HKX reader/writer is standalone Python. Import just the
    # codec to avoid loading the unrelated Windows-only NIF mesh DLL.
    module_path = addon_folder / 'hkx'
    if not (module_path / 'anim_skyrim.py').is_file():
        raise ValueError('Pass the extracted PyNifly V30.3.0 io_scene_nifly folder to --pynifly.')
    sys.path.insert(0, str(module_path))
    import anim_skyrim
    return anim_skyrim


def check_se_header(path):
    with path.open('rb') as stream:
        header = stream.read(64)
    if len(header) != 64 or header[:8] != bytes.fromhex('57e0e05710c0c010') or header[16] != 8 or b'hk_2010' not in header:
        raise ValueError(f'{path.name} must be an x64 Skyrim SE hk_2010 binary packfile.')


def q_to_hkx(value):
    q = value.normalized()
    return [q.x, q.y, q.z, q.w]


def idle_base(skeleton, idle, frame):
    count = len(skeleton.bones)
    if idle.blend_hint != 0 or not 0 <= frame < idle.num_frames:
        raise ValueError(f'Choose a non-additive standing idle and an idle sample in 0–{idle.num_frames-1}.')
    if idle.original_skeleton_name and idle.original_skeleton_name != skeleton.name:
        raise ValueError('The idle and skeleton have different binding names.')
    binding = idle.track_to_bone_indices or list(range(idle.num_tracks))
    if len(binding) != len(idle.tracks) or len(set(binding)) != len(binding) or any(i < 0 or i >= count for i in binding):
        raise ValueError('The idle has a transform binding incompatible with the supplied skeleton.')
    tracks = dict(zip(binding, idle.tracks))
    base = []
    for index, rest in enumerate(skeleton.reference_pose):
        track = tracks.get(index)
        t = track.translations[frame] if track and track.translations else rest.translation
        q = track.rotations[frame] if track and track.rotations else rest.rotation
        s = track.scales[frame] if track and track.scales else rest.scale
        base.append((Vector(t), rotation(q), Vector(s)))
    slots = idle.float_slot_indices or list(range(len(idle.float_tracks)))
    if len(slots) != len(idle.float_tracks) or any(i < 0 or i >= len(skeleton.float_slots) for i in slots):
        raise ValueError('The idle has a float binding incompatible with the supplied skeleton.')
    values = [samples[frame] for samples in idle.float_tracks]
    return base, slots, values


def set_action(rig, action):
    animation = rig.animation_data
    animation.action = action
    if hasattr(animation, 'action_slot') and animation.action_slot is None and action.slots:
        animation.action_slot = action.slots[0]


def rotation_angle(a, b):
    angle = a.rotation_difference(b).angle
    return degrees(min(angle, abs(2*pi-angle)))


def blend_weight(label, frame):
    # The two authored handoffs between custom actions stay at full weight.
    if label == 'PickUp':
        t = min(1, frame/4)
    elif label == 'Throw':
        t = min(1, (THROW_END-frame)/4)
    else:
        return 1
    return t*t*(3-2*t)


def decoded_pose(clip, frame):
    return [(Vector(t.translations[frame]), rotation(t.rotations[frame]), Vector(t.scales[frame]))
            for t in clip.tracks]


def anatomical_errors(retarget, source, source_rotations, decoded):
    transforms = world_pose(retarget.parents, decoded)
    target = {role: transforms[index].translation for role, index in retarget.indices.items()}
    errors = {}
    for role, distal in retarget.directions.items():
        source_head = source[role]
        source_tail = (source[f'Toe.{role[-1]}'] if role.startswith('Foot.') else source[distal])
        expected = retarget.heading @ (source_tail-source_head)
        errors[role] = degrees(expected.angle(target[distal]-target[role]))
    for side in ('R', 'L'):
        errors[f'Palm.{side}'] = rotation_angle(retarget.heading @ palm_frame(source, side),
                                               palm_frame(target, side))
    for role, posed in source_rotations.items():
        expected = retarget.heading @ posed @ retarget.calibration[role]
        errors[role] = rotation_angle(expected, transforms[retarget.indices[role]].to_quaternion())
    return errors


def check_handoff(label, first, second):
    rotation_error = max(rotation_angle(a[1], b[1]) for a,b in zip(first,second,strict=True))
    position_error = max((a[0]-b[0]).length for a,b in zip(first,second,strict=True))
    scale_error = max((a[2]-b[2]).length for a,b in zip(first,second,strict=True))
    if rotation_error>.10 or position_error>.025 or scale_error>.002:
        raise RuntimeError(f'{label} disagrees by {rotation_error:.3f}° and {position_error:.4f} game units.')
    return {'degrees':rotation_error,'game_units':position_error,'scale':scale_error}


def shoulder_metrics(retarget, pose, source, weight):
    indices = retarget.indices
    world = world_pose(retarget.parents, pose)
    chest_inverse = world[indices['Chest']].inverted()
    neutral_chest_inverse = retarget.idle_world[indices['Chest']].inverted()
    joints = [indices[f'UpperArm.{side}'] for side in ('R', 'L')]
    points = [chest_inverse @ world[index].translation for index in joints]
    neutral = [neutral_chest_inverse @ retarget.idle_world[index].translation for index in joints]
    actual_angles = [rotation_angle(retarget.idle[index][1], pose[index][1]) for index in retarget.clavicles]
    expected_angles = [weight * source[role] for role, _ in retarget.clavicles.values()]
    return {
        'width': (points[0]-points[1]).length,
        'neutral_width': (neutral[0]-neutral[1]).length,
        'chest_space_pivot_drift': max((a-b).length for a,b in zip(points,neutral,strict=True)),
        'native_local_scale_error': max((pose[i][2]-retarget.idle[i][2]).length for i in retarget.shoulder_layout),
        'native_local_translation_error': max((pose[i][0]-retarget.idle[i][0]).length for i in retarget.shoulder_layout),
        'max_unauthored_clavicle_rotation_degrees': max(abs(a-b) for a,b in zip(actual_angles,expected_angles,strict=True)),
    }


def export_bound_clips(codec, skeleton, idle, idle_frame, out, rig, compare=None):
    gait_ranges = {gait.action:gait.end for gait in GAITS.values()
                   if bpy.data.actions.get('ThrowObject_'+gait.action)}
    local_base, float_slots, float_values = idle_base(skeleton, idle, idle_frame)
    retarget = Retarget(skeleton, rig, local_base)
    scene = bpy.context.scene
    scene.render.fps, scene.render.fps_base = FPS, 1

    report = {}
    clips = {}
    ranges = [('Hold',HOLD_END),('Throw',THROW_END),*gait_ranges.items()]
    if bpy.data.actions.get('ThrowObject_PickUp'):
        ranges.append(('PickUp',PICKUP_END))
    events = {'Throw':(RELEASE_EVENT,RELEASE),'PickUp':(GRAB_EVENT,GRAB_FRAME)}
    for label,last_frame in ranges:
        action = bpy.data.actions['ThrowObject_'+label]
        set_action(rig, action)
        frame_count = last_frame + 1
        annotation = []
        if label in events:
            name,frame = events[label]
            markers = [marker for marker in action.pose_markers if marker.name == name]
            if len(markers) != 1 or markers[0].frame != frame or not 0 < frame < last_frame:
                raise ValueError(f'{action.name} has an invalid {name} marker.')
            annotation.append(codec.Annotation(time=frame/FPS,text=name))
        result = codec.AnimationData(
            duration=last_frame/FPS, num_frames=frame_count, frame_duration=1/FPS,
            num_tracks=len(skeleton.bones), track_to_bone_indices=list(range(len(skeleton.bones))),
            original_skeleton_name=skeleton.name, annotations=annotation, blend_hint=0, rot_quant=2,
            float_slot_indices=float_slots, float_tracks=[[value]*frame_count for value in float_values],
            tracks=[codec.TrackData() for _ in skeleton.bones],
        )
        source_frames = []
        for frame in range(frame_count):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            bones = rig.pose.bones
            source = {bone.name: bone.head.copy() for bone in bones}
            for side in ('R', 'L'):
                source[f'Toe.{side}'] = bones[f'Foot.{side}'].tail.copy()
            body = {role: bones[role].matrix.to_quaternion()
                    for role in ('Pelvis', 'Spine', 'Chest', 'Neck', 'Head')}
            clavicles = {role: rotation_angle(rotation((0,0,0,1)), bones[role].matrix_basis.to_quaternion())
                         for role, _ in retarget.clavicles.values()}
            source_frames.append((source,body,clavicles))
            pose = retarget.frame(bones, blend_weight(label, frame))
            for track, (t, q, s) in zip(result.tracks, pose, strict=True):
                track.translations.append(list(t))
                if track.rotations and rotation(track.rotations[-1]).dot(q) < 0:
                    q.negate()
                track.rotations.append(q_to_hkx(q))
                track.scales.append(list(s))

        path = out / f'{action.name}.hkx'
        codec.write_skyrim_animation(str(path), result, ptr_size=8)
        check_se_header(path)
        decoded = codec.load_skyrim_animation(str(path))
        clips[label] = decoded
        if decoded.original_skeleton_name != result.original_skeleton_name or decoded.track_to_bone_indices != result.track_to_bone_indices:
            raise RuntimeError(f'{path.name} failed the binding round trip.')
        if decoded.num_frames != result.num_frames or abs(decoded.duration-result.duration) > .0001:
            raise RuntimeError(f'{path.name} failed the timing round trip.')
        if decoded.float_slot_indices != result.float_slot_indices or len(decoded.float_tracks) != len(result.float_tracks):
            raise RuntimeError(f'{path.name} failed the float-slot binding round trip.')
        if [(a.text, round(a.time, 4)) for a in decoded.annotations] != [(a.text, round(a.time, 4)) for a in annotation]:
            raise RuntimeError(f'{path.name} failed the annotation round trip.')
        max_drift = max(rotation_angle(rotation(a), rotation(b))
                        for original, roundtrip in zip(result.tracks, decoded.tracks)
                        for a, b in zip(original.rotations, roundtrip.rotations))
        if max_drift > .85:
            raise RuntimeError(f'{path.name} changed by {max_drift:.3f}° in HKX compression.')
        seam_drift = max(rotation_angle(rotation(track.rotations[0]), rotation(track.rotations[-1]))
                         for track in decoded.tracks)
        looping = label not in events
        if looping and seam_drift > .15:
            raise RuntimeError(f'{path.name} has a {seam_drift:.3f}° loop seam in HKX.')
        review = []
        baseline = codec.load_skyrim_animation(str(compare/path.name)) if compare else None
        for frame, (source, body, clavicles) in enumerate(source_frames):
            pose = decoded_pose(decoded, frame)
            weight = blend_weight(label, frame)
            new_errors = anatomical_errors(retarget, source, body, pose)
            shoulders = shoulder_metrics(retarget, pose, clavicles, weight)
            if (shoulders['max_unauthored_clavicle_rotation_degrees']>.10 or
                shoulders['native_local_scale_error']>.002 or shoulders['native_local_translation_error']>.025):
                raise RuntimeError(f'{path.name} frame {frame} changed the neutral shoulder layout or authored motion.')
            row = {'frame': frame, 'seconds': round(frame/FPS, 6),
                   'authored_weight': weight,
                   'corrected_degrees': {k: round(v, 4) for k,v in new_errors.items()},
                   'shoulders': {k: round(v, 5) for k,v in shoulders.items()}}
            if baseline:
                previous = decoded_pose(baseline, frame)
                old_errors = anatomical_errors(retarget, source, body, previous)
                row['previous_degrees'] = {k: round(v, 4) for k,v in old_errors.items()}
                row['previous_shoulders'] = {k: round(v, 5) for k,v in
                                             shoulder_metrics(retarget, previous, clavicles, weight).items()}
            review.append(row)
        no_fade = [row for row in review if row['authored_weight'] == 1]
        worst = max(max(row['corrected_degrees'].values()) for row in no_fade)
        if worst > 1:
            raise RuntimeError(f'{path.name} failed decoded anatomical pose alignment ({worst:.3f}°).')
        report[action.name] = {
            'frame_count': decoded.num_frames, 'duration_seconds': decoded.duration,
            'binding_tracks': decoded.num_tracks, 'float_tracks': len(decoded.float_tracks),
            'max_roundtrip_rotation_error_degrees': max_drift,
            'loop_seam_degrees': seam_drift if looping else None,
            'max_source_alignment_error_degrees_full_weight': worst,
            'frames': review,
            'events': [{'event': a.text, 'seconds': a.time} for a in decoded.annotations],
        }
    hold_start = decoded_pose(clips['Hold'], 0)
    report['ThrowObject_Throw']['handoffs'] = {
        'hold_to_throw': check_handoff('Hold to Throw',hold_start,decoded_pose(clips['Throw'],0)),
        'throw_to_idle': check_handoff('Throw to Idle',decoded_pose(clips['Throw'],THROW_END),local_base),
    }
    if 'PickUp' in clips:
        report['ThrowObject_PickUp']['handoffs'] = {
            'idle_to_pickup': check_handoff('Idle to Pick Up',local_base,decoded_pose(clips['PickUp'],0)),
            'pickup_to_hold': check_handoff('Pick Up to Hold',decoded_pose(clips['PickUp'],PICKUP_END),hold_start),
        }
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pynifly', type=Path, required=True)
    parser.add_argument('--skeleton', type=Path, required=True)
    parser.add_argument('--idle', type=Path, required=True)
    parser.add_argument('--idle-frame', type=int, default=0, help='Zero-based sample in the supplied vanilla idle')
    parser.add_argument('--compare', type=Path, help='Optional previous HKX folder for per-frame comparison')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    codec = load_codec(args.pynifly)
    check_se_header(args.skeleton)
    check_se_header(args.idle)
    skeleton = codec.load_skyrim_skeleton(str(args.skeleton))
    if not skeleton or len(skeleton.bones) != 99 or skeleton.name != NAMES['Root'] or not set(NAMES.values()).issubset(skeleton.bones):
        raise ValueError('Select the vanilla third-person human skeleton.hkx from your installation.')
    rig = bpy.data.objects.get('Throw Object')
    if not rig or rig.type != 'ARMATURE':
        raise ValueError('Open the project’s generated/ThrowObject.blend in Blender.')
    args.out.mkdir(parents=True, exist_ok=True)
    result = export_bound_clips(codec, skeleton, codec.load_skyrim_animation(str(args.idle)),
                                args.idle_frame, args.out, rig, args.compare)
    (args.out/'hkx-qa.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: {field: value for field, value in row.items() if field != 'frames'}
                      for k, row in result.items()}, indent=2))
    print('HKX conversion round-tripped. Runtime playback still requires testing in Skyrim SE 1.6.1170.')


if __name__ == '__main__':
    main()

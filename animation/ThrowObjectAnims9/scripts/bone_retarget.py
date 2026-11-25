"""Map evaluated Anims9 anatomy to Skyrim model space, then to HKX local space."""
from mathutils import Matrix, Quaternion, Vector

from motion import DIGITS, Y, Z, axes, joint_basis

NAMES = {
    'Root': 'NPC Root [Root]', 'Pelvis': 'NPC Pelvis [Pelv]', 'COM': 'NPC COM [COM ]',
    'Spine': 'NPC Spine1 [Spn1]', 'Chest': 'NPC Spine2 [Spn2]',
    'Neck': 'NPC Neck [Neck]', 'Head': 'NPC Head [Head]',
    **{f'{role}.{side}': f'NPC {side} {name} [{side}{tag}]'
       for side in ('R', 'L') for role, name, tag in (
           ('Clavicle', 'Clavicle', 'Clv'), ('UpperArm', 'UpperArm', 'Uar'),
           ('Forearm', 'Forearm', 'Lar'), ('Hand', 'Hand', 'Hnd'),
           ('Thigh', 'Thigh', 'Thg'), ('Shin', 'Calf', 'Clf'), ('Foot', 'Foot', 'ft '),
           ('Toe', 'Toe0', 'Toe'))},
    **{f'{name}{part+1}.{side}': f'NPC {side} Finger{i}{part} [{side}F{i}{part}]'
       for side in ('R', 'L') for i, name in enumerate(DIGITS) for part in range(3)},
}


def rotation(value):
    x, y, z, w = value
    return Quaternion((w, x, y, z)).normalized()


def world_pose(parents, local):
    world = []
    for index, ((t, q, s), parent) in enumerate(zip(local, parents, strict=True)):
        if parent >= index:
            raise ValueError('Expected parent-first HKX skeleton')
        matrix = Matrix.LocRotScale(t, q, s)
        world.append(world[parent] @ matrix if parent >= 0 else matrix)
    return world


def palm_frame(positions, side):
    forward = (positions[f'Middle1.{side}'] - positions[f'Hand.{side}']).normalized()
    across = positions[f'Index1.{side}'] - positions[f'Little1.{side}']
    normal = across.cross(forward)
    return axes(forward, normal if side == 'R' else -normal)


def unit(value):
    if value.length_squared < 1e-10:
        raise ValueError('Anatomical landmarks are degenerate')
    return value.normalized()


def chain_axes(points, upper, lower, distal):
    up = unit(points[lower] - points[upper])
    down = unit(points[distal] - points[lower])
    hinge = unit(up.cross(down))
    return joint_basis(hinge, up), joint_basis(hinge, down)


class Retarget:
    def __init__(self, skeleton, rig, idle):
        missing = set(NAMES.values()) - set(skeleton.bones)
        if missing:
            raise ValueError(f'Missing vanilla bones: {sorted(missing)}')
        self.indices = {role: skeleton.bones.index(name) for role, name in NAMES.items()}
        self.parents = skeleton.parents
        self.idle = idle
        self.rest = [(Vector(p.translation), rotation(p.rotation), Vector(p.scale))
                     for p in skeleton.reference_pose]
        self.world = world_pose(self.parents, self.rest)
        self.source = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
        self.source_points = {role: matrix.translation for role, matrix in self.source.items()}
        self.target_points = {role: self.world[index].translation for role, index in self.indices.items()}
        right = self.target_points['Thigh.R'] - self.target_points['Thigh.L']
        right = unit(right - Z * right.dot(Z))
        self.heading = joint_basis(right, Z.cross(right))
        eye = self.world[skeleton.bones.index('NPCEyeBone')].translation
        if (eye - self.target_points['Head']).dot(self.heading @ Y) <= 0:
            raise ValueError('Supplied skeleton does not face its anatomical forward axis')
        self.calibration = {}
        self.directions = {}
        self.clavicles = {}
        self.twists = []
        self.idle_world = world_pose(self.parents, self.idle)
        ratios = []
        for role in ('Pelvis', 'Spine', 'Chest', 'Neck', 'Head'):
            self.set_neutral(role, self.target_rotation(role))
        for side in ('R', 'L'):
            clavicle, upper, lower, hand = (f'{r}.{side}' for r in ('Clavicle', 'UpperArm', 'Forearm', 'Hand'))
            parent = self.parents[self.indices[clavicle]]
            idle_chest = self.idle_world[parent].to_quaternion()
            self.clavicles[self.indices[clavicle]] = (
                clavicle, idle_chest.inverted() @ self.heading @ self.source[clavicle].to_quaternion())
            self.limb(upper, lower, hand)
            self.calibration[hand] = palm_frame(self.target_points, side).inverted() @ self.target_rotation(hand)
            thigh, calf, foot, toe = (f'{r}.{side}' for r in ('Thigh', 'Shin', 'Foot', 'Toe'))
            self.limb(thigh, calf, foot)
            self.aim(foot, toe)
            source_length = sum((self.source_points[b] - self.source_points[a]).length
                                for a, b in ((thigh, calf), (calf, foot)))
            target_length = sum((self.target_points[b] - self.target_points[a]).length
                                for a, b in ((thigh, calf), (calf, foot)))
            ratios.append(target_length / source_length)
            for digit in DIGITS:
                first, second, tip = (f'{digit}{part}.{side}' for part in range(1, 4))
                self.aim(first, second)
                self.aim(second, tip)
                self.set_neutral(tip, self.target_rotation(tip),
                                 self.target_points[tip] - self.target_points[second])
        self.scale = sum(ratios) / len(ratios)
        for i, amount in ((1, .40), (2, .80)):
            name = f'NPC R ForearmTwist{i} [RLt{i}]'
            if name in skeleton.bones:
                self.twists.append((skeleton.bones.index(name), amount))
        self.forearm = self.indices['Forearm.R']
        self.forearm_axis = self.target_rotation('Forearm.R').inverted() @ unit(
            self.target_points['Hand.R'] - self.target_points['Forearm.R'])
        self.mapped = set(self.indices.values())
        self.shoulder_layout = {self.indices[f'{role}.{side}']
                                for role in ('Clavicle', 'UpperArm') for side in ('R', 'L')}

    def target_rotation(self, role):
        return self.world[self.indices[role]].to_quaternion()

    def set_neutral(self, role, target_neutral, target_direction=None):
        source = self.source[role].to_quaternion()
        if target_direction is not None:
            direction = self.heading @ (source @ Y)
            target_neutral = unit(target_direction).rotation_difference(direction) @ target_neutral
        self.calibration[role] = source.inverted() @ self.heading.inverted() @ target_neutral

    def aim(self, role, child):
        self.set_neutral(role, self.target_rotation(role),
                         self.target_points[child] - self.target_points[role])
        self.directions[role] = child

    def limb(self, upper, lower, distal):
        source_axes = chain_axes(self.source_points, upper, lower, distal)
        target_axes = chain_axes(self.target_points, upper, lower, distal)
        for role, source_axis, target_axis in zip((upper, lower), source_axes, target_axes, strict=True):
            self.calibration[role] = (self.source[role].to_quaternion().inverted() @ source_axis
                                      @ target_axis.inverted() @ self.target_rotation(role))
        self.directions.update({upper: lower, lower: distal})

    def frame(self, bones, weight):
        positions = {role: bone.head.copy() for role, bone in bones.items()}
        desired = {}
        for role, correction in self.calibration.items():
            source = palm_frame(positions, role[-1]) if role.startswith('Hand.') else bones[role].matrix.to_quaternion()
            desired[self.indices[role]] = self.heading @ source @ correction
        com, pelvis = (self.indices[role] for role in ('COM', 'Pelvis'))
        hips = self.world[pelvis].translation + self.heading @ (
            positions['Pelvis'] - self.source_points['Pelvis']) * self.scale
        roll = bones['ForearmRoll.R'].matrix_basis.to_quaternion().to_swing_twist('Y')[1]
        for helper, amount in self.twists:
            desired[helper] = (desired[self.forearm] @ Quaternion(self.forearm_axis, roll * amount)
                               @ self.rest[helper][1])

        local_pose, world = [], []
        for index, parent in enumerate(self.parents):
            use_reference = index not in self.shoulder_layout and (index in self.mapped or index in desired)
            t, q, s = self.rest[index] if use_reference else self.idle[index]
            parent_transform = world[parent] if parent >= 0 else Matrix.Identity(4)
            if index == com:
                t = parent_transform.inverted() @ hips - q @ (self.rest[pelvis][0] * s.x)
            if index in self.clavicles:
                # Keep the native shoulder socket in chest space. Transfer only
                # the source bone's authored local motion around that socket.
                role, basis = self.clavicles[index]
                q = basis @ bones[role].matrix_basis.to_quaternion() @ basis.inverted() @ q
            elif index in desired:
                q = parent_transform.to_quaternion().inverted() @ desired[index]
            local_pose.append((t, q.normalized(), s))
            world.append(parent_transform @ Matrix.LocRotScale(t, q, s))
        if weight == 1:
            return local_pose
        return [(ti.lerp(t, weight), qi.slerp(q, weight), si.lerp(s, weight))
                for (ti, qi, si), (t, q, s) in zip(self.idle, local_pose, strict=True)]

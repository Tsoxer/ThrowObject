"""Reference axes and the two-bone leg solution shared by stance and gait."""
from dataclasses import dataclass
from math import cos, radians, sqrt

from mathutils import Matrix, Quaternion, Vector


def joint_basis(x, y):
    return Matrix((x, y, x.cross(y))).transposed().to_quaternion()


@dataclass
class Leg:
    thigh: str
    shin: str
    foot: str
    upper: float
    lower: float
    upper_correction: Quaternion
    lower_correction: Quaternion
    rest_ankle: Vector
    rest_rotation: Quaternion
    side: int
    stance_pole: Vector
    shoe: tuple = ()


def leg_reference(rig, side):
    names = [f'{part}.{side}' for part in ('Thigh','Shin','Foot')]
    thigh, shin, foot = (rig.data.bones[name] for name in names)
    upper = (shin.head_local-thigh.head_local).normalized()
    lower = (foot.head_local-shin.head_local).normalized()
    axis = upper.cross(lower).normalized()
    neutral_line = (foot.head_local-thigh.head_local).normalized()
    # A knee pole is the bend direction away from the hip–ankle line.
    # Its reference vector must stay well away from parallel with that line.
    stance_pole = (upper - neutral_line*upper.dot(neutral_line)).normalized()
    return Leg(*names, thigh.length, shin.length,
               joint_basis(axis,upper).inverted() @ thigh.matrix_local.to_quaternion(),
               joint_basis(axis,lower).inverted() @ shin.matrix_local.to_quaternion(),
               foot.head_local.copy(), foot.matrix_local.to_quaternion(), 1 if side=='R' else -1, stance_pole)


def knee_solution(hip, ankle, leg, minimum=7, pole=None):
    line = ankle-hip
    distance = line.length
    direction = line.normalized()
    lengths = leg.upper**2+leg.lower**2
    max_reach = sqrt(lengths+2*leg.upper*leg.lower*cos(radians(minimum)))
    min_reach = sqrt(lengths+2*leg.upper*leg.lower*cos(radians(144)))
    if not min_reach <= distance <= max_reach:
        raise ValueError(f'{leg.foot} target is outside the permitted knee range ({distance:.3f} m, {min_reach:.3f}–{max_reach:.3f} m)')
    if pole is None:
        pole = Vector((leg.side*.12,1,0))
    bend = (pole-direction*direction.dot(pole)).normalized()
    along = (leg.upper**2-leg.lower**2+distance**2)/(2*distance)
    knee = hip + direction*along + bend*sqrt(max(0,leg.upper**2-along**2))
    upper = (knee-hip).normalized()
    lower = (ankle-knee).normalized()
    axis = upper.cross(lower).normalized()
    return joint_basis(axis,upper) @ leg.upper_correction, joint_basis(axis,lower) @ leg.lower_correction

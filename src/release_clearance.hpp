#pragma once

#include "core.hpp"

namespace to
{
struct ReleaseBounds {
    Vec half;
    std::array<Vec,3> axes;

    float extent(Vec axis) const
    {
        return projectedExtent(half,{axis.dot(axes[0]),axis.dot(axes[1]),axis.dot(axes[2])});
    }

    bool valid() const
    {
        return half.x>=0.f && half.y>=0.f && half.z>=0.f &&
               std::isfinite(half.length()) && half.length()>1.f &&
               std::all_of(axes.begin(),axes.end(),[](Vec axis) {
                   const auto length = axis.length();
                   return length>.98f && length<1.02f;
               });
    }
};

inline std::array<Vec,12> thirdPersonReleaseCandidates(const ReleaseBounds& box, Vec heldCenter,
                                                        Vec torso, Vec forward, Vec right,
                                                        Vec bottom, Vec top, float playerRadius)
{
    const Vec up{0,0,1};
    const Vec frontBase{torso.x,torso.y,std::clamp(heldCenter.z,bottom.z+11.f,top.z+24.f)};
    const float bodyClearance = playerRadius+10.f;
    const float sideReach = bodyClearance+box.extent(right);
    const float forwardReach = bodyClearance+box.extent(forward);
    const float handSide = std::clamp((heldCenter-torso).dot(right),9.f,26.f);
    const float lift = std::clamp(6.f+box.extent(up)*.45f,8.f,26.f);
    const auto besideHand = heldCenter+right*12.f;
    const auto outside = frontBase+forward*18.f+right*sideReach;
    const auto ahead = frontBase+forward*forwardReach;
    const auto aheadOfHand = ahead+right*handSide;
    return {heldCenter, heldCenter+up*lift, besideHand, besideHand+up*lift,
            heldCenter+up*(2.f*lift), outside, outside+up*lift,
            aheadOfHand, aheadOfHand+up*lift, ahead, ahead+up*lift, aheadOfHand+forward*12.f};
}

inline bool sweptBoundsClearCapsule(const ReleaseBounds& box, Vec center, Vec travel,
                                    Vec bottom, Vec top, float radius)
{
    const auto fromCapsule = Vec{center.x-bottom.x,center.y-bottom.y,0.f}.unit();
    // A separating plane remains valid throughout a straight translation.
    for (const auto axis : {box.axes[0],box.axes[1],box.axes[2],fromCapsule,Vec{0,0,1}}) {
        if (axis.length()<.5f) continue;
        const auto padding = box.extent(axis)+radius;
        const auto start = center.dot(axis);
        const auto finish = (center+travel).dot(axis);
        const auto low = std::min(bottom.dot(axis),top.dot(axis));
        const auto high = std::max(bottom.dot(axis),top.dot(axis));
        if (std::min(start,finish)-high>padding || low-std::max(start,finish)>padding) return true;
    }
    return false;
}

inline std::array<Vec,18> equipmentHandDropPoints(const ReleaseBounds& box, Vec heldCenter,
                                                  Vec playerAxis, Vec outward, float playerRadius)
{
    constexpr Vec up{0,0,1};
    const Vec across{outward.y,-outward.x,0.f};
    const std::array directions{outward,(outward+across).unit(),(outward-across).unit(),across,across*-1.f};
    const auto separation = heldCenter-playerAxis;
    std::array<Vec,18> points{heldCenter,heldCenter+up*5.f,heldCenter+up*10.f};
    std::size_t next = 3;
    for (const auto direction : directions) {
        const float shift = std::max(3.f,playerRadius+box.extent(direction)+1.f-separation.dot(direction));
        const auto clearOfBody = heldCenter+direction*shift;
        points[next++] = clearOfBody;
        points[next++] = clearOfBody+up*6.f;
        points[next++] = clearOfBody+direction*6.f;
    }
    std::stable_sort(points.begin()+1,points.end(),[heldCenter](Vec lhs, Vec rhs) {
        const auto left = lhs-heldCenter;
        const auto right = rhs-heldCenter;
        return left.dot(left)<right.dot(right);
    });
    return points;
}

inline std::optional<Vec> viewDropPoint(const ReleaseBounds& box, Vec eye, Vec forward,
                                       float nearPlane, float extraDepth)
{
    constexpr Vec up{0,0,1};
    const auto viewUp = (up-forward*forward.z).unit();
    if (!std::isfinite(eye.length()) || !std::isfinite(nearPlane) || nearPlane<=0.f ||
        !(forward.length()>.5f) || !(viewUp.length()>.5f)) return std::nullopt;
    const float depth = std::max(36.f,nearPlane+box.extent(forward)+6.f)+extraDepth;
    if (!(depth<=80.f)) return std::nullopt;
    // Keep the drop below the center of the view at any candidate depth.
    return eye+forward*depth-viewUp*std::min(depth*.24f,16.f);
}

inline std::array<Vec,16> groundDropFootprints(const ReleaseBounds& box, Vec feet, Vec forward,
                                              float playerRadius)
{
    const Vec right{forward.y,-forward.x,0.f};
    const auto frontRight = (forward+right).unit();
    const auto frontLeft = (forward-right).unit();
    const std::array directions{forward, frontRight, frontLeft, right, right*-1.f,
                                frontLeft*-1.f, frontRight*-1.f, forward*-1.f};
    std::array<Vec,16> footprints{};
    for (std::size_t i=0; i<directions.size(); ++i) {
        const auto direction = directions[i];
        const float reach = playerRadius+box.extent(direction)+8.f;
        footprints[i] = feet+direction*reach;
        footprints[i+directions.size()] = feet+direction*(reach+14.f);
    }
    return footprints;
}
}

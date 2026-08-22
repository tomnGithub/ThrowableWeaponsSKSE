#pragma once

#include "SaberThrow/SaberThrowCommon.hpp"

namespace SaberThrow
{
    inline constexpr float kPitchSign = 1.0f;
    inline constexpr float kPitchCompSign = -1.0f;
    inline constexpr float kRollCompSign = 1.0f;
    inline constexpr float kFlatBaseYaw = 1.57079632679f;

    inline constexpr bool kDefaultSpearMode = false;

    enum class NoReturnSpearTipAxis : std::uint8_t
    {
        LocalPositiveX,
        LocalNegativeX,
        LocalPositiveY,
        LocalNegativeY,
        LocalPositiveZ,
        LocalNegativeZ
    };

    inline constexpr NoReturnSpearTipAxis kSpearTipAxis = NoReturnSpearTipAxis::LocalPositiveY;

    inline constexpr float kSpearSnapExtra = 48.0f;

    inline constexpr float kPiRad = 3.14159265358979323846f;

    inline RE::NiPoint3 GetThrowDir(const RE::NiPoint3& from, const RE::NiPoint3& to)
    {
        RE::NiPoint3 dir{ to.x - from.x, to.y - from.y, to.z - from.z };
        if (!NormalizePoint(dir)) {
            return RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
        }
        return dir;
    }

    inline float GetThrowPitchFromDir(const RE::NiPoint3& from, const RE::NiPoint3& to)
    {
        const RE::NiPoint3 dir = GetThrowDir(from, to);
        const float horizontal = std::sqrt((dir.x * dir.x) + (dir.y * dir.y));
        return kPitchSign * std::atan2(dir.z, horizontal);
    }

    inline float GetThrowYawFromDir(const RE::NiPoint3& from, const RE::NiPoint3& to)
    {
        const RE::NiPoint3 dir = GetThrowDir(from, to);
        const float horizontal = std::sqrt((dir.x * dir.x) + (dir.y * dir.y));
        if (horizontal <= 1e-5f) {
            return 1.5707963f;
        }
        return Wrap0To2Pi(std::atan2(dir.x, dir.y));
    }

    inline void MakeThrowEuler(
        float rawThrowPitchRad,
        float throwYaw,
        float& outVisualPitchRad,
        float& outVisualRollRad)
    {
        outVisualPitchRad = kPitchCompSign * rawThrowPitchRad * std::cos(throwYaw);
        outVisualRollRad = kRollCompSign * rawThrowPitchRad * std::sin(throwYaw);
    }

    inline constexpr float kActorSweepRadius = 24.0f;

    inline RE::NiPoint3 CrossPoint(const RE::NiPoint3& a, const RE::NiPoint3& b)
    {
        return RE::NiPoint3{
            (a.y * b.z) - (a.z * b.y),
            (a.z * b.x) - (a.x * b.z),
            (a.x * b.y) - (a.y * b.x)
        };
    }

    inline RE::NiPoint3 NegPoint(const RE::NiPoint3& p)
    {
        return RE::NiPoint3{ -p.x, -p.y, -p.z };
    }

    inline RE::NiPoint3 RotateAroundAxis(
        const RE::NiPoint3& point,
        const RE::NiPoint3& axisNormalized,
        float angleRad)
    {
        const float c = std::cos(angleRad);
        const float s = std::sin(angleRad);

        const RE::NiPoint3 cross = CrossPoint(axisNormalized, point);
        const float dot =
            (axisNormalized.x * point.x) +
            (axisNormalized.y * point.y) +
            (axisNormalized.z * point.z);

        return RE::NiPoint3{
            (point.x * c) + (cross.x * s) + (axisNormalized.x * dot * (1.0f - c)),
            (point.y * c) + (cross.y * s) + (axisNormalized.y * dot * (1.0f - c)),
            (point.z * c) + (cross.z * s) + (axisNormalized.z * dot * (1.0f - c))
        };
    }

    inline void SetMatrixCols(
        RE::NiMatrix3& m,
        const RE::NiPoint3& localXAxisWorld,
        const RE::NiPoint3& localYAxisWorld,
        const RE::NiPoint3& localZAxisWorld)
    {
        m.entry[0][0] = localXAxisWorld.x;
        m.entry[1][0] = localXAxisWorld.y;
        m.entry[2][0] = localXAxisWorld.z;

        m.entry[0][1] = localYAxisWorld.x;
        m.entry[1][1] = localYAxisWorld.y;
        m.entry[2][1] = localYAxisWorld.z;

        m.entry[0][2] = localZAxisWorld.x;
        m.entry[1][2] = localZAxisWorld.y;
        m.entry[2][2] = localZAxisWorld.z;
    }

    inline RE::NiPoint3 MatrixToEuler(const RE::NiMatrix3& m)
    {
        const float sy = std::clamp(-m.entry[2][0], -1.0f, 1.0f);
        const float roll = std::asin(sy);
        const float cy = std::cos(roll);

        float pitch = 0.0f;
        float yaw = 0.0f;

        if (std::fabs(cy) > 1e-5f) {
            pitch = std::atan2(m.entry[2][1], m.entry[2][2]);
            yaw = std::atan2(m.entry[1][0], m.entry[0][0]);
        }
        else {
            pitch = 0.0f;
            yaw = std::atan2(-m.entry[0][1], m.entry[1][1]);
        }

        return RE::NiPoint3{
            WrapNegPiToPi(pitch),
            WrapNegPiToPi(roll),
            Wrap0To2Pi(yaw)
        };
    }

    inline float UnwrapAngle(float angleRad, float referenceRad)
    {
        return referenceRad + std::remainder(angleRad - referenceRad, 2.0f * kPiRad);
    }

    inline RE::NiPoint3 MatrixToEulerCont(
        const RE::NiMatrix3& m,
        const RE::NiPoint3& previousEuler)
    {
        const float sinY = std::clamp(-m.entry[0][2], -1.0f, 1.0f);
        float y = std::asin(sinY);
        const float cosY = std::cos(y);

        float x = 0.0f;
        float z = 0.0f;

        if (std::fabs(cosY) > 1e-5f) {
            x = std::atan2(m.entry[1][2], m.entry[2][2]);
            z = std::atan2(m.entry[0][1], m.entry[0][0]);
        }
        else {
            z = previousEuler.z;

            if (sinY >= 0.0f) {
                const float xMinusZ = std::atan2(m.entry[1][0], m.entry[1][1]);
                x = xMinusZ + z;
            }
            else {
                const float xPlusZ = std::atan2(-m.entry[1][0], m.entry[1][1]);
                x = xPlusZ - z;
            }
        }

        auto unwrapCandidate = [&](const RE::NiPoint3& candidate) {
            return RE::NiPoint3{
                UnwrapAngle(candidate.x, previousEuler.x),
                UnwrapAngle(candidate.y, previousEuler.y),
                UnwrapAngle(candidate.z, previousEuler.z)
            };
        };

        const RE::NiPoint3 primary = unwrapCandidate(RE::NiPoint3{ x, y, z });

        const RE::NiPoint3 alternate = unwrapCandidate(RE::NiPoint3{
            x + kPiRad,
            kPiRad - y,
            z + kPiRad
        });

        const auto distanceSquared = [&](const RE::NiPoint3& candidate) {
            const float dx = candidate.x - previousEuler.x;
            const float dy = candidate.y - previousEuler.y;
            const float dz = candidate.z - previousEuler.z;
            return (dx * dx) + (dy * dy) + (dz * dz);
        };

        return distanceSquared(primary) <= distanceSquared(alternate) ? primary : alternate;
    }

    inline RE::NiPoint3 GetSpearEngineEuler(float throwPitchRad, float throwYaw)
    {
        return RE::NiPoint3{
            WrapNegPiToPi(throwPitchRad),
            0.0f,
            Wrap0To2Pi(-throwYaw)
        };
    }

    inline RE::NiMatrix3 MakeSpearMatrix(const RE::NiPoint3& rawThrowDir, float spearRollRad = 0.0f)
    {
        RE::NiPoint3 forward = rawThrowDir;
        if (!NormalizePoint(forward)) {
            forward = RE::NiPoint3{ -1.0f, 0.0f, 0.0f };
        }

        RE::NiPoint3 worldUp{ 0.0f, 0.0f, 1.0f };
        RE::NiPoint3 side = CrossPoint(worldUp, forward);

        if (!NormalizePoint(side)) {
            worldUp = RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
            side = CrossPoint(worldUp, forward);
            if (!NormalizePoint(side)) {
                side = RE::NiPoint3{ 0.0f, -1.0f, 0.0f };
            }
        }

        RE::NiPoint3 up = CrossPoint(forward, side);
        if (!NormalizePoint(up)) {
            up = RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
        }

        if (std::fabs(spearRollRad) > 1e-6f) {
            RE::NiPoint3 rolledSide = RotateAroundAxis(side, forward, spearRollRad);

            if (NormalizePoint(rolledSide)) {
                side = rolledSide;

                up = CrossPoint(forward, side);
                if (!NormalizePoint(up)) {
                    up = worldUp;
                    NormalizePoint(up);
                }
            }
        }

        RE::NiPoint3 xAxis{};
        RE::NiPoint3 yAxis{};
        RE::NiPoint3 zAxis{};

        switch (kSpearTipAxis) {
        case NoReturnSpearTipAxis::LocalPositiveX:
            xAxis = forward;
            yAxis = up;
            zAxis = NegPoint(side);
            break;

        case NoReturnSpearTipAxis::LocalNegativeX:
            xAxis = NegPoint(forward);
            yAxis = up;
            zAxis = side;
            break;

        case NoReturnSpearTipAxis::LocalPositiveY:
            xAxis = NegPoint(side);
            yAxis = forward;
            zAxis = up;
            break;

        case NoReturnSpearTipAxis::LocalNegativeY:
            xAxis = side;
            yAxis = NegPoint(forward);
            zAxis = up;
            break;

        case NoReturnSpearTipAxis::LocalNegativeZ:
            xAxis = side;
            yAxis = NegPoint(up);
            zAxis = NegPoint(forward);
            break;

        case NoReturnSpearTipAxis::LocalPositiveZ:
        default:
            xAxis = side;
            yAxis = up;
            zAxis = forward;
            break;
        }

        NormalizePoint(xAxis);
        NormalizePoint(yAxis);
        NormalizePoint(zAxis);

        RE::NiMatrix3 m{};
        SetMatrixCols(m, xAxis, yAxis, zAxis);
        return m;
    }

    inline RE::NiMatrix3 MakeNoReturnMatrix(
        const RE::NiPoint3& rawThrowDir,
        float throwYaw,
        float spinRad)
    {
        const RE::NiPoint3 worldUp{ 0.0f, 0.0f, 1.0f };

        RE::NiPoint3 forward{
            rawThrowDir.x,
            rawThrowDir.y,
            0.0f
        };
        if (!NormalizePoint(forward)) {
            forward = RE::NiPoint3{
                std::sin(throwYaw),
                std::cos(throwYaw),
                0.0f
            };
            if (!NormalizePoint(forward)) {
                forward = RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
            }
        }

        RE::NiPoint3 tumbleAxis = CrossPoint(worldUp, forward);
        if (!NormalizePoint(tumbleAxis)) {
            tumbleAxis = RE::NiPoint3{ -1.0f, 0.0f, 0.0f };
        }

        const float c = std::cos(spinRad);
        const float s = std::sin(spinRad);
        const RE::NiPoint3 pointAxis = AddPoint(MulPoint(worldUp, c), MulPoint(forward, s));
        const RE::NiPoint3 secondaryAxis = AddPoint(MulPoint(forward, c), MulPoint(worldUp, -s));

        RE::NiMatrix3 matrix{};
        SetMatrixCols(matrix, pointAxis, secondaryAxis, tumbleAxis);
        return matrix;
    }

    inline void RotateMatrix(
        RE::TESObjectREFR* ref,
        const RE::NiMatrix3& matrix,
        const RE::NiPoint3* eulerOverride = nullptr)
    {
        if (!ref) {
            return;
        }

        ref->data.angle = eulerOverride ?
            *eulerOverride :
            MatrixToEulerCont(matrix, ref->data.angle);

        auto applyExactMatrix = [&]() {
            if (auto* node3D = Get3D(ref)) {
                node3D->local.rotate = matrix;
                node3D->world.rotate = matrix;
            }
        };

        applyExactMatrix();
        Refresh3D(ref, "RotateMatrixRaw");
        ForceKeyframedMotion(ref, "RotateMatrixRaw after Refresh3D");
        applyExactMatrix();
    }

    inline bool MakeActorHitSweepFrame(const RE::NiPoint3& from, const RE::NiPoint3& to, RE::NiPoint3& outRight, RE::NiPoint3& outUp)
    {
        RE::NiPoint3 dir{ to.x - from.x, to.y - from.y, to.z - from.z };
        if (!NormalizePoint(dir)) {
            return false;
        }

        RE::NiPoint3 worldUp{ 0.0f, 0.0f, 1.0f };
        outRight = CrossPoint(dir, worldUp);

        if (!NormalizePoint(outRight)) {
            worldUp = RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
            outRight = CrossPoint(dir, worldUp);
            if (!NormalizePoint(outRight)) {
                return false;
            }
        }

        outUp = CrossPoint(outRight, dir);
        return NormalizePoint(outUp);
    }

    inline bool GetSphereHitFraction(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        const RE::NiPoint3& center,
        float radius,
        float& outFraction)
    {
        const RE::NiPoint3 d = SubPoint(to, from);
        const RE::NiPoint3 m = SubPoint(from, center);

        const float a = (d.x * d.x) + (d.y * d.y) + (d.z * d.z);
        if (a <= 1e-6f || radius <= 1e-4f) {
            return false;
        }

        const float b = (m.x * d.x) + (m.y * d.y) + (m.z * d.z);
        const float c = ((m.x * m.x) + (m.y * m.y) + (m.z * m.z)) - (radius * radius);

        if (c <= 0.0f) {
            outFraction = 0.0f;
            return true;
        }

        const float discriminant = (b * b) - (a * c);
        if (discriminant < 0.0f) {
            return false;
        }

        const float t = (-b - std::sqrt(discriminant)) / a;
        if (t < 0.0f || t > 1.0f) {
            return false;
        }

        outFraction = t;
        return true;
    }

    inline bool NeedsAnimBoundsFallback(RE::Actor& actor)
    {
        if (actor.GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
            return true;
        }

        if (actor.GetOccupiedFurniture().get()) {
            return true;
        }

        return actor.GetCharController() == nullptr;
    }

    using AnimatedActorFallbackVisitor =
        std::function<RE::BSContainer::ForEachResult(RE::Actor*)>;

    inline bool ForEachFallbackActor(
        RE::Actor* caster,
        const AnimatedActorFallbackVisitor& visitor);

    inline bool FindAnimBoundsHit(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore,
        RE::Actor** outHitActor,
        RE::TESObjectREFR** outHitRef,
        RE::NiPoint3* outHitPos)
    {
        if (!caster) {
            return false;
        }

        RE::Actor* bestActor = nullptr;
        float bestFraction = 2.0f;

        const bool visitedCandidates =
            ForEachFallbackActor(
                caster,
                [&](RE::Actor* actor) {
                    if (!actor || actor == caster || actor->GetParentCell() != caster->GetParentCell()) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    if (!NeedsAnimBoundsFallback(*actor)) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    auto* actor3D = actor->Get3D(false);
                    if (!actor3D) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    const bool actorRootIgnored = std::any_of(
                        ignore.begin(),
                        ignore.end(),
                        [actor3D](RE::NiAVObject* skip) {
                            return skip && skip == actor3D;
                        });
                    if (actorRootIgnored) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    RE::NiPoint3 center = actor3D->worldBound.center;
                    float radius = actor3D->worldBound.radius;

                    if (!std::isfinite(radius) || radius < 8.0f || radius > 128.0f) {
                        center = actor3D->world.translate;
                        center.z += 48.0f;
                        radius = 52.0f;
                    }

                    radius = std::clamp(radius + (kActorSweepRadius * 0.35f), 20.0f, 112.0f);

                    float fraction = 0.0f;
                    if (!GetSphereHitFraction(from, to, center, radius, fraction) ||
                        fraction >= bestFraction) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    const RE::NiPoint3 candidateHit{
                        from.x + ((to.x - from.x) * fraction),
                        from.y + ((to.y - from.y) * fraction),
                        from.z + ((to.z - from.z) * fraction)
                    };

                    RE::Actor* blockerActor = nullptr;
                    RE::TESObjectREFR* blockerRef = nullptr;
                    RE::NiPoint3 blockerPos{};
                    const bool clearToCandidate = RayIsClear(
                        from,
                        candidateHit,
                        caster,
                        ignore,
                        &blockerActor,
                        &blockerRef,
                        &blockerPos);

                    auto occupiedFurniture = actor->GetOccupiedFurniture().get();
                    auto* occupiedRef = occupiedFurniture ? occupiedFurniture.get() : nullptr;
                    if (!clearToCandidate && blockerActor != actor && blockerRef != occupiedRef) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    bestActor = actor;
                    bestFraction = fraction;
                    return RE::BSContainer::ForEachResult::kContinue;
                });

        if (!visitedCandidates || !bestActor) {
            return false;
        }

        if (outHitActor) {
            *outHitActor = bestActor;
        }
        if (outHitRef) {
            *outHitRef = bestActor;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{
                from.x + ((to.x - from.x) * bestFraction),
                from.y + ((to.y - from.y) * bestFraction),
                from.z + ((to.z - from.z) * bestFraction)
            };
        }

        return true;
    }

    inline bool RayHitActorSwept(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore = {},
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        bool allowAnimFallback = false)
    {
        if (RayHitActor(
            from,
            to,
            caster,
            ignore,
            outHitActor,
            outHitRef,
            outHitPos)) {
            return true;
        }

        if (kActorSweepRadius <= 0.0f) {
            return false;
        }

        RE::NiPoint3 right{};
        RE::NiPoint3 up{};
        if (!MakeActorHitSweepFrame(from, to, right, up)) {
            return false;
        }

        const float r = kActorSweepRadius;
        const float d = r * 0.70710678f;

        const RE::NiPoint3 offsets[] = {
            {  right.x * r,                 right.y * r,                 right.z * r                  },
            { -right.x * r,                -right.y * r,                -right.z * r                  },
            {  up.x * r,                    up.y * r,                    up.z * r                     },
            { -up.x * r,                   -up.y * r,                   -up.z * r                     },
            { (right.x * d) + (up.x * d),   (right.y * d) + (up.y * d),   (right.z * d) + (up.z * d)   },
            { (right.x * d) - (up.x * d),   (right.y * d) - (up.y * d),   (right.z * d) - (up.z * d)   },
            { (-right.x * d) + (up.x * d), (-right.y * d) + (up.y * d), (-right.z * d) + (up.z * d) },
            { (-right.x * d) - (up.x * d), (-right.y * d) - (up.y * d), (-right.z * d) - (up.z * d) }
        };

        for (const auto& offset : offsets) {
            const RE::NiPoint3 sweepFrom{ from.x + offset.x, from.y + offset.y, from.z + offset.z };
            const RE::NiPoint3 sweepTo{ to.x + offset.x, to.y + offset.y, to.z + offset.z };

            RE::Actor* hitActor = nullptr;
            RE::TESObjectREFR* hitRef = nullptr;
            RE::NiPoint3 hitPos{};

            if (RayHitActor(
                sweepFrom,
                sweepTo,
                caster,
                ignore,
                &hitActor,
                &hitRef,
                &hitPos)) {
                if (outHitActor) {
                    *outHitActor = hitActor;
                }
                if (outHitRef) {
                    *outHitRef = hitRef;
                }
                if (outHitPos) {
                    *outHitPos = hitPos;
                }


                return true;
            }
        }

        if (!allowAnimFallback) {
            return false;
        }

        return FindAnimBoundsHit(
            from,
            to,
            caster,
            ignore,
            outHitActor,
            outHitRef,
            outHitPos);
    }

    inline RE::NiPoint3 TransformPoint(const RE::NiTransform& transform, const RE::NiPoint3& local)
    {
        const auto& r = transform.rotate;
        const float s = transform.scale;

        return RE::NiPoint3{
            transform.translate.x + (s * ((r.entry[0][0] * local.x) + (r.entry[0][1] * local.y) + (r.entry[0][2] * local.z))),
            transform.translate.y + (s * ((r.entry[1][0] * local.x) + (r.entry[1][1] * local.y) + (r.entry[1][2] * local.z))),
            transform.translate.z + (s * ((r.entry[2][0] * local.x) + (r.entry[2][1] * local.y) + (r.entry[2][2] * local.z)))
        };
    }

    inline void AddIgnoreNode(std::vector<RE::NiAVObject*>& ignore, RE::NiAVObject* node)
    {
        if (!node) {
            return;
        }

        for (auto* existing : ignore) {
            if (existing == node) {
                return;
            }
        }

        ignore.push_back(node);
    }

    inline std::vector<RE::NiAVObject*> MakePlayerIgnoreList(RE::Actor* player)
    {
        std::vector<RE::NiAVObject*> ignore;
        if (!player) {
            return ignore;
        }

        AddIgnoreNode(ignore, player->Get3D(false));
        AddIgnoreNode(ignore, player->Get3D(true));
        return ignore;
    }

}

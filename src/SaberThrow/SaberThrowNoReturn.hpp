#pragma once

#include "SaberThrow/SaberThrowState.hpp"

namespace SaberThrow
{
    inline float GetFinalClearanceMin()
    {
        const auto settings = ::SaberThrow::Settings::Get();
        return std::max(0.0f, settings.minBounceDist);
    }

    inline float GetFinalClearanceLift(float clearanceMinimum)
    {
        return clearanceMinimum + 15.0f;
    }

    inline constexpr float kClearanceEpsilon = 0.25f;
    inline constexpr std::uint32_t kClearanceMaxPasses = 4;

    inline constexpr float kSideBiasMax = 64.0f;

    inline constexpr float kContactBackoffMin = 8.0f;
    inline constexpr float kContactBackoffMax = 128.0f;

    inline float GetGeomBackoff(float clearanceMinimum)
    {
        return std::clamp(
            clearanceMinimum,
            kContactBackoffMin,
            kContactBackoffMax);
    }

    inline RE::NiPoint3 GetFinalPlayerBias(
        const RE::NiPoint3& fromPos,
        RE::Actor* player,
        float clearanceMinimum)
    {
        if (!player || clearanceMinimum <= 1e-5f) {
            return RE::NiPoint3{};
        }

        const RE::NiPoint3 playerPos = player->GetPosition();
        RE::NiPoint3 playerDir{
            playerPos.x - fromPos.x,
            playerPos.y - fromPos.y,
            0.0f
        };

        if (!NormalizePoint(playerDir)) {
            return RE::NiPoint3{};
        }

        const float biasDistance = std::clamp(
            clearanceMinimum,
            0.0f,
            kSideBiasMax);

        return MulPoint(playerDir, biasDistance);
    }

    inline bool RayFinalGeomHit(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore,
        RE::NiPoint3* outHitPos = nullptr)
    {
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }

        if (!caster) {
            return false;
        }

        auto* cell = caster->GetParentCell();
        if (!cell) {
            return false;
        }

        auto* world = cell->GetbhkWorld();
        if (!world) {
            return false;
        }

        const float hwScale = RE::bhkWorld::GetWorldScale();

        RE::bhkPickData pickData{};
        pickData.rayInput.from = RE::hkVector4(from.x * hwScale, from.y * hwScale, from.z * hwScale, 0.0f);
        pickData.rayInput.to = RE::hkVector4(to.x * hwScale, to.y * hwScale, to.z * hwScale, 0.0f);
        pickData.rayInput.enableShapeCollectionFilter = false;

        RE::CFilter cfilter{};
        caster->GetCollisionFilterInfo(cfilter);
        constexpr std::uint32_t kItemPickerLayer = 40;
        cfilter.SetCollisionLayer(static_cast<RE::COL_LAYER>(kItemPickerLayer));
        pickData.rayInput.filterInfo = cfilter;

        world->PickObject(pickData);

        if (!pickData.rayOutput.HasHit()) {
            return false;
        }

        RE::NiAVObject* hitObj = nullptr;
        {
            auto* collidable = pickData.rayOutput.rootCollidable;
            if (collidable) {
                typedef RE::NiAVObject* (*_GetAV)(const RE::hkpCollidable*);
                static auto getAV = REL::Relocation<_GetAV>(RELOCATION_ID(76160, 77988));
                hitObj = getAV(collidable);
            }
        }

        if (!hitObj) {
            return false;
        }

        for (auto* skip : ignore) {
            if (skip && skip == hitObj) {
                return false;
            }
        }

        RE::TESObjectREFR* hitRef = RE::TESObjectREFR::FindReferenceFor3D(hitObj);
        if (hitRef && hitRef->As<RE::Actor>()) {
            return false;
        }

        const std::uint64_t mask = GetRayMask();
        if (!LayerInMask(hitObj, mask)) {
            return false;
        }

        if (outHitPos) {
            const float f = pickData.rayOutput.hitFraction;
            outHitPos->x = from.x + ((to.x - from.x) * f);
            outHitPos->y = from.y + ((to.y - from.y) * f);
            outHitPos->z = from.z + ((to.z - from.z) * f);
        }

        return true;
    }

    inline RE::NiPoint3 GetFinalClearance(
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& desiredStopPos,
        RE::Actor* hitActor = nullptr,
        bool geometryStop = false)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !ref) {
            return desiredStopPos;
        }

        std::vector<RE::NiAVObject*> ignoreList = MakePlayerIgnoreList(player);
        AddIgnoreNode(ignoreList, ref->Get3D(false));
        AddIgnoreNode(ignoreList, ref->Get3D(true));

        if (hitActor) {
            AddIgnoreNode(ignoreList, hitActor->Get3D(false));
            AddIgnoreNode(ignoreList, hitActor->Get3D(true));
        }

        RE::NiPoint3 candidate = desiredStopPos;

        const float clearanceMin = GetFinalClearanceMin();
        const float clearanceLift =
            GetFinalClearanceLift(clearanceMin);

        constexpr float kDiag = 0.70710678f;
        const RE::NiPoint3 wallDirs[] = {
            {  1.0f,  0.0f, 0.0f },
            { -1.0f,  0.0f, 0.0f },
            {  0.0f,  1.0f, 0.0f },
            {  0.0f, -1.0f, 0.0f },
            {  kDiag,  kDiag, 0.0f },
            {  kDiag, -kDiag, 0.0f },
            { -kDiag,  kDiag, 0.0f },
            { -kDiag, -kDiag, 0.0f }
        };

        const RE::NiPoint3 floorProbeOffsets[] = {
            { 0.0f, 0.0f, 0.0f },
            {  clearanceMin * 0.5f, 0.0f, 0.0f },
            { -clearanceMin * 0.5f, 0.0f, 0.0f },
            { 0.0f,  clearanceMin * 0.5f, 0.0f },
            { 0.0f, -clearanceMin * 0.5f, 0.0f }
        };

        bool sideBiasApplied = false;

        if (geometryStop && !hitActor) {
            const RE::NiPoint3 playerSideBias = GetFinalPlayerBias(
                candidate,
                player,
                clearanceMin);

            if (LengthPoint(playerSideBias) > 1e-4f) {
                candidate.x += playerSideBias.x;
                candidate.y += playerSideBias.y;
                sideBiasApplied = true;
            }
        }

        for (std::uint32_t pass = 0; pass < kClearanceMaxPasses; ++pass) {
            bool adjusted = false;
            bool liftedFromFloor = false;

            bool foundFloor = false;
            float highestFloorZ = -1000000000.0f;

            for (const auto& offset : floorProbeOffsets) {
                const RE::NiPoint3 floorFrom{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z + clearanceLift
                };

                const RE::NiPoint3 floorTo{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z - clearanceMin
                };

                RE::NiPoint3 floorHit{};
                if (RayFinalGeomHit(floorFrom, floorTo, player, ignoreList, &floorHit) && floorHit.z <= candidate.z) {
                    foundFloor = true;
                    highestFloorZ = std::max(highestFloorZ, floorHit.z);
                }
            }

            if (foundFloor) {
                const float floorClearance = candidate.z - highestFloorZ;
                if (floorClearance < clearanceMin) {
                    candidate.z += (clearanceMin - floorClearance) + kClearanceEpsilon;
                    adjusted = true;
                    liftedFromFloor = true;
                }
            }

            if (liftedFromFloor && !sideBiasApplied && !hitActor) {
                const RE::NiPoint3 playerSideBias = GetFinalPlayerBias(
                    candidate,
                    player,
                    clearanceMin);

                if (LengthPoint(playerSideBias) > 1e-4f) {
                    candidate.x += playerSideBias.x;
                    candidate.y += playerSideBias.y;
                    adjusted = true;
                    sideBiasApplied = true;
                }
            }

            const RE::NiPoint3 wallFromBase{
                candidate.x,
                candidate.y,
                candidate.z + clearanceLift
            };

            for (const auto& dir : wallDirs) {
                const RE::NiPoint3 wallTo{
                    wallFromBase.x + (dir.x * clearanceMin),
                    wallFromBase.y + (dir.y * clearanceMin),
                    wallFromBase.z
                };

                RE::NiPoint3 wallHit{};
                if (!RayFinalGeomHit(wallFromBase, wallTo, player, ignoreList, &wallHit)) {
                    continue;
                }

                const float hitDist = LengthPoint(SubPoint(wallHit, wallFromBase));
                if (hitDist < clearanceMin) {
                    const float pushBack = (clearanceMin - hitDist) + kClearanceEpsilon;
                    candidate.x -= dir.x * pushBack;
                    candidate.y -= dir.y * pushBack;
                    adjusted = true;
                }
            }

            if (!adjusted) {
                break;
            }
        }

        return candidate;
    }

}

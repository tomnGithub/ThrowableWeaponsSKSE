#pragma once

#include "SaberThrow/SaberThrowLifecycle.hpp"

namespace SaberThrow
{

    inline constexpr const char* kDismemberNeck = "NPC Neck [Neck]";

    inline constexpr const char* kDismemberNodes[] = {
        "NPC L Calf [LClf]",
        "NPC R Calf [RClf]",
        kDismemberNeck,
        "NPC L Forearm [LLar]",
        "NPC R Forearm [RLar]"
    };

    inline bool g_dismemberLogged{ false };
    inline bool g_dismemberVerLogged{ false };

    inline bool IsFinitePoint(const RE::NiPoint3& p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }

    inline float DistSquaredPoint(const RE::NiPoint3& a, const RE::NiPoint3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return (dx * dx) + (dy * dy) + (dz * dz);
    }

    inline DismemberingFrameworkAPI::DismemberingFrameworkAPI* GetDismemberAPI()
    {
        auto* api = DismemberingFrameworkAPI::g_API;

        if (!api) {
            DismemberingFrameworkAPI::LoadAPI();
            api = DismemberingFrameworkAPI::g_API;
        }

        if (!api) {
            return nullptr;
        }

        const auto version = api->GetVersion();
        if (version != DF_API_VERSION) {
            if (!g_dismemberVerLogged) {
                g_dismemberVerLogged = true;
                SKSE::log::warn(
                    "[SaberThrow] Dismembering Framework API version mismatch. Expected 0x{:06X}, got 0x{:06X}; dismemberment disabled.",
                    static_cast<std::uint32_t>(DF_API_VERSION),
                    static_cast<std::uint32_t>(version));
            }
            return nullptr;
        }

        if (!g_dismemberLogged) {
            g_dismemberLogged = true;
            SKSE::log::info("[SaberThrow] Dismembering Framework API detected. Fatal throw dismemberment enabled.");
        }

        return api;
    }

    inline bool GetActorNodeWorldPos(
        RE::Actor* actor,
        const char* nodeName,
        RE::NiPoint3& outWorldPosition)
    {
        outWorldPosition = RE::NiPoint3{};

        if (!actor || !nodeName || nodeName[0] == '\0') {
            return false;
        }

        if (!actor->Get3D(false) && !actor->Get3D(true)) {
            return false;
        }

        auto* node = actor->GetNodeByName(nodeName);
        if (!node) {
            return false;
        }

        const RE::NiPoint3 pos = node->world.translate;
        if (!IsFinitePoint(pos)) {
            return false;
        }

        outWorldPosition = pos;
        return true;
    }

    inline const char* FindDismemberNode(
        RE::Actor* target,
        const RE::NiPoint3& impactPosition,
        bool allowNeck,
        float maxNodeDistance)
    {
        if (!target || !IsFinitePoint(impactPosition)) {
            return nullptr;
        }

        const char* bestNode = nullptr;
        float bestDistanceSq = 3.402823466e+38F;

        for (const char* nodeName : kDismemberNodes) {
            if (!allowNeck && std::strcmp(nodeName, kDismemberNeck) == 0) {
                continue;
            }

            RE::NiPoint3 nodePos{};
            if (!GetActorNodeWorldPos(target, nodeName, nodePos)) {
                continue;
            }

            const float distSq = DistSquaredPoint(nodePos, impactPosition);
            if (distSq < bestDistanceSq) {
                bestDistanceSq = distSq;
                bestNode = nodeName;
            }
        }

        if (!bestNode) {
            return nullptr;
        }

        const float maxDist = std::clamp(maxNodeDistance, 0.0f, 100000.0f);
        if (maxDist <= 0.0f) {
            return nullptr;
        }

        const float scale = std::clamp(target->GetScale(), 0.25f, 4.0f);
        const float maxDistance = maxDist * scale;
        if (bestDistanceSq > (maxDistance * maxDistance)) {
            SKSE::log::debug(
                "[SaberThrow] Skipping Dismembering Framework call: nearest candidate node '{}' is too far from impact position ({:.2f} > {:.2f}).",
                bestNode,
                std::sqrt(bestDistanceSq),
                maxDistance);
            return nullptr;
        }

        return bestNode;
    }

    inline bool ActorIsDeadOrDying(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        if (ActorDead(actor)) {
            return true;
        }

        return actor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f;
    }

    inline bool SkipDismemberAPI(RE::Actor* actor)
    {
        if (!actor) {
            return true;
        }

        if (actor->IsEssential()) {
            return true;
        }

        auto* actorBase = actor->GetActorBase();

        if (actorBase && actorBase->IsEssential()) {
            return true;
        }

        if (actorBase && actorBase->IsInvulnerable()) {
            return true;
        }

        return false;
    }

    inline bool DismemberKilledActor(
        RE::Actor* target,
        RE::Actor* aggressor,
        RE::TESObjectWEAP* weapon,
        const RE::NiPoint3& impactPosition,
        const RE::HitData* hitData = nullptr,
        bool allowNeck = true)
    {
        if (!target || !aggressor || target == aggressor) {
            return false;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        if (!settings.dismemberOn) {
            return false;
        }

        const float dismemberMaxDist = std::clamp(settings.dismemberMaxDist, 0.0f, 100000.0f);
        if (dismemberMaxDist <= 0.0f) {
            return false;
        }

        if (SkipDismemberAPI(target)) {
            SKSE::log::debug(
                "[SaberThrow] Skipping Dismembering Framework call: actor 0x{:08X} is essential or invulnerable.",
                target->GetFormID());
            return false;
        }

        if (!ActorIsDeadOrDying(target)) {
            return false;
        }

        auto* df = GetDismemberAPI();
        if (!df) {
            return false;
        }

        const char* nearestNodeName = FindDismemberNode(
            target,
            impactPosition,
            allowNeck,
            dismemberMaxDist);
        if (!nearestNodeName) {
            return false;
        }

        const RE::BSFixedString nearestNode(nearestNodeName);
        if (df->IsDismemberedNode(target, nearestNode)) {
            return false;
        }

        DismemberingFrameworkAPI::DismembermentParams params{};
        params.forceExecution = true;
        params.specificNode = nearestNodeName;
        params.ignoreArmorClass = true;
        params.noLimbImpulse = true;
        params.noSoundEffect = false;
        params.noPlayerEffect = false;
        params.hitData = const_cast<RE::HitData*>(hitData);

        df->Dismember(target, nearestNode, aggressor, weapon, &params);

        SKSE::log::debug(
            "[SaberThrow] Dismembering Framework requested node '{}' on dead actor 0x{:08X}.",
            nearestNodeName,
            target->GetFormID());

        return true;
    }


}

#pragma once

#include "SaberThrow/SaberThrowSimulation.hpp"

namespace SaberThrow
{
    inline RE::ExtraDataList* FindActorThrowExtra(
        RE::Actor* actor,
        RE::TESBoundObject* item,
        bool leftHand)
    {
        return FindPlayerThrowExtra(actor, item, leftHand);
    }

    struct PendingNPCAnimationThrowRequest
    {
        std::uint32_t requestID{ 0 };
        std::uint32_t loadShutdownGen{ 0 };
        RE::ActorHandle sourceActorHandle{};
        RE::FormID sourceActorFormID{ 0 };
        RE::FormID weaponFormID{ 0 };
        float totalTravel{ 10.0f };
        float stepDist{ 0.15f };
        float spinRateRadTick{ 0.10f };
        bool returningThrow{ false };
        std::string triggerEventName{ "ThrowWeaponRelease" };
        std::chrono::steady_clock::time_point animRequestTime{};
        std::chrono::steady_clock::time_point expiresAt{};
    };

    inline constexpr const char* kNPCStopCastEvent = "CastStop";
    inline constexpr const char* kNPCStopMoveEvent = "moveStop";
    inline constexpr const char* kNPCThrowEvent = "ThrowWeaponRelease";
    inline constexpr std::uint64_t kNPCLegacyDelayMs = 140;
    inline constexpr std::uint32_t kNPCReqExpiryMs = 1500;


    inline std::atomic_uint32_t g_nextNPCReqID{ 1 };
    inline std::mutex g_npcAnimLock{};
    inline std::vector<PendingNPCAnimationThrowRequest> g_npcAnimRequests{};
    inline std::vector<RE::FormID> g_npcAnimSinkIDs{};

    inline void RunNPCAnimRequest(
        const char* triggerTag,
        PendingNPCAnimationThrowRequest request);

    inline void QueueNPCAnimThrow(
        PendingNPCAnimationThrowRequest request);

    inline void PruneNPCAnimRequests(
        std::chrono::steady_clock::time_point now)
    {
        for (auto it = g_npcAnimRequests.begin();
            it != g_npcAnimRequests.end();) {
            if (it->expiresAt != std::chrono::steady_clock::time_point{} &&
                now >= it->expiresAt) {
                it = g_npcAnimRequests.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    inline bool TakeNPCAnimRequest(
        const RE::BSAnimationGraphEvent* event,
        PendingNPCAnimationThrowRequest& outRequest)
    {
        const char* tag = event ? event->tag.c_str() : nullptr;
        if (!tag || tag[0] == '\0') {
            return false;
        }

        std::scoped_lock lock(g_npcAnimLock);
        PruneNPCAnimRequests(std::chrono::steady_clock::now());

        for (auto it = g_npcAnimRequests.begin();
            it != g_npcAnimRequests.end();) {
            auto sourcePtr = it->sourceActorHandle.get();
            auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;

            if (!sourceActor) {
                it = g_npcAnimRequests.erase(it);
                continue;
            }

            if (event->holder != sourceActor) {
                ++it;
                continue;
            }

            const bool triggerMatches =
                it->triggerEventName == tag ||
                (AnimEventNameEquals(it->triggerEventName, "HitFrame") &&
                    AnimEventNameEquals(tag, "HitFrame"));
            if (!triggerMatches) {
                ++it;
                continue;
            }

            outRequest = *it;
            g_npcAnimRequests.erase(it);

            return true;
        }

        return false;
    }

    inline bool TakeNPCAnimRequestByID(
        std::uint32_t requestID,
        PendingNPCAnimationThrowRequest& outRequest)
    {
        if (requestID == 0) {
            return false;
        }

        std::scoped_lock lock(g_npcAnimLock);
        for (auto it = g_npcAnimRequests.begin();
            it != g_npcAnimRequests.end();
            ++it) {
            if (it->requestID != requestID) {
                continue;
            }

            outRequest = *it;
            g_npcAnimRequests.erase(it);
            return true;
        }

        return false;
    }

    class NPCAnimationThrowEventSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
        {
            PendingNPCAnimationThrowRequest request{};
            if (TakeNPCAnimRequest(event, request)) {
                QueueNPCAnimThrow(request);
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline NPCAnimationThrowEventSink g_npcAnimSink{};

    inline void ClearNPCAnimThrowSinks(const char* reason)
    {
        (void)reason;

        std::vector<RE::FormID> actorFormIDs;
        {
            std::scoped_lock lock(g_npcAnimLock);
            actorFormIDs.swap(g_npcAnimSinkIDs);
        }

        for (const RE::FormID actorFormID : actorFormIDs) {
            auto* rawForm = RE::TESForm::LookupByID(actorFormID);
            auto* actor = rawForm ? rawForm->As<RE::Actor>() : nullptr;
            if (actor) {
                actor->RemoveAnimationGraphEventSink(&g_npcAnimSink);
            }
        }
    }

    inline bool InstallNPCAnimThrowSink(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        const RE::FormID actorFormID = actor->GetFormID();
        {
            std::scoped_lock lock(g_npcAnimLock);
            if (std::find(
                g_npcAnimSinkIDs.begin(),
                g_npcAnimSinkIDs.end(),
                actorFormID) != g_npcAnimSinkIDs.end()) {
                return true;
            }
        }

        const bool added = actor->AddAnimationGraphEventSink(&g_npcAnimSink);
        if (!added) {
            SKSE::log::warn(
                "[SaberThrow] NPC weapon throw could not install animation event sink for actor 0x{:08X}.",
                actorFormID);
            return false;
        }

        {
            std::scoped_lock lock(g_npcAnimLock);
            g_npcAnimSinkIDs.push_back(actorFormID);
        }

        SKSE::log::debug(
            "[SaberThrow] NPC weapon throw installed animation event sink for actor 0x{:08X}.",
            actorFormID);
        return true;
    }

    inline void ClearNPCAnimRequests(const char* reason)
    {
        {
            std::scoped_lock lock(g_npcAnimLock);
            g_npcAnimRequests.clear();
        }

        if (!AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] cleared pending NPC animation throw requests: {}.",
                reason ? reason : "no reason");
        }
    }

    inline bool HasNPCAnimRequest(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        const RE::FormID actorFormID = actor->GetFormID();
        std::scoped_lock lock(g_npcAnimLock);
        PruneNPCAnimRequests(std::chrono::steady_clock::now());
        return std::any_of(
            g_npcAnimRequests.begin(),
            g_npcAnimRequests.end(),
            [actorFormID](const PendingNPCAnimationThrowRequest& request) {
                return request.sourceActorFormID == actorFormID;
            });
    }

    inline RE::NiPointer<RE::TESObjectREFR> DropNPCRHWeapon(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon,
        const RE::NiPoint3& dropPos)
    {
        if (!actor || !weapon) {
            return nullptr;
        }

        auto* equippedExtraList = FindActorThrowExtra(
            actor,
            weapon,
            false);

        RE::NiPoint3 rotation = actor->GetAngle();
        const RE::ObjectRefHandle droppedHandle = actor->RemoveItem(
            weapon,
            1,
            RE::ITEM_REMOVE_REASON::kDropping,
            equippedExtraList,
            nullptr,
            &dropPos,
            &rotation);

        auto droppedPtr = droppedHandle.get();
        auto* droppedRef = droppedPtr ? droppedPtr.get() : nullptr;
        if (!droppedRef) {
            SKSE::log::warn(
                "[SaberThrow] NPC weapon throw failed: RemoveItem(kDropping) did not return a dropped ref for actor 0x{:08X}, weapon 0x{:08X}.",
                actor->GetFormID(),
                weapon->GetFormID());
            return nullptr;
        }

        droppedRef->SetPosition(dropPos);
        BlockThrowActivation(droppedRef, "DropActorRightHandWeaponForNPCThrowMainThread");
        ForceKeyframedMotion(droppedRef, "DropActorRightHandWeaponForNPCThrowMainThread");


        return droppedPtr;
    }

    inline void RunNPCAnimRequest(
        const char* triggerTag,
        PendingNPCAnimationThrowRequest request)
    {
        if (request.loadShutdownGen !=
            g_loadShutdownGen.load(std::memory_order_acquire)) {
            return;
        }

        auto sourcePtr = request.sourceActorHandle.get();
        auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!sourceActor || !player || sourceActor == player || ActorDead(sourceActor)) {
            SKSE::log::debug(
                "[SaberThrow] NPC weapon throw skipped on '{}': source actor is gone, invalid, player, or dead.",
                triggerTag ? triggerTag : "unknown trigger");
            return;
        }


        auto* weapon = GetActorRHEquipWeapon(sourceActor);
        if (!weapon || weapon->GetFormID() != request.weaponFormID) {
            SKSE::log::debug(
                "[SaberThrow] NPC weapon throw skipped on '{}': actor 0x{:08X} no longer has the original right-hand weapon equipped.",
                triggerTag ? triggerTag : "unknown trigger",
                sourceActor->GetFormID());
            return;
        }

        RE::NiPoint3 snapPos{};
        RE::NiPoint3 farPos{};
        if (!GetNPCThrowPoints(sourceActor, request.totalTravel, snapPos, farPos)) {
            SKSE::log::warn(
                "[SaberThrow] NPC weapon throw skipped on '{}': could not build NPC-to-player throw points for actor 0x{:08X}.",
                triggerTag ? triggerTag : "unknown trigger",
                sourceActor->GetFormID());
            return;
        }

        auto droppedRef = DropNPCRHWeapon(sourceActor, weapon, snapPos);
        if (!droppedRef) {
            return;
        }

        const bool noReturnDynamic = !request.returningThrow;
        const std::uint32_t mySession = g_saberSession.load(std::memory_order_acquire);
        StartThrow(
            request.returningThrow ? "ThrowableWeaponsSKSE_NPC_Returning" : "ThrowableWeaponsSKSE_NPC",
            droppedRef,
            kDefaultSnapDist,
            request.totalTravel,
            request.stepDist,
            request.spinRateRadTick,
            noReturnDynamic,
            false,
            false,
            mySession,
            sourceActor,
            true,
            snapPos,
            farPos);

    }

    inline void QueueNPCAnimThrow(
        PendingNPCAnimationThrowRequest request)
    {
        const bool legacyHitDelay =
            AnimEventNameEquals(request.triggerEventName, "HitFrame");

        if (legacyHitDelay) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedDuration = now - request.animRequestTime;
            const std::uint64_t elapsedMs = elapsedDuration > std::chrono::steady_clock::duration::zero() ?
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsedDuration).count()) :
                0;

            if (elapsedMs < kImmediateHitMs) {
                const std::uint64_t waitToAnchorMs = elapsedMs < kHitFrameAnchorMs ?
                    (kHitFrameAnchorMs - elapsedMs) :
                    0;
                const std::uint64_t totalWaitMs = waitToAnchorMs + kNPCLegacyDelayMs;

                SKSE::log::debug(
                    "[SaberThrow] NPC early HitFrame for requestID={} at {} ms; waiting {} ms to the 500 ms anchor plus {} ms NPC delay.",
                    request.requestID,
                    elapsedMs,
                    waitToAnchorMs,
                    kNPCLegacyDelayMs);

                std::thread([request, totalWaitMs]() mutable {
                    std::this_thread::sleep_for(std::chrono::milliseconds(totalWaitMs));

                    auto* taskInterface = SKSE::GetTaskInterface();
                    if (!taskInterface) {
                        SKSE::log::warn(
                            "[SaberThrow] NPC weapon throw requestID={} could not launch after delayed HitFrame: no SKSE task interface.",
                            request.requestID);
                        return;
                    }

                    taskInterface->AddTask([request]() mutable {
                        RunNPCAnimRequest(
                            "NPC HitFrame / legacy 0.14-second delay",
                            request);
                        });
                    }).detach();
                return;
            }

            SKSE::log::debug(
                "[SaberThrow] NPC HitFrame for requestID={} arrived at {} ms; launching immediately without the 0.14-second delay.",
                request.requestID,
                elapsedMs);
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            SKSE::log::warn(
                "[SaberThrow] NPC weapon throw requestID={} could not launch after '{}': no SKSE task interface.",
                request.requestID,
                request.triggerEventName.c_str());
            return;
        }

        taskInterface->AddTask([request]() mutable {
            RunNPCAnimRequest(
                "NPC configured animation release",
                request);
            });
    }

    inline constexpr std::uint32_t kPreflightBatch = 5;
    inline constexpr std::uint32_t kPreflightMaxTicks = 8192;
    inline constexpr float kPreflightReturnZ = 20.0f;

    enum class NPCThrowPreflightSegmentResult : std::uint8_t
    {
        Clear,
        TrajectoryStops,
        DisallowedNPC
    };

    struct NPCThrowPreflightSegmentHit
    {
        NPCThrowPreflightSegmentResult result{ NPCThrowPreflightSegmentResult::Clear };
        RE::Actor* actor{ nullptr };
        RE::NiPoint3 position{};
    };

    inline bool IsBlockedNPC(
        RE::Actor* sourceActor,
        RE::Actor* candidate)
    {
        return candidate &&
            candidate != sourceActor &&
            !ActorDead(candidate) &&
            !IsAllowedNPCThrowActor(candidate);
    }

    inline void MakeNPCIgnoreList(
        RE::Actor* sourceActor,
        std::vector<RE::NiAVObject*>& outIgnoreList)
    {
        outIgnoreList.clear();
        if (!sourceActor) {
            return;
        }

        AddIgnoreNode(outIgnoreList, sourceActor->Get3D(false));
        AddIgnoreNode(outIgnoreList, sourceActor->Get3D(true));
    }

    inline bool RayHitsBlockedNPC(
        RE::Actor* sourceActor,
        const RE::NiPoint3& rayFrom,
        const RE::NiPoint3& rayTo,
        const std::vector<RE::NiAVObject*>& ignoreList,
        RE::Actor** outHitActor,
        RE::NiPoint3* outHitPos)
    {
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }

        RE::NiPoint3 rayDir = SubPoint(rayTo, rayFrom);
        const float totalDistance = LengthPoint(rayDir);
        if (!sourceActor || totalDistance <= 1e-5f || !NormalizePoint(rayDir)) {
            return false;
        }

        RE::NiPoint3 cursor = rayFrom;
        constexpr float kActorAdvance = 8.0f;
        constexpr std::uint32_t kMaxRayActors = 96;

        for (std::uint32_t contact = 0; contact < kMaxRayActors; ++contact) {
            RE::Actor* hitActor = nullptr;
            RE::TESObjectREFR* hitRef = nullptr;
            RE::NiPoint3 hitPos{};
            if (!RayHitActor(
                cursor,
                rayTo,
                sourceActor,
                ignoreList,
                &hitActor,
                &hitRef,
                &hitPos)) {
                return false;
            }

            if (IsBlockedNPC(sourceActor, hitActor)) {
                if (outHitActor) {
                    *outHitActor = hitActor;
                }
                if (outHitPos) {
                    *outHitPos = hitPos;
                }
                return true;
            }

            const float remainingDistance = LengthPoint(SubPoint(rayTo, hitPos));
            if (remainingDistance <= kActorAdvance) {
                return false;
            }

            cursor = AddPoint(hitPos, MulPoint(rayDir, kActorAdvance));
        }

        return false;
    }

    inline bool SweepHitsBlockedNPC(
        RE::Actor* sourceActor,
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        const std::vector<RE::NiAVObject*>& ignoreList,
        RE::Actor** outHitActor,
        RE::NiPoint3* outHitPos)
    {
        if (RayHitsBlockedNPC(
            sourceActor,
            from,
            to,
            ignoreList,
            outHitActor,
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
            {  right.x * r,                  right.y * r,                  right.z * r                  },
            { -right.x * r,                 -right.y * r,                -right.z * r                  },
            {  up.x * r,                     up.y * r,                     up.z * r                     },
            { -up.x * r,                    -up.y * r,                    -up.z * r                     },
            { (right.x * d) + (up.x * d),    (right.y * d) + (up.y * d),    (right.z * d) + (up.z * d)   },
            { (right.x * d) - (up.x * d),    (right.y * d) - (up.y * d),    (right.z * d) - (up.z * d)   },
            { (-right.x * d) + (up.x * d),  (-right.y * d) + (up.y * d),  (-right.z * d) + (up.z * d) },
            { (-right.x * d) - (up.x * d),  (-right.y * d) - (up.y * d),  (-right.z * d) - (up.z * d) }
        };

        for (const auto& offset : offsets) {
            if (RayHitsBlockedNPC(
                sourceActor,
                AddPoint(from, offset),
                AddPoint(to, offset),
                ignoreList,
                outHitActor,
                outHitPos)) {
                return true;
            }
        }

        return false;
    }

    inline NPCThrowPreflightSegmentHit ProbeNPCPath(
        RE::Actor* sourceActor,
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        bool checkGeometry,
        bool actorStopsPath,
        bool useWideGeomSweep = false)
    {
        NPCThrowPreflightSegmentHit result{};
        if (!sourceActor || LengthPoint(SubPoint(to, from)) <= 1e-5f) {
            return result;
        }

        std::vector<RE::NiAVObject*> ignoreList;
        MakeNPCIgnoreList(sourceActor, ignoreList);

        auto probeGeometryRay = [&](const RE::NiPoint3& offset) -> bool {
            RE::Actor* rayHitActor = nullptr;
            RE::TESObjectREFR* rayHitRef = nullptr;
            RE::NiPoint3 rayHitPos{};

            if (RayIsClear(
                AddPoint(from, offset),
                AddPoint(to, offset),
                sourceActor,
                ignoreList,
                &rayHitActor,
                &rayHitRef,
                &rayHitPos)) {
                return false;
            }

            result.actor = rayHitActor;
            result.position = SubPoint(rayHitPos, offset);

            if (IsBlockedNPC(sourceActor, rayHitActor)) {
                result.result = NPCThrowPreflightSegmentResult::DisallowedNPC;
                return true;
            }

            if (!rayHitActor || actorStopsPath) {
                result.result = NPCThrowPreflightSegmentResult::TrajectoryStops;
                return true;
            }

            return false;
            };

        if (checkGeometry) {
            if (probeGeometryRay(RE::NiPoint3{})) {
                return result;
            }

            if (useWideGeomSweep && kGroundSweepRadius > 0.0f) {
                RE::NiPoint3 right{};
                RE::NiPoint3 up{};
                if (MakeActorHitSweepFrame(from, to, right, up)) {
                    const float r = kGroundSweepRadius;
                    const float d = r * 0.70710678f;
                    const RE::NiPoint3 offsets[] = {
                        {  right.x * r,                  right.y * r,                  right.z * r                  },
                        { -right.x * r,                 -right.y * r,                -right.z * r                  },
                        {  up.x * r,                     up.y * r,                     up.z * r                     },
                        { -up.x * r,                    -up.y * r,                    -up.z * r                     },
                        { (right.x * d) + (up.x * d),    (right.y * d) + (up.y * d),    (right.z * d) + (up.z * d)   },
                        { (right.x * d) - (up.x * d),    (right.y * d) - (up.y * d),    (right.z * d) - (up.z * d)   },
                        { (-right.x * d) + (up.x * d),  (-right.y * d) + (up.y * d),  (-right.z * d) + (up.z * d) },
                        { (-right.x * d) - (up.x * d),  (-right.y * d) - (up.y * d),  (-right.z * d) - (up.z * d) },
                        { 0.0f,                          0.0f,                         -r                            },
                        {  right.x * d,                  right.y * d,                 (right.z * d) - d             },
                        { -right.x * d,                 -right.y * d,                (-right.z * d) - d             },
                        {  up.x * d,                     up.y * d,                    (up.z * d) - d                },
                        { -up.x * d,                    -up.y * d,                   (-up.z * d) - d                }
                    };

                    for (const auto& offset : offsets) {
                        if (probeGeometryRay(offset)) {
                            return result;
                        }
                    }
                }
            }
        }

        if (!actorStopsPath) {
            RE::Actor* disallowedActor = nullptr;
            RE::NiPoint3 disallowedHitPos{};
            if (SweepHitsBlockedNPC(
                sourceActor,
                from,
                to,
                ignoreList,
                &disallowedActor,
                &disallowedHitPos)) {
                result.result = NPCThrowPreflightSegmentResult::DisallowedNPC;
                result.actor = disallowedActor;
                result.position = disallowedHitPos;
            }
            return result;
        }

        RE::Actor* sweepHitActor = nullptr;
        RE::TESObjectREFR* sweepHitRef = nullptr;
        RE::NiPoint3 sweepHitPos{};
        if (RayHitActorSwept(
            from,
            to,
            sourceActor,
            ignoreList,
            &sweepHitActor,
            &sweepHitRef,
            &sweepHitPos,
            false)) {
            result.actor = sweepHitActor;
            result.position = sweepHitPos;

            if (IsBlockedNPC(sourceActor, sweepHitActor)) {
                result.result = NPCThrowPreflightSegmentResult::DisallowedNPC;
                return result;
            }

            result.result = NPCThrowPreflightSegmentResult::TrajectoryStops;
        }

        return result;
    }

    inline RE::NiPoint3 GetNPCNoReturnDir(
        const RE::NiPoint3& normalOutwardDir,
        float distanceTraveled,
        float stepDistance,
        float totalDistance)
    {
        const RE::NiPoint3 downDir{ 0.0f, 0.0f, -1.0f };
        const float safeTotalDistance = std::max(totalDistance, 1e-4f);
        const float progress = std::clamp(
            (distanceTraveled + (stepDistance * 0.5f)) / safeTotalDistance,
            0.0f,
            1.0f);

        RE::NiPoint3 outwardDir = normalOutwardDir;
        const float liftEnd = std::clamp(
            kNPCLiftEndFrac,
            1e-4f,
            std::max(kCurveStartFrac, 1e-4f));

        if (progress < liftEnd) {
            RE::NiPoint3 liftedDir = normalOutwardDir;
            liftedDir.z = std::max(
                liftedDir.z,
                kNPCMinLiftZ);

            if (!NormalizePoint(liftedDir)) {
                liftedDir = RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
            }

            const float liftT = SmoothStep01(progress / liftEnd);
            outwardDir = RE::NiPoint3{
                (liftedDir.x * (1.0f - liftT)) + (normalOutwardDir.x * liftT),
                (liftedDir.y * (1.0f - liftT)) + (normalOutwardDir.y * liftT),
                (liftedDir.z * (1.0f - liftT)) + (normalOutwardDir.z * liftT)
            };

            if (!NormalizePoint(outwardDir)) {
                outwardDir = normalOutwardDir;
            }
        }

        const float curveStart = std::clamp(
            kCurveStartFrac,
            0.0f,
            0.999f);
        const float curveT = SmoothStep01(
            (progress - curveStart) /
            std::max(1.0f - curveStart, 1e-4f));

        RE::NiPoint3 blendedDir{
            (outwardDir.x * (1.0f - curveT)) + (downDir.x * curveT),
            (outwardDir.y * (1.0f - curveT)) + (downDir.y * curveT),
            (outwardDir.z * (1.0f - curveT)) + (downDir.z * curveT)
        };

        if (!NormalizePoint(blendedDir)) {
            blendedDir = downDir;
        }

        return blendedDir;
    }

    inline bool MakeNPCPreflightPath(
        RE::Actor* sourceActor,
        RE::TESObjectWEAP* weapon,
        float requestedTravel,
        float requestedStepDist,
        bool returningThrow,
        RE::NiPoint3& outSnapPos,
        RE::NiPoint3& outFarPos,
        float& outStepDist)
    {
        if (!sourceActor || !weapon ||
            !GetNPCThrowPoints(
                sourceActor,
                requestedTravel,
                outSnapPos,
                outFarPos)) {
            return false;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        outSnapPos.z += settings.zOffset;
        outFarPos.z += settings.zOffset;

        const bool noReturnSpearMode =
            !returningThrow && WeaponHasSpearKeyword(weapon, settings);

        if (returningThrow) {
            outSnapPos.z += kPreflightReturnZ;
            outFarPos.z += kPreflightReturnZ;
        }
        else if (noReturnSpearMode && kSpearSnapExtra > 0.0f) {
            RE::NiPoint3 spearSnapDir = SubPoint(outFarPos, outSnapPos);
            if (NormalizePoint(spearSnapDir)) {
                const RE::NiPoint3 spearSnapOffset = MulPoint(
                    spearSnapDir,
                    kSpearSnapExtra);
                outSnapPos = AddPoint(outSnapPos, spearSnapOffset);
                outFarPos = AddPoint(outFarPos, spearSnapOffset);
            }
        }

        const auto multipliers = ApplyNPCThrowMults(
            GetWeaponThrowMults(
                weapon,
                noReturnSpearMode),
            settings);
        const float distMult = std::clamp(
            multipliers.distance,
            0.0f,
            100.0f);

        if (std::fabs(distMult - 1.0f) > 0.000001f) {
            RE::NiPoint3 travelDir = SubPoint(outFarPos, outSnapPos);
            const float baseTravel = LengthPoint(travelDir);
            if (baseTravel > 1e-4f && NormalizePoint(travelDir)) {
                outFarPos = AddPoint(
                    outSnapPos,
                    MulPoint(travelDir, baseTravel * distMult));
            }
        }

        outStepDist =
            requestedStepDist * 10.0f * multipliers.speed * kNPCTickScale;
        return std::isfinite(outStepDist) &&
            std::isfinite(outSnapPos.x) && std::isfinite(outSnapPos.y) && std::isfinite(outSnapPos.z) &&
            std::isfinite(outFarPos.x) && std::isfinite(outFarPos.y) && std::isfinite(outFarPos.z);
    }

    inline bool CheckNPCNoReturnThrow(
        RE::Actor* sourceActor,
        const RE::NiPoint3& snapPos,
        const RE::NiPoint3& farPos,
        float realStepDistance,
        RE::Actor** outBlockingActor)
    {
        if (outBlockingActor) {
            *outBlockingActor = nullptr;
        }

        const float minDownStep =
            kMinDownSpeed * kNPCTickScale;
        const float launchStep = std::max(
            realStepDistance,
            minDownStep);
        const float outboundDist = std::max(
            LengthPoint(SubPoint(farPos, snapPos)),
            1e-4f);
        const float ignoreGeomUntil = outboundDist * std::clamp(
            kNPCIgnoreGeomFrac,
            0.0f,
            1.0f);

        RE::NiPoint3 normalOutwardDir = SubPoint(farPos, snapPos);
        if (!NormalizePoint(normalOutwardDir)) {
            return true;
        }

        RE::NiPoint3 position = snapPos;
        float distanceTraveled = 0.0f;
        bool downwardPhase = false;
        std::uint32_t downwardTicks = 0;
        const std::uint32_t maxSimTicks = static_cast<std::uint32_t>(
            std::ceil(kAutoReturnSec / std::max(kNPCFixedDt, 1e-4f)));

        for (std::uint32_t batchStart = 0;
            batchStart < maxSimTicks;
            batchStart += kPreflightBatch) {
            for (std::uint32_t batchTick = 0;
                batchTick < kPreflightBatch &&
                (batchStart + batchTick) < maxSimTicks;
                ++batchTick) {
                float movementDistance = 0.0f;
                RE::NiPoint3 movementDir{};

                if (!downwardPhase) {
                    const float progress = std::clamp(
                        distanceTraveled / outboundDist,
                        0.0f,
                        1.0f);
                    const float speedFraction = 1.0f -
                        ((1.0f - kEndSpeedFrac) * progress);
                    movementDistance = std::max(
                        launchStep * speedFraction,
                        minDownStep);
                    movementDir = GetNPCNoReturnDir(
                        normalOutwardDir,
                        distanceTraveled,
                        movementDistance,
                        outboundDist);
                }
                else {
                    const float downElapsed =
                        static_cast<float>(downwardTicks) * kNPCFixedDt;
                    const float ramp = std::clamp(
                        downElapsed /
                        std::max(kDownRampSec, 1e-4f),
                        0.0f,
                        1.0f);
                    const float speedFraction = kEndSpeedFrac +
                        ((1.0f - kEndSpeedFrac) * ramp);
                    movementDistance = std::min(
                        launchStep,
                        std::max(
                            launchStep * speedFraction,
                            minDownStep));
                    movementDir = RE::NiPoint3{ 0.0f, 0.0f, -1.0f };
                    ++downwardTicks;
                }

                auto probeAndAdvance = [&](float segmentDistance, bool checkGeometry) -> int {
                    if (segmentDistance <= 1e-5f) {
                        return 0;
                    }

                    const RE::NiPoint3 nextPos = AddPoint(
                        position,
                        MulPoint(movementDir, segmentDistance));
                    const auto probe = ProbeNPCPath(
                        sourceActor,
                        position,
                        nextPos,
                        checkGeometry,
                        true,
                        true);

                    if (probe.result == NPCThrowPreflightSegmentResult::DisallowedNPC) {
                        if (outBlockingActor) {
                            *outBlockingActor = probe.actor;
                        }
                        return -1;
                    }

                    if (probe.result == NPCThrowPreflightSegmentResult::TrajectoryStops) {
                        return 1;
                    }

                    position = nextPos;
                    distanceTraveled += segmentDistance;
                    return 0;
                    };

                if (!downwardPhase && distanceTraveled < ignoreGeomUntil) {
                    const float ignoreDistLeft = std::max(
                        0.0f,
                        ignoreGeomUntil - distanceTraveled);
                    const float ignoredDist = std::min(
                        movementDistance,
                        ignoreDistLeft);

                    const int ignoredResult = probeAndAdvance(
                        ignoredDist,
                        false);
                    if (ignoredResult < 0) {
                        return false;
                    }
                    if (ignoredResult > 0) {
                        return true;
                    }

                    movementDistance -= ignoredDist;
                }

                const int checkedResult = probeAndAdvance(
                    movementDistance,
                    true);
                if (checkedResult < 0) {
                    return false;
                }
                if (checkedResult > 0) {
                    return true;
                }

                if (!downwardPhase && distanceTraveled >= outboundDist) {
                    downwardPhase = true;
                    downwardTicks = 0;
                }
            }
        }

        return true;
    }

    inline bool CheckNPCReturnThrow(
        RE::Actor* sourceActor,
        const RE::NiPoint3& snapPos,
        const RE::NiPoint3& farPos,
        float realStepDistance,
        RE::Actor** outBlockingActor)
    {
        if (outBlockingActor) {
            *outBlockingActor = nullptr;
        }

        const float stepDistance = std::max(realStepDistance, 1e-4f);
        const float outboundDist = std::max(
            LengthPoint(SubPoint(farPos, snapPos)),
            1e-4f);
        const float ignoreGeomUntil = outboundDist * std::clamp(
            kNPCIgnoreGeomFrac,
            0.0f,
            1.0f);
        const bool actorStopsOut =
            ::SaberThrow::Settings::Get().continueThrow != 1;

        RE::NiPoint3 position = snapPos;
        bool returning = false;
        std::uint32_t simulatedTicks = 0;

        while (simulatedTicks < kPreflightMaxTicks) {
            for (std::uint32_t batchTick = 0;
                batchTick < kPreflightBatch &&
                simulatedTicks < kPreflightMaxTicks;
                ++batchTick, ++simulatedTicks) {
                const RE::NiPoint3 target = returning ?
                    GetHandThrowSource(sourceActor, false) :
                    farPos;
                RE::NiPoint3 toTarget = SubPoint(target, position);
                const float targetDistance = LengthPoint(toTarget);
                if (targetDistance <= 1e-4f) {
                    if (returning) {
                        return true;
                    }
                    returning = true;
                    continue;
                }

                if (!NormalizePoint(toTarget)) {
                    return true;
                }

                float movementDistance = std::min(stepDistance, targetDistance);

                auto probeAndAdvance = [&](float segmentDistance, bool checkGeometry) -> int {
                    if (segmentDistance <= 1e-5f) {
                        return 0;
                    }

                    const RE::NiPoint3 nextPos = AddPoint(
                        position,
                        MulPoint(toTarget, segmentDistance));
                    const auto probe = ProbeNPCPath(
                        sourceActor,
                        position,
                        nextPos,
                        checkGeometry,
                        !returning && actorStopsOut);

                    if (probe.result == NPCThrowPreflightSegmentResult::DisallowedNPC) {
                        if (outBlockingActor) {
                            *outBlockingActor = probe.actor;
                        }
                        return -1;
                    }

                    if (probe.result == NPCThrowPreflightSegmentResult::TrajectoryStops) {
                        position = probe.position;
                        if (!returning) {
                            returning = true;
                            return 1;
                        }
                    }

                    position = nextPos;
                    return 0;
                    };

                if (!returning) {
                    const float outboundTraveled = std::clamp(
                        LengthPoint(SubPoint(position, snapPos)),
                        0.0f,
                        outboundDist);

                    if (outboundTraveled < ignoreGeomUntil) {
                        const float ignoreDistLeft = std::max(
                            0.0f,
                            ignoreGeomUntil - outboundTraveled);
                        const float ignoredDist = std::min(
                            movementDistance,
                            ignoreDistLeft);

                        const int ignoredResult = probeAndAdvance(
                            ignoredDist,
                            false);
                        if (ignoredResult < 0) {
                            return false;
                        }
                        if (ignoredResult > 0) {
                            continue;
                        }

                        movementDistance -= ignoredDist;
                    }
                }

                const int checkedResult = probeAndAdvance(
                    movementDistance,
                    !returning);
                if (checkedResult < 0) {
                    return false;
                }
                if (checkedResult > 0) {
                    continue;
                }

                if (!returning &&
                    LengthPoint(SubPoint(farPos, position)) <= 1e-4f) {
                    returning = true;
                }
                else if (returning &&
                    LengthPoint(SubPoint(target, position)) <= 1e-4f) {
                    return true;
                }
            }
        }

        SKSE::log::warn(
            "[SaberThrow] NPC returning throw preflight exceeded {} simulated ticks for actor 0x{:08X}; cancelling conservatively.",
            kPreflightMaxTicks,
            sourceActor ? sourceActor->GetFormID() : 0);
        return false;
    }

    inline bool CheckNPCThrow(
        RE::Actor* sourceActor,
        RE::TESObjectWEAP* weapon,
        float totalTravel,
        float stepDist,
        bool returningThrow,
        RE::Actor** outBlockingActor = nullptr)
    {
        if (outBlockingActor) {
            *outBlockingActor = nullptr;
        }

        RE::NiPoint3 snapPos{};
        RE::NiPoint3 farPos{};
        float realStepDistance = 0.0f;
        if (!MakeNPCPreflightPath(
            sourceActor,
            weapon,
            totalTravel,
            stepDist,
            returningThrow,
            snapPos,
            farPos,
            realStepDistance)) {
            SKSE::log::warn(
                "[SaberThrow] NPC {} throw preflight could not build a valid trajectory for actor 0x{:08X}; cancelling before animation.",
                returningThrow ? "returning" : "no-return",
                sourceActor ? sourceActor->GetFormID() : 0);
            return false;
        }

        if (realStepDistance <= 1e-5f) {
            return true;
        }

        return returningThrow ?
            CheckNPCReturnThrow(
                sourceActor,
                snapPos,
                farPos,
                realStepDistance,
                outBlockingActor) :
            CheckNPCNoReturnThrow(
                sourceActor,
                snapPos,
                farPos,
                realStepDistance,
                outBlockingActor);
    }

    inline bool IsNPCActorCasting(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        bool isCastingRight = false;
        bool isCastingLeft = false;
        bool isCastingDual = false;

        return
            (actor->GetGraphVariableBool("IsCastingRight", isCastingRight) && isCastingRight) ||
            (actor->GetGraphVariableBool("IsCastingLeft", isCastingLeft) && isCastingLeft) ||
            (actor->GetGraphVariableBool("IsCastingDual", isCastingDual) && isCastingDual);
    }

    inline bool QueueNPCThrowAfterAnim(
        RE::Actor* sourceActor,
        RE::TESObjectWEAP* weapon,
        float totalTravel,
        float stepDist,
        float spinRateRadTick,
        bool returningThrow)
    {
        if (!sourceActor || !weapon) {
            return false;
        }

        if (!CheckNPCThrow(
            sourceActor,
            weapon,
            totalTravel,
            stepDist,
            returningThrow,
            nullptr)) {
            return false;
        }

        if (!InstallNPCAnimThrowSink(sourceActor)) {
            return false;
        }

        PendingNPCAnimationThrowRequest request{};
        request.requestID = g_nextNPCReqID.fetch_add(1, std::memory_order_acq_rel);
        if (request.requestID == 0) {
            request.requestID = g_nextNPCReqID.fetch_add(1, std::memory_order_acq_rel);
        }
        request.loadShutdownGen =
            g_loadShutdownGen.load(std::memory_order_acquire);
        request.sourceActorHandle = RE::ActorHandle(sourceActor);
        request.sourceActorFormID = sourceActor->GetFormID();
        request.weaponFormID = weapon->GetFormID();
        request.totalTravel = totalTravel;
        request.stepDist = stepDist;
        request.spinRateRadTick = spinRateRadTick;
        request.returningThrow = returningThrow;

        if (returningThrow) {
            (void)CastNPCSignalSelf(
                sourceActor,
                kNPCReturnStartID);
        }

        if (returningThrow && IsNPCActorCasting(sourceActor)) {
            (void)sourceActor->NotifyAnimationGraph(
                RE::BSFixedString(kNPCStopCastEvent));
        }

        (void)sourceActor->NotifyAnimationGraph(
            RE::BSFixedString(kNPCStopMoveEvent));

        (void)CastNPCSignalSelf(
            sourceActor,
            returningThrow ?
            kReturnSpellID :
            kNoReturnSpellID);

        const auto settings = ::SaberThrow::Settings::Get();
        request.triggerEventName = settings.throwTriggerEvent.empty() ?
            kNPCThrowEvent :
            settings.throwTriggerEvent;

        const auto animRequestTime = std::chrono::steady_clock::now();
        request.animRequestTime = animRequestTime;
        request.expiresAt = animRequestTime +
            std::chrono::milliseconds(kNPCReqExpiryMs);

        {
            std::scoped_lock lock(g_npcAnimLock);
            PruneNPCAnimRequests(animRequestTime);
            g_npcAnimRequests.push_back(request);
        }

        const std::string& throwAnimEvent = returningThrow ?
            settings.npcReturnAnim :
            settings.npcNoReturnAnim;
        const bool sent = sourceActor->NotifyAnimationGraph(
            RE::BSFixedString(throwAnimEvent.c_str()));
        if (!sent) {
            PendingNPCAnimationThrowRequest removed{};
            TakeNPCAnimRequestByID(request.requestID, removed);
            SKSE::log::warn(
                "[SaberThrow] NPC weapon throw skipped: actor 0x{:08X} did not accept animation event '{}'.",
                sourceActor->GetFormID(),
                throwAnimEvent.c_str());
            return false;
        }

        return true;
    }

    inline bool QueueNPCNoReturnThrow(
        RE::Actor* sourceActor,
        RE::TESObjectWEAP* weapon,
        float totalTravel,
        float stepDist,
        float spinRateRadTick)
    {
        return QueueNPCThrowAfterAnim(
            sourceActor,
            weapon,
            totalTravel,
            stepDist,
            spinRateRadTick,
            false);
    }

    inline bool QueueNPCReturnThrow(
        RE::Actor* sourceActor,
        RE::TESObjectWEAP* weapon,
        float totalTravel,
        float stepDist,
        float spinRateRadTick)
    {
        return QueueNPCThrowAfterAnim(
            sourceActor,
            weapon,
            totalTravel,
            stepDist,
            spinRateRadTick,
            true);
    }

    inline bool StartNPCNoReturnRightHandWeaponThrowMainThread(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!sourceActor || !player || sourceActor == player || ActorDead(sourceActor)) {
            return false;
        }


        auto* weapon = GetActorRHEquipWeapon(sourceActor);
        if (!weapon) {
            SKSE::log::debug(
                "[SaberThrow] NPC weapon throw skipped: actor 0x{:08X} has no right-hand weapon.",
                sourceActor->GetFormID());
            return false;
        }

        float finalTravel = totalTravel;
        float resolvedStepDist = stepDist;
        if (finalTravel <= 0.0f || resolvedStepDist <= 0.0f) {
            const auto settings = ::SaberThrow::Settings::Get();
            finalTravel = settings.noReturnDist;
            resolvedStepDist = settings.noReturnSpeed;
        }


        return QueueNPCNoReturnThrow(
            sourceActor,
            weapon,
            finalTravel,
            resolvedStepDist,
            spinRateRadTick);
    }

    inline bool StartNPCReturnThrowNow(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!sourceActor || !player || sourceActor == player || ActorDead(sourceActor)) {
            return false;
        }


        auto* weapon = GetActorRHEquipWeapon(sourceActor);
        if (!weapon) {
            SKSE::log::debug(
                "[SaberThrow] NPC returning weapon throw skipped: actor 0x{:08X} has no right-hand weapon.",
                sourceActor->GetFormID());
            return false;
        }

        float finalTravel = totalTravel;
        float resolvedStepDist = stepDist;
        if (finalTravel <= 0.0f || resolvedStepDist <= 0.0f) {
            const auto settings = ::SaberThrow::Settings::Get();
            finalTravel = settings.throwDist;
            resolvedStepDist = settings.throwSpeed;
        }


        return QueueNPCReturnThrow(
            sourceActor,
            weapon,
            finalTravel,
            resolvedStepDist,
            spinRateRadTick);
    }

    inline bool StartNPCTelekineticRightHandWeaponThrowMainThread(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        return StartNPCReturnThrowNow(
            sourceActor,
            totalTravel,
            stepDist,
            spinRateRadTick);
    }

    inline bool IsNPCNoReturnRightHandWeaponThrowPendingMainThread(RE::Actor* sourceActor)
    {
        return HasNPCAnimRequest(sourceActor);
    }

    inline bool StartNPCNoReturnRightHandWeaponThrow(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        if (!sourceActor) {
            return false;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return false;
        }

        RE::ActorHandle sourceHandle(sourceActor);
        taskInterface->AddTask([sourceHandle, totalTravel, stepDist, spinRateRadTick]() mutable {
            auto sourcePtr = sourceHandle.get();
            auto* source = sourcePtr ? sourcePtr.get() : nullptr;
            if (!source) {
                return;
            }

            StartNPCNoReturnRightHandWeaponThrowMainThread(
                source,
                totalTravel,
                stepDist,
                spinRateRadTick);
            });

        return true;
    }

    inline bool IsNPCReturnThrowPending(RE::Actor* sourceActor)
    {
        return HasNPCAnimRequest(sourceActor);
    }

    inline bool StartNPCReturnThrow(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        if (!sourceActor) {
            return false;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return false;
        }

        RE::ActorHandle sourceHandle(sourceActor);
        taskInterface->AddTask([sourceHandle, totalTravel, stepDist, spinRateRadTick]() mutable {
            auto sourcePtr = sourceHandle.get();
            auto* source = sourcePtr ? sourcePtr.get() : nullptr;
            if (!source) {
                return;
            }

            StartNPCReturnThrowNow(
                source,
                totalTravel,
                stepDist,
                spinRateRadTick);
            });

        return true;
    }

    inline bool IsNPCTelekineticRightHandWeaponThrowPendingMainThread(RE::Actor* sourceActor)
    {
        return IsNPCReturnThrowPending(sourceActor);
    }

    inline bool StartNPCTelekineticRightHandWeaponThrow(
        RE::Actor* sourceActor,
        float totalTravel = -1.0f,
        float stepDist = -1.0f,
        float spinRateRadTick = 0.10f)
    {
        return StartNPCReturnThrow(
            sourceActor,
            totalTravel,
            stepDist,
            spinRateRadTick);
    }

    inline bool HasNPCRecoveryRecords()
    {
        std::scoped_lock lock(g_npcDropLock);
        return !g_npcDropRecords.empty();
    }

    inline std::vector<NPCDroppedWeaponRecoveryRecord> GetNPCRecoveryRecords()
    {
        std::scoped_lock lock(g_npcDropLock);
        return g_npcDropRecords;
    }

    inline bool RemoveNPCRecoveryRecord(std::uint32_t recoveryID)
    {
        if (recoveryID == 0) {
            return false;
        }

        std::scoped_lock lock(g_npcDropLock);
        const auto oldSize = g_npcDropRecords.size();
        g_npcDropRecords.erase(
            std::remove_if(
                g_npcDropRecords.begin(),
                g_npcDropRecords.end(),
                [recoveryID](const NPCDroppedWeaponRecoveryRecord& record) {
                    return record.recoveryID == recoveryID;
                }),
            g_npcDropRecords.end());
        return g_npcDropRecords.size() != oldSize;
    }

    inline void ClearNPCRecoveryRecords(const char* reason)
    {
        (void)reason;
        std::scoped_lock lock(g_npcDropLock);
        g_npcDropRecords.clear();
    }

    inline bool IsNearNPCDrop(
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef)
    {
        if (!sourceActor || !droppedRef || droppedRef->IsDisabled()) {
            return false;
        }

        const RE::NiPoint3 actorPos = sourceActor->GetPosition();
        const RE::NiPoint3 weaponPos = droppedRef->GetPosition();
        const float dx = actorPos.x - weaponPos.x;
        const float dy = actorPos.y - weaponPos.y;
        const float dz = actorPos.z - weaponPos.z;
        const float distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        return distanceSquared <= kNPCDropDistSq;
    }

    inline bool MarkNPCPathSent(
        std::uint32_t recoveryID)
    {
        if (recoveryID == 0) {
            return false;
        }

        std::scoped_lock lock(g_npcDropLock);
        auto existing = std::find_if(
            g_npcDropRecords.begin(),
            g_npcDropRecords.end(),
            [recoveryID](const NPCDroppedWeaponRecoveryRecord& record) {
                return record.recoveryID == recoveryID;
            });

        if (existing == g_npcDropRecords.end()) {
            return false;
        }

        existing->pathRequested = true;
        return true;
    }

    inline bool RequestNPCDropPath(
        const NPCDroppedWeaponRecoveryRecord& record,
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef)
    {
        if (!sourceActor || !droppedRef || ActorDead(sourceActor) || droppedRef->IsDisabled()) {
            return false;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            return false;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            return false;
        }

        const RE::VMHandle handle = handlePolicy->GetHandleForObject(
            RE::FormType::ActorCharacter,
            sourceActor);
        if (handle == handlePolicy->EmptyHandle()) {
            return false;
        }

        auto args = std::unique_ptr<RE::BSScript::IFunctionArguments>(
            RE::MakeFunctionArguments(
                static_cast<RE::TESObjectREFR*>(droppedRef),
                static_cast<float>(kNPCDropRunPct)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        const bool dispatched = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("Actor"),
            RE::BSFixedString("PathToReference"),
            args.get(),
            callback);

        return dispatched;
    }

    inline bool RestoreNPCThrownWeapon(
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef,
        const char* reason)
    {
        (void)reason;
        if (!sourceActor || !droppedRef || ActorDead(sourceActor) || droppedRef->IsDisabled()) {
            return false;
        }

        auto* weapon = GetWeaponBase(droppedRef);
        if (!weapon) {
            return false;
        }

        const RE::FormID actorFormID = sourceActor->GetFormID();
        const RE::FormID droppedRefFormID = droppedRef->GetFormID();
        const RE::FormID weaponFormID = weapon->GetFormID();

        std::int32_t countBefore = 0;
        FindInvItemByID(sourceActor, weaponFormID, &countBefore);

        (void)DispelNPCSignal(
            sourceActor,
            kNPCReturnStartID);
        (void)CastNPCSignalSelf(
            sourceActor,
            kNPCReturnEndID);

        droppedRef->SetActivationBlocked(false);
        (void)droppedRef->ActivateRef(
            sourceActor,
            0,
            weapon,
            1,
            false);

        std::int32_t countAfter = 0;
        auto* recoveredItem = FindInvItemByID(
            sourceActor,
            weaponFormID,
            &countAfter);

        if (!recoveredItem || countAfter <= countBefore) {
            return false;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return true;
        }

        auto* recoveredExtra = FindInvExtraForID(
            sourceActor,
            weaponFormID);
        auto* rightHandSlot = GetEquipSlotThrownHand(recoveredItem, false);

        constexpr bool kQueueEquip = false;
        constexpr bool kForceEquip = true;
        constexpr bool kPlaySounds = false;
        constexpr bool kApplyNow = true;

        equipManager->EquipObject(
            sourceActor,
            recoveredItem,
            recoveredExtra,
            1,
            rightHandSlot,
            kQueueEquip,
            kForceEquip,
            kPlaySounds,
            kApplyNow);

        if (!droppedRef->IsDisabled()) {
            droppedRef->Disable();
        }
        RemovePickupBlock(droppedRef);
        droppedRef->SetDelete(true);

        return true;
    }

    inline bool RecoverNPCDroppedWeapon(
        const NPCDroppedWeaponRecoveryRecord& record,
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef)
    {
        if (!sourceActor || !droppedRef || ActorDead(sourceActor) || droppedRef->IsDisabled()) {
            return false;
        }

        auto* weapon = GetWeaponBase(droppedRef);
        if (!weapon || weapon->GetFormID() != record.weaponFormID) {
            return false;
        }

        std::int32_t countBefore = 0;
        FindInvItemByID(sourceActor, record.weaponFormID, &countBefore);

        droppedRef->SetActivationBlocked(false);
        (void)droppedRef->ActivateRef(
            sourceActor,
            0,
            weapon,
            1,
            false);

        std::int32_t countAfter = 0;
        auto* recoveredItem = FindInvItemByID(
            sourceActor,
            record.weaponFormID,
            &countAfter);

        if (!recoveredItem || countAfter <= countBefore) {
            return false;
        }

        RemoveNPCNoReturnRef(record.droppedRefFormID);

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return true;
        }

        auto* recoveredExtra = FindInvExtraForID(
            sourceActor,
            record.weaponFormID);
        auto* rightHandSlot = GetEquipSlotThrownHand(recoveredItem, false);

        constexpr bool kQueueEquip = false;
        constexpr bool kForceEquip = true;
        constexpr bool kPlaySounds = false;
        constexpr bool kApplyNow = true;

        equipManager->EquipObject(
            sourceActor,
            recoveredItem,
            recoveredExtra,
            1,
            rightHandSlot,
            kQueueEquip,
            kForceEquip,
            kPlaySounds,
            kApplyNow);

        if (!droppedRef->IsDisabled()) {
            droppedRef->Disable();
        }
        RemovePickupBlock(droppedRef);
        droppedRef->SetDelete(true);

        return true;
    }

    inline void TickNPCRecoveryMonitor()
    {
        const auto records = GetNPCRecoveryRecords();
        if (records.empty()) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto currentGeneration =
            g_loadShutdownGen.load(std::memory_order_acquire);

        for (const auto& record : records) {
            if (record.recoveryID == 0) {
                continue;
            }

            if (record.loadShutdownGen != currentGeneration) {
                RemoveNPCRecoveryRecord(record.recoveryID);
                continue;
            }

            if (record.expiresAt != std::chrono::steady_clock::time_point{} &&
                now >= record.expiresAt) {
                RemoveNPCRecoveryRecord(record.recoveryID);
                continue;
            }

            auto sourcePtr = record.sourceActorHandle.get();
            auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;
            auto droppedPtr = record.droppedRefHandle.get();
            auto* droppedRef = droppedPtr ? droppedPtr.get() : nullptr;

            if (!sourceActor || !droppedRef) {
                continue;
            }

            if (ActorDead(sourceActor) || droppedRef->IsDisabled()) {
                RemoveNPCRecoveryRecord(record.recoveryID);
                continue;
            }

            if (!IsNearNPCDrop(sourceActor, droppedRef)) {
                if (!record.pathRequested &&
                    RequestNPCDropPath(record, sourceActor, droppedRef)) {
                    MarkNPCPathSent(record.recoveryID);
                }
                continue;
            }

            if (RecoverNPCDroppedWeapon(record, sourceActor, droppedRef)) {
                RemoveNPCRecoveryRecord(record.recoveryID);
            }
        }
    }

    inline void QueueNPCRecovery()
    {
        bool expected = false;
        if (!g_npcDropQueued.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel)) {
            return;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            g_npcDropQueued.store(false, std::memory_order_release);
            return;
        }

        taskInterface->AddTask([]() {
            g_npcDropQueued.store(false, std::memory_order_release);
            TickNPCRecoveryMonitor();
            });
    }

    inline void StartNPCRecoveryMonitor()
    {
        bool expected = false;
        if (!g_npcDropMonitor.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel)) {
            return;
        }

        std::thread([]() {
            while (HasNPCRecoveryRecords()) {
                QueueNPCRecovery();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(kNPCDropPollMs));
            }

            g_npcDropMonitor.store(false, std::memory_order_release);

            if (HasNPCRecoveryRecords()) {
                StartNPCRecoveryMonitor();
            }
            }).detach();
    }

    inline void AddNPCRecovery(
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef,
        const char* reason)
    {
        (void)reason;
        if (!sourceActor || !droppedRef || sourceActor->IsPlayerRef()) {
            return;
        }

        auto* weapon = GetWeaponBase(droppedRef);
        if (!weapon) {
            return;
        }

        NPCDroppedWeaponRecoveryRecord record{};
        record.recoveryID = g_nextNPCDropID.fetch_add(1, std::memory_order_acq_rel);
        if (record.recoveryID == 0) {
            record.recoveryID = g_nextNPCDropID.fetch_add(1, std::memory_order_acq_rel);
        }
        record.loadShutdownGen =
            g_loadShutdownGen.load(std::memory_order_acquire);
        record.sourceActorHandle = RE::ActorHandle(sourceActor);
        record.sourceActorFormID = sourceActor->GetFormID();
        record.droppedRefHandle = droppedRef->GetHandle();
        record.droppedRefFormID = droppedRef->GetFormID();
        record.weaponFormID = weapon->GetFormID();
        record.pathRequested = false;
        record.expiresAt = std::chrono::steady_clock::now() +
            std::chrono::seconds(kNPCDropMaxSec);

        {
            std::scoped_lock lock(g_npcDropLock);
            auto existing = std::find_if(
                g_npcDropRecords.begin(),
                g_npcDropRecords.end(),
                [&](const NPCDroppedWeaponRecoveryRecord& candidate) {
                    return candidate.droppedRefFormID == record.droppedRefFormID;
                });

            if (existing != g_npcDropRecords.end()) {
                *existing = record;
            }
            else {
                g_npcDropRecords.push_back(record);
            }
        }


        if (!IsNearNPCDrop(sourceActor, droppedRef) &&
            RequestNPCDropPath(record, sourceActor, droppedRef)) {
            MarkNPCPathSent(record.recoveryID);
        }

        StartNPCRecoveryMonitor();
    }


}

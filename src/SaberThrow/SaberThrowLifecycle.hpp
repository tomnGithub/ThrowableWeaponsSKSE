#pragma once

#include "SaberThrow/SaberThrowPlayerInput.hpp"

namespace SaberThrow
{
    inline void SendPapyrusNoReturnStop(
        RE::TESObjectREFR* stoppedRef,
        const char* reason,
        RE::Actor* hitActor = nullptr)
    {
        auto* eventSource = SKSE::GetModCallbackEventSource();
        if (!eventSource) {
            return;
        }

        RE::TESObjectREFR* eventSender = hitActor ? static_cast<RE::TESObjectREFR*>(hitActor) : stoppedRef;

        const std::string payload = g_state.modName + "|" + (reason ? reason : "");

        SKSE::ModCallbackEvent event{
            RE::BSFixedString(kNoReturnStopEvent),
            RE::BSFixedString(payload.c_str()),
            hitActor ? 1.0f : 0.0f,
            eventSender
        };

        eventSource->SendEvent(&event);

    }

    inline void SendPapyrusReturnStop(
        RE::TESObjectREFR* stoppedRef,
        const char* reason,
        RE::Actor* hitActor = nullptr)
    {
        auto* eventSource = SKSE::GetModCallbackEventSource();
        if (!eventSource) {
            return;
        }

        RE::TESObjectREFR* eventSender = hitActor ? static_cast<RE::TESObjectREFR*>(hitActor) : stoppedRef;

        const std::string payload = g_state.modName + "|" + (reason ? reason : "");

        SKSE::ModCallbackEvent event{
            RE::BSFixedString(kReturnStopEvent),
            RE::BSFixedString(payload.c_str()),
            hitActor ? 1.0f : 0.0f,
            eventSender
        };

        eventSource->SendEvent(&event);

    }

    inline void SendPapyrusRestore(const char* reason)
    {
        auto* eventSource = SKSE::GetModCallbackEventSource();
        if (!eventSource) {
            return;
        }

        const std::string payload = g_state.modName + "|" + (reason ? reason : "");

        SKSE::ModCallbackEvent event{
            RE::BSFixedString(kRestoreEvent),
            RE::BSFixedString(payload.c_str()),
            0.0f,
            nullptr
        };

        eventSource->SendEvent(&event);
    }

    inline void ClearState(const char* reason)
    {
        StopLoopSound(reason ? reason : "ClearStateMainThread");

        g_state.phase = Phase::Idle;
        g_state.refHandle = nullptr;
        g_state.sourceActorHandle.reset();
        g_state.npcNoReturn = false;
        g_state.npcReturning = false;
        g_state.skipPickupReg = false;
        g_state.unblockOnStop = false;
        g_state.modName.clear();
        g_state.snapPos = RE::NiPoint3{};
        g_state.farPos = RE::NiPoint3{};
        g_state.appliedAimOffset = 0.0f;
        g_state.totalTravel = 0.0f;
        g_state.throwPitchRad = 0.0f;
        g_state.throwYawRad = 0.0f;
        g_state.visualPitchRad = 0.0f;
        g_state.visualRollRad = 0.0f;
        g_state.baseYawRad = 0.0f;
        g_state.noReturnVelocity = RE::NiPoint3{};
        g_state.launchSpeed = 0.0f;
        g_state.noReturnStartTime = std::chrono::steady_clock::time_point{};
        g_state.downStartTime = std::chrono::steady_clock::time_point{};
        g_state.lastSimTime = std::chrono::steady_clock::time_point{};
        g_state.simAccumSec = 0.0f;
        g_state.parabolaActive = false;
        g_state.travelDist = 0.0f;
        g_state.gravityStartDist = 0.0f;
        g_state.gravityEnabled = false;
        g_state.aimDamping = 0.0f;
        g_state.stopEventSent = false;
        g_state.damagedActorIDs.clear();
        g_state.moveEquipToTemp = false;
        g_state.hasThrowHand = false;
        g_state.throwHandLeft = false;
        g_state.weaponToReequip = nullptr;
        g_state.weaponUnequipped = false;
        g_state.weaponWasLeft = false;
        g_state.throwPoison = ThrowPoisonState{};
        g_state.noReturnDynamic = false;
        g_state.shieldFullDist = false;
        g_state.visualOrientation = ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        g_state.noReturnSpearMode = kDefaultSpearMode;
        g_state.spearFixedMatrix = RE::NiMatrix3{};
        g_state.spearFixedEuler = RE::NiPoint3{};
        g_state.spearEuler = RE::NiPoint3{};
        g_state.spearFixedValid = false;
        g_state.spearEulerValid = false;
        g_state.forceReturn = false;
    }

    inline void DisableDeleteThrownRef(RE::TESObjectREFR* ref, const char* reason)
    {
        if (!ref) {
            return;
        }

        if (!ref->IsDisabled()) {
            ref->Disable();
        }
        RemovePickupBlock(ref);
        ref->SetDelete(true);

        if (!AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] disabled/deleted thrown ref 0x{:08X}: {}",
                ref->GetFormID(),
                reason ? reason : "manual cleanup");
        }
    }

    inline void ClearAnimThrowRequest(const char* reason)
    {
        std::scoped_lock lock(g_animDebugLock);

        if (g_animThrowReq.active) {
            DisableDeleteThrownRef(
                g_animThrowReq.refHandle.get(),
                reason ? reason : "manual cleanup pending animation throw ref");
        }

        g_animThrowReq = PendingAnimationThrowRequest{};
        ClearHitFrameThrow(reason ? reason : "manual cleanup pending animation throw request");
    }

    inline void ShutdownThrowsForLoad(const char* reason)
    {
        const char* cleanupReason = reason ? reason : "load-screen active throw shutdown";

        g_forceReturn.store(false, std::memory_order_release);
        g_saberRunning.store(false, std::memory_order_release);
        g_updateQueued.store(false, std::memory_order_release);
        g_schedulerActive.store(false, std::memory_order_release);
        g_saberSession.fetch_add(1, std::memory_order_acq_rel);
        g_loadShutdownGen.fetch_add(1, std::memory_order_acq_rel);

        ClearAnimThrowRequest(cleanupReason);
        ClearNPCAnimRequests(cleanupReason);
        ClearNPCAnimThrowSinks(cleanupReason);
        ClearNPCRecoveryRecords(cleanupReason);

        for (auto& state : g_throwStates) {
            if (!state) {
                continue;
            }

            g_currentState = state.get();
            StopLoopSound(cleanupReason);

            if (auto refHandle = g_state.refHandle; refHandle) {
                DisableDeleteThrownRef(refHandle.get(), cleanupReason);
            }

            ClearState(cleanupReason);
        }

        g_throwStates.clear();
        g_currentState = &g_fallbackState;
        ClearState(cleanupReason);

        ClearHitFrameThrow(cleanupReason);

        {
            std::scoped_lock poisonLock(g_throwPoisonLock);
            g_throwPoison = ThrowPoisonState{};
        }

    }

    inline void CleanupThrowInv(const char* reason)
    {
        const char* cleanupReason = reason ? reason : "manual cleanup";

        g_forceReturn.store(false, std::memory_order_release);
        g_saberRunning.store(false, std::memory_order_release);
        g_saberSession.fetch_add(1, std::memory_order_acq_rel);

        ClearAnimThrowRequest(cleanupReason);
        ClearNPCAnimRequests(cleanupReason);
        ClearNPCAnimThrowSinks(cleanupReason);
        ClearNPCRecoveryRecords(cleanupReason);

        for (auto& state : g_throwStates) {
            if (!state) {
                continue;
            }

            g_currentState = state.get();
            StopLoopSound(cleanupReason);

            if (auto refHandle = g_state.refHandle; refHandle) {
                DisableDeleteThrownRef(refHandle.get(), cleanupReason);
            }

            ClearState(cleanupReason);
        }

        g_throwStates.clear();
        g_currentState = &g_fallbackState;

        ClearPickupTriggerRefs(cleanupReason);
        ClearNPCNoReturnRefs(cleanupReason);
        ClearPickupBlockRefs();
        ReturnThrowTempItems(cleanupReason);
        ClearThrowInvMemory(cleanupReason);

        SKSE::log::info("[SaberThrow] manual cleanup completed: {}", cleanupReason);
    }

    inline void FinishNoReturnStop(
        RE::TESObjectREFR* ref,
        const char* reason,
        const RE::NiPoint3* stopPos = nullptr,
        RE::Actor* hitActor = nullptr,
        bool geometryStop = false)
    {

        if (ref) {
            const RE::NiPoint3 desiredStopPos = stopPos ? *stopPos : ref->GetPosition();
            const RE::NiPoint3 safeStopPos = GetFinalClearance(
                ref,
                desiredStopPos,
                hitActor,
                geometryStop);
            const RE::NiPoint3 cur = ref->GetPosition();
            const RE::NiPoint3 moveDelta = SubPoint(safeStopPos, cur);

            if (LengthPoint(moveDelta) > 1e-4f) {
                if (g_state.noReturnDynamic && g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike) {
                    MoveAndApplySpearPose(ref, safeStopPos);
                }
                else {
                    MoveRaw(ref, moveDelta.x, moveDelta.y, moveDelta.z);
                }
            }

            if (g_state.noReturnDynamic && g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike) {
                ApplySpearPose(ref);
            }

            ForceDynamicMotion(ref, "FinishNoReturnStopDynamicMainThread final clearance then dynamic");

            AddPickupBlock(ref);

            if (g_state.unblockOnStop) {
                ref->SetActivationBlocked(false);
            }

            if (!g_state.skipPickupReg) {
                AddPickupTriggerRef(ref, reason);
            }

            if (g_state.npcNoReturn) {
                AddNPCNoReturnRef(ref, reason);

                auto sourcePtr = g_state.sourceActorHandle.get();
                auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;
                AddNPCRecovery(sourceActor, ref, reason);
            }
        }

        SendPapyrusNoReturnStop(ref, reason, hitActor);

        ClearState(reason);
    }

    inline void QueueUpdateTask()
    {
        bool expected = false;
        if (!g_updateQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            g_updateQueued.store(false, std::memory_order_release);
            return;
        }

        taskInterface->AddTask([]()
            {
                g_updateQueued.store(false, std::memory_order_release);
                UpdateThrow();
            });
    }

    inline void StartScheduler(std::uint32_t session)
    {
        bool expected = false;
        if (!g_schedulerActive.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        std::thread([session]()
            {
                using namespace std::chrono_literals;

                while (g_saberRunning.load(std::memory_order_acquire) && g_saberSession.load(std::memory_order_acquire) == session) {
                    QueueUpdateTask();

                    std::this_thread::sleep_for(8ms);
                }

                g_schedulerActive.store(false, std::memory_order_release);
            }).detach();
    }

    inline bool StepToward3D(
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& target,
        float stepDist,
        bool& outBlocked,
        bool checkGeometry = true,
        bool actorHitsBlock = true,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr,
        bool checkActorSweep = false)
    {
        outBlocked = false;
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }
        if (outHitDir) {
            *outHitDir = RE::NiPoint3{};
        }

        const RE::NiPoint3 cur = ref->GetPosition();

        const float dx = target.x - cur.x;
        const float dy = target.y - cur.y;
        const float dz = target.z - cur.z;
        const float dist2 = dx * dx + dy * dy + dz * dz;

        if (dist2 < 1e-8f) {
            return true;
        }

        const float dist = std::sqrt(dist2);

        float adx = 0.0f;
        float ady = 0.0f;
        float adz = 0.0f;
        if (dist <= stepDist) {
            adx = dx;
            ady = dy;
            adz = dz;
        }
        else {
            const float scale = stepDist / dist;
            adx = dx * scale;
            ady = dy * scale;
            adz = dz * scale;
        }

        const RE::NiPoint3 nextPos{ cur.x + adx, cur.y + ady, cur.z + adz };

        if (checkGeometry || checkActorSweep) {
            auto* collisionCaster = GetThrowSourceActor();

            std::vector<RE::NiAVObject*> ignoreList;
            if (auto* selfNode = ref->Get3D(false)) {
                ignoreList.push_back(selfNode);
            }
            IgnoreThrowSource(ignoreList);

            auto setHitOutputs = [&](RE::Actor* hitActor, RE::TESObjectREFR* hitRef, const RE::NiPoint3& hitPos)
                {
                    if (outHitActor) {
                        *outHitActor = hitActor;
                    }
                    if (outHitRef) {
                        *outHitRef = hitRef;
                    }
                    if (outHitPos) {
                        *outHitPos = hitPos;
                    }
                    if (outHitDir) {
                        RE::NiPoint3 hitDir{ nextPos.x - cur.x, nextPos.y - cur.y, nextPos.z - cur.z };
                        NormalizePoint(hitDir);
                        *outHitDir = hitDir;
                    }
                };

            if (checkGeometry) {
                RE::Actor* rayHitActor = nullptr;
                RE::TESObjectREFR* rayHitRef = nullptr;
                RE::NiPoint3 hitPos{};
                if (!RayIsClear(cur, nextPos, collisionCaster, ignoreList, &rayHitActor, &rayHitRef, &hitPos)) {
                    if (!IgnoreActorNPCThrow(rayHitActor)) {
                        setHitOutputs(rayHitActor, rayHitRef, hitPos);

                        if (!rayHitActor || actorHitsBlock) {
                            outBlocked = true;
                            return false;
                        }
                    }
                }
            }

            RE::Actor* sweepHitActor = nullptr;
            RE::TESObjectREFR* sweepHitRef = nullptr;
            RE::NiPoint3 hitPos{};
            if (RayHitActorSwept(
                cur,
                nextPos,
                collisionCaster,
                ignoreList,
                &sweepHitActor,
                &sweepHitRef,
                &hitPos,
                IsThrowFromPlayer()) &&
                !IgnoreActorNPCThrow(sweepHitActor)) {
                setHitOutputs(sweepHitActor, sweepHitRef, hitPos);

                if (actorHitsBlock) {
                    outBlocked = true;
                    return false;
                }
            }
        }

        MoveRaw(ref, adx, ady, adz);
        return dist <= stepDist;
    }

    inline bool StepNPCReturnOut(
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& target,
        float stepDist,
        bool& outBlocked,
        bool actorHitsBlock,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr)
    {
        outBlocked = false;

        const float outboundDist = std::max(g_state.totalTravel, 1e-4f);
        const float ignoreGeomUntil = outboundDist * std::clamp(
            kNPCIgnoreGeomFrac,
            0.0f,
            1.0f);

        const float distanceTraveled = std::clamp(
            LengthPoint(SubPoint(ref->GetPosition(), g_state.snapPos)),
            0.0f,
            outboundDist);
        const float ignoreDistLeft = std::max(
            0.0f,
            ignoreGeomUntil - distanceTraveled);

        const float ignoredGeomDist = std::min(stepDist, ignoreDistLeft);
        if (ignoredGeomDist > 1e-5f) {
            bool segmentBlocked = false;
            const bool segmentReached = StepToward3D(
                ref,
                target,
                ignoredGeomDist,
                segmentBlocked,
                false,
                actorHitsBlock,
                outHitActor,
                outHitRef,
                outHitPos,
                outHitDir,
                true);

            outBlocked = segmentBlocked;
            if (segmentBlocked || segmentReached || (outHitActor && *outHitActor)) {
                return segmentReached;
            }
        }

        const float checkedGeomDist = stepDist - ignoredGeomDist;
        if (checkedGeomDist <= 1e-5f) {
            return false;
        }

        return StepToward3D(
            ref,
            target,
            checkedGeomDist,
            outBlocked,
            true,
            actorHitsBlock,
            outHitActor,
            outHitRef,
            outHitPos,
            outHitDir);
    }

    inline void BeginNoReturnDown()
    {
        const float minDownStep = GetThrowMinDownStep();
        g_state.launchSpeed = std::max(g_state.stepDist, minDownStep);
        g_state.travelDist = 0.0f;

        g_state.gravityStartDist = std::max(
            LengthPoint(SubPoint(g_state.farPos, g_state.snapPos)),
            1e-4f);

        g_state.gravityEnabled = false;

        const float startDownSpeed = std::max(
            g_state.launchSpeed * kEndSpeedFrac,
            minDownStep);
        g_state.noReturnVelocity = RE::NiPoint3{ 0.0f, 0.0f, -startDownSpeed };
        g_state.aimDamping = 0.0f;
        g_state.noReturnStartTime = std::chrono::steady_clock::now();
        g_state.downStartTime = std::chrono::steady_clock::time_point{};
        g_state.parabolaActive = true;
    }

    inline void BeginShieldNoReturnDrop()
    {
        const float minDownStep = GetThrowMinDownStep();
        g_state.launchSpeed = std::max(g_state.stepDist, minDownStep);

        const float tickDistanceScale = 1.0f / kTickScale;
        const float smoothDownDist = std::clamp(
            g_state.launchSpeed * 12.0f * tickDistanceScale,
            64.0f,
            256.0f);

        g_state.gravityStartDist = std::max(smoothDownDist, 1e-4f);
        g_state.travelDist =
            g_state.gravityStartDist *
            std::clamp(kCurveStartFrac, 0.0f, 0.999f);

        g_state.gravityEnabled = false;
        g_state.aimDamping = 0.0f;
        g_state.noReturnStartTime = std::chrono::steady_clock::now();
        g_state.downStartTime = std::chrono::steady_clock::time_point{};
        g_state.parabolaActive = true;

        const float startDownSpeed = std::max(
            g_state.launchSpeed * kEndSpeedFrac,
            minDownStep);
        g_state.noReturnVelocity = RE::NiPoint3{ 0.0f, 0.0f, -startDownSpeed };
    }

    inline bool NoReturnDownTimedOut()
    {
        if (!g_state.parabolaActive) {
            return false;
        }

        const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_state.noReturnStartTime).count();
        return elapsed >= kAutoReturnSec;
    }

    inline bool IsAllowedNPCThrowActor(RE::Actor* actor)
    {
        return actor && (actor->IsPlayerRef() || actor->IsPlayerTeammate());
    }

    inline bool IgnoreActorNPCThrow(RE::Actor* actor)
    {
        return IsThrowFromNPC() &&
            actor &&
            !IsAllowedNPCThrowActor(actor);
    }

    inline bool RayClearNoReturnWide(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outStopPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr)
    {
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outStopPos) {
            *outStopPos = RE::NiPoint3{};
        }
        if (outHitDir) {
            RE::NiPoint3 hitDir = SubPoint(to, from);
            NormalizePoint(hitDir);
            *outHitDir = hitDir;
        }

        auto testRay = [&](const RE::NiPoint3& offset) -> bool {
            const RE::NiPoint3 rayFrom = AddPoint(from, offset);
            const RE::NiPoint3 rayTo = AddPoint(to, offset);

            RE::Actor* hitActor = nullptr;
            RE::TESObjectREFR* hitRef = nullptr;
            RE::NiPoint3 hitPos{};

            if (RayIsClear(rayFrom, rayTo, caster, ignore, &hitActor, &hitRef, &hitPos)) {
                return true;
            }

            if (IgnoreActorNPCThrow(hitActor)) {
                return true;
            }

            if (outHitActor) {
                *outHitActor = hitActor;
            }
            if (outHitRef) {
                *outHitRef = hitRef;
            }

            if (outStopPos) {
                RE::NiPoint3 stopPos = SubPoint(hitPos, offset);

                if (!hitActor) {
                    stopPos.z += kGeomStopLift;

                    RE::NiPoint3 backoffDir = SubPoint(from, to);
                    if (NormalizePoint(backoffDir)) {
                        stopPos = AddPoint(
                            stopPos,
                            MulPoint(
                                backoffDir,
                                GetGeomBackoff(GetFinalClearanceMin())));
                    }
                }

                *outStopPos = stopPos;
            }


            return false;
            };

        if (!testRay(RE::NiPoint3{ 0.0f, 0.0f, 0.0f })) {
            return false;
        }

        const float r = caster == RE::PlayerCharacter::GetSingleton() ?
            kGroundSweepRadius * ::SaberThrow::Settings::Get().groundSweepRadiusMult :
            kGroundSweepRadius;
        if (r <= 0.0f) {
            return true;
        }

        RE::NiPoint3 right{};
        RE::NiPoint3 up{};
        if (!MakeActorHitSweepFrame(from, to, right, up)) {
            return true;
        }

        const float d = r * 0.70710678f;

        const RE::NiPoint3 offsets[] = {
            {  right.x * r,                  right.y * r,                  right.z * r                  },
            { -right.x * r,                 -right.y * r,                 -right.z * r                  },
            {  up.x * r,                     up.y * r,                     up.z * r                     },
            { -up.x * r,                    -up.y * r,                    -up.z * r                     },
            { (right.x * d) + (up.x * d),    (right.y * d) + (up.y * d),    (right.z * d) + (up.z * d)   },
            { (right.x * d) - (up.x * d),    (right.y * d) - (up.y * d),    (right.z * d) - (up.z * d)   },
            { (-right.x * d) + (up.x * d),  (-right.y * d) + (up.y * d),  (-right.z * d) + (up.z * d) },
            { (-right.x * d) - (up.x * d),  (-right.y * d) - (up.y * d),  (-right.z * d) - (up.z * d) },

            { 0.0f, 0.0f, -r },
            {  right.x * d,                  right.y * d,                 (right.z * d) - d             },
            { -right.x * d,                 -right.y * d,                (-right.z * d) - d             },
            {  up.x * d,                     up.y * d,                    (up.z * d) - d                },
            { -up.x * d,                    -up.y * d,                   (-up.z * d) - d                }
        };

        for (const auto& offset : offsets) {
            if (!testRay(offset)) {
                return false;
            }
        }

        return true;
    }

    inline float GetNoReturnOutSpeed()
    {
        const float totalDistance = std::max(g_state.gravityStartDist, 1e-4f);
        const float progress = std::clamp(g_state.travelDist / totalDistance, 0.0f, 1.0f);
        const float speedFraction = 1.0f - ((1.0f - kEndSpeedFrac) * progress);

        return std::max(
            g_state.launchSpeed * speedFraction,
            GetThrowMinDownStep());
    }

    inline float GetNoReturnDownSpeed()
    {
        const float minDownStep = GetThrowMinDownStep();
        const float originalSpeed = std::max(g_state.launchSpeed, minDownStep);

        if (g_state.downStartTime == std::chrono::steady_clock::time_point{}) {
            return std::max(originalSpeed * kEndSpeedFrac, minDownStep);
        }

        const auto elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - g_state.downStartTime).count();

        const float ramp = std::clamp(elapsed / std::max(kDownRampSec, 1e-4f), 0.0f, 1.0f);
        const float speedFraction = kEndSpeedFrac + ((1.0f - kEndSpeedFrac) * ramp);

        return std::min(originalSpeed, std::max(originalSpeed * speedFraction, minDownStep));
    }

    inline float SmoothStep01(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return (t * t) * (3.0f - (2.0f * t));
    }

    inline RE::NiPoint3 GetNoReturnOutDir()
    {
        RE::NiPoint3 dir = SubPoint(g_state.farPos, g_state.snapPos);
        if (!NormalizePoint(dir)) {
            dir = RE::NiPoint3{ std::sin(g_state.throwYawRad), std::cos(g_state.throwYawRad), 0.0f };
            if (!NormalizePoint(dir)) {
                dir = RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
            }
        }

        return dir;
    }

    inline RE::NiPoint3 GetNoReturnCurveDir(float stepDistance)
    {
        const RE::NiPoint3 normalOutwardDir = GetNoReturnOutDir();
        const RE::NiPoint3 downDir{ 0.0f, 0.0f, -1.0f };

        const float totalDistance = std::max(g_state.gravityStartDist, 1e-4f);

        const float progress = std::clamp(
            (g_state.travelDist + (stepDistance * 0.5f)) / totalDistance,
            0.0f,
            1.0f);

        RE::NiPoint3 outwardDir = normalOutwardDir;

        if (g_state.npcNoReturn) {
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
        }

        const float curveStart = std::clamp(kCurveStartFrac, 0.0f, 0.999f);
        const float curveT = SmoothStep01((progress - curveStart) / std::max(1.0f - curveStart, 1e-4f));

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

    inline bool IsNoReturnSmoothingDown(const RE::NiPoint3& movementDir)
    {
        if (g_state.gravityEnabled) {
            return true;
        }

        RE::NiPoint3 normalizedMoveDir = movementDir;
        if (!NormalizePoint(normalizedMoveDir)) {
            return false;
        }

        if (normalizedMoveDir.z >= -1e-4f) {
            return false;
        }

        const float totalDistance = std::max(g_state.gravityStartDist, 1e-4f);
        const float curveStart = std::clamp(kCurveStartFrac, 0.0f, 0.999f);
        const float progress = std::clamp(
            g_state.travelDist / totalDistance,
            0.0f,
            1.0f);

        if (progress >= curveStart) {
            return true;
        }

        RE::NiPoint3 outwardDir = GetNoReturnOutDir();
        if (!NormalizePoint(outwardDir)) {
            return false;
        }

        return normalizedMoveDir.z < (outwardDir.z - 1e-4f);
    }

    inline bool StepNoReturnStepAndDist(
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& direction,
        float stepDistance,
        bool& outBlocked,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr)
    {
        outBlocked = false;
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }
        if (outHitDir) {
            *outHitDir = direction;
        }

        if (!ref || stepDistance <= 1e-5f) {
            return ref != nullptr;
        }

        RE::NiPoint3 moveDir = direction;
        if (!NormalizePoint(moveDir)) {
            return true;
        }

        auto moveSegment = [&](float segmentDistance, bool checkGeometry) -> bool {
            if (segmentDistance <= 1e-5f) {
                return true;
            }

            const RE::NiPoint3 cur = ref->GetPosition();
            const RE::NiPoint3 nextPos = AddPoint(cur, MulPoint(moveDir, segmentDistance));

            auto* collisionCaster = GetThrowSourceActor();

            std::vector<RE::NiAVObject*> ignoreList;
            if (auto* selfNode = ref->Get3D(false)) {
                ignoreList.push_back(selfNode);
            }
            IgnoreThrowSource(ignoreList);

            if (checkGeometry) {
                RE::NiPoint3 stopPos{};
                if (!RayClearNoReturnWide(
                    cur,
                    nextPos,
                    collisionCaster,
                    ignoreList,
                    outHitActor,
                    outHitRef,
                    &stopPos,
                    outHitDir)) {
                    if (outHitPos) {
                        *outHitPos = stopPos;
                    }

                    outBlocked = true;
                    return false;
                }
            }

            RE::Actor* sweepActor = nullptr;
            RE::TESObjectREFR* actorSweepHitRef = nullptr;
            RE::NiPoint3 actorSweepHitPos{};
            if (RayHitActorSwept(
                cur,
                nextPos,
                collisionCaster,
                ignoreList,
                &sweepActor,
                &actorSweepHitRef,
                &actorSweepHitPos,
                IsThrowFromPlayer()) &&
                !IgnoreActorNPCThrow(sweepActor)) {
                if (outHitActor) {
                    *outHitActor = sweepActor;
                }
                if (outHitRef) {
                    *outHitRef = actorSweepHitRef;
                }
                if (outHitPos) {
                    *outHitPos = actorSweepHitPos;
                }
                if (outHitDir) {
                    *outHitDir = moveDir;
                }

                outBlocked = true;
                return false;
            }

            MoveRaw(ref, nextPos.x - cur.x, nextPos.y - cur.y, nextPos.z - cur.z);

            const RE::NiPoint3 after = ref->GetPosition();
            g_state.travelDist += LengthPoint(SubPoint(after, cur));
            return true;
            };

        float ignoredGeomDist = 0.0f;
        if (g_state.npcNoReturn && !g_state.gravityEnabled) {
            const float outboundDist = std::max(g_state.gravityStartDist, 1e-4f);
            const float ignoreGeomUntil = outboundDist * std::clamp(
                kNPCIgnoreGeomFrac,
                0.0f,
                1.0f);

            const float ignoreDistLeft = std::max(
                0.0f,
                ignoreGeomUntil - g_state.travelDist);

            ignoredGeomDist = std::min(stepDistance, ignoreDistLeft);
        }

        if (ignoredGeomDist > 1e-5f && !moveSegment(ignoredGeomDist, false)) {
            return false;
        }

        const float checkedGeomDist = stepDistance - ignoredGeomDist;
        if (checkedGeomDist > 1e-5f) {
            return moveSegment(checkedGeomDist, true);
        }

        return true;
    }

    inline bool StepNoReturnDropStep(
        RE::TESObjectREFR* ref,
        float downDistance,
        bool& outBlocked,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr)
    {
        return StepNoReturnStepAndDist(
            ref,
            RE::NiPoint3{ 0.0f, 0.0f, -1.0f },
            downDistance,
            outBlocked,
            outHitActor,
            outHitRef,
            outHitPos,
            outHitDir);
    }

    inline bool StepNoReturnDown(
        RE::TESObjectREFR* ref,
        bool& outBlocked,
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr,
        RE::NiPoint3* outHitDir = nullptr)
    {
        outBlocked = false;
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }
        if (outHitDir) {
            *outHitDir = RE::NiPoint3{};
        }

        if (!ref) {
            return false;
        }

        if (!g_state.parabolaActive) {
            BeginNoReturnDown();
        }

        if (!g_state.gravityEnabled) {
            const float outboundStep = GetNoReturnOutSpeed();
            const RE::NiPoint3 travelDir = GetNoReturnCurveDir(outboundStep);

            StepNoReturnStepAndDist(
                ref,
                travelDir,
                outboundStep,
                outBlocked,
                outHitActor,
                outHitRef,
                outHitPos,
                outHitDir);

            if (outBlocked) {
                return false;
            }

            if (g_state.travelDist < g_state.gravityStartDist) {
                return true;
            }

            g_state.gravityEnabled = true;
            g_state.downStartTime = std::chrono::steady_clock::now();
            const float startDownSpeed = GetNoReturnDownSpeed();
            g_state.noReturnVelocity = RE::NiPoint3{ 0.0f, 0.0f, -startDownSpeed };

            return true;
        }

        const float downStepDistance = GetNoReturnDownSpeed();
        return StepNoReturnDropStep(
            ref,
            downStepDistance,
            outBlocked,
            outHitActor,
            outHitRef,
            outHitPos,
            outHitDir);
    }



}

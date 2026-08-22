#pragma once

#include "SaberThrow/SaberThrowPose.hpp"

namespace SaberThrow
{
    inline void DoSpinTick(RE::TESObjectREFR* ref)
    {
        ++g_state.spinTickCount;

        if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike) {
            g_state.spinZ = Wrap0To2Pi(g_state.spinZ + g_state.spinRateRadTick);
            ApplySpearPose(ref);
            return;
        }

        g_state.spinZ = Wrap0To2Pi(g_state.spinZ + g_state.spinRateRadTick);

        if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::Vertical &&
            !IsThrownShield(ref)) {
            ApplyNoReturnPose(ref);
            return;
        }

        const float visualYaw = Wrap0To2Pi(kFlatBaseYaw + g_state.spinZ);

        MakeThrowEuler(g_state.throwPitchRad, g_state.throwYawRad, g_state.visualPitchRad, g_state.visualRollRad);


        RotateVisualRaw(ref, g_state.visualPitchRad, g_state.visualRollRad, visualYaw, true);
    }

    inline bool UpdateLiveAimCourse()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* sourceActor = GetThrowSourceActor();
        if (!player || !sourceActor) {
            return false;
        }

        RE::NiPoint3 newSnapPos{};
        RE::NiPoint3 newFarPos{};
        TDMLockStatus tdmStatus{};

        const float travelDistance = std::max(g_state.totalTravel, 1e-4f);
        bool gotLivePoints = false;

        if (sourceActor != player) {
            gotLivePoints = GetNPCThrowPoints(
                sourceActor,
                travelDistance,
                newSnapPos,
                newFarPos);
        }
        else {
            gotLivePoints = GetBlinkThrowPoints(
                player,
                g_state.snapForwardDist,
                travelDistance,
                newSnapPos,
                newFarPos,
                &tdmStatus);

            if (!gotLivePoints) {
                gotLivePoints = GetFallbackPoints(
                    player,
                    g_state.snapForwardDist,
                    travelDistance,
                    newSnapPos,
                    newFarPos);
            }
        }

        if (!gotLivePoints) {
            SKSE::log::warn(
                "[SaberThrow] Snap insurance live snap/aim refresh failed on tick {}; keeping previous snapPos/farPos.",
                g_state.snapTickCount);
            return false;
        }

        newSnapPos.z += g_state.snapZOffset;
        newFarPos.z += g_state.snapZOffset;

        if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike &&
            kSpearSnapExtra > 0.0f) {
            RE::NiPoint3 spearSnapDir = SubPoint(newFarPos, newSnapPos);
            if (NormalizePoint(spearSnapDir)) {
                const RE::NiPoint3 spearSnapOffset = MulPoint(
                    spearSnapDir,
                    kSpearSnapExtra);

                newSnapPos = AddPoint(newSnapPos, spearSnapOffset);
                newFarPos = AddPoint(newFarPos, spearSnapOffset);
            }
        }

        const float aimOffset = (sourceActor == player) ?
            GetAimOffsetHeightThrow(
                player,
                tdmStatus.validTarget,
                g_state.noReturnDynamic) :
            0.0f;
        ApplyAimHeight(newFarPos, aimOffset);

        g_state.snapPos = newSnapPos;
        g_state.farPos = newFarPos;
        g_state.appliedAimOffset = aimOffset;
        g_state.throwPitchRad = GetThrowPitchFromDir(g_state.snapPos, g_state.farPos);
        g_state.throwYawRad = GetThrowYawFromDir(g_state.snapPos, g_state.farPos);
        MakeThrowEuler(
            g_state.throwPitchRad,
            g_state.throwYawRad,
            g_state.visualPitchRad,
            g_state.visualRollRad);

        g_state.spearFixedValid = false;

        return true;
    }

    inline bool TickOnce()
    {
        const auto session = g_saberSession.load(std::memory_order_acquire);
        if (!g_saberRunning.load(std::memory_order_acquire) || g_state.session != session) {
            ClearState("stopped or stale session");
            return false;
        }

        auto refHandle = g_state.refHandle;
        if (!refHandle) {
            ClearState("lost refHandle");
            return false;
        }

        auto* ref = refHandle.get();

        RefreshFallbackActorsDue();

        if (g_state.phase != Phase::Snap) {
            EnsureRefEnabled(ref, "TickMainThreadOnce before physics update");
            ForceKeyframedMotion(ref, "TickMainThreadOnce");
        }

        switch (g_state.phase) {
        case Phase::Snap:
        {
            ++g_state.snapTickCount;

            UpdateLiveAimCourse();

            const bool wasDisabled = ref->IsDisabled();
            const RE::NiPoint3 cur = ref->GetPosition();

            g_state.spinZ = 0.0f;

            if (wasDisabled) {
                ref->SetPosition(g_state.snapPos);
            }
            else {
                MoveRaw(ref, g_state.snapPos.x - cur.x, g_state.snapPos.y - cur.y, g_state.snapPos.z - cur.z);
            }

            if (!EnsureRefEnabled(ref, "Phase::Snap enable after snap move")) {
                return false;
            }

            ForceKeyframedMotion(ref, "Phase::Snap after enable");

            if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike) {
                ApplySpearPose(ref);

            }
            else if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::Vertical &&
                !IsThrownShield(ref)) {
                ApplyNoReturnPose(ref);

            }
            else {
                MakeThrowEuler(g_state.throwPitchRad, g_state.throwYawRad, g_state.visualPitchRad, g_state.visualRollRad);
                g_state.baseYawRad = kFlatBaseYaw;
                RotateVisualRaw(ref, g_state.visualPitchRad, g_state.visualRollRad, kFlatBaseYaw, true);

            }

            const std::uint32_t requiredSnapTicks = 4u;
            if (g_state.snapTickCount < requiredSnapTicks) {
                return false;
            }

            if (!EnsureRef3DLoaded(ref, "Phase::Snap waiting for 3D before Out")) {
                return false;
            }

            if (g_state.moveEquipToTemp) {
                g_state.moveEquipToTemp = false;
                const bool movedToTemp =
                    MoveRHItemToTemp(
                        ref,
                        "ThrowEquippedWeapon starting outbound movement");

                ClearHitFrameThrow(
                    movedToTemp ?
                    "equipped item moved to temp container; new throw may start" :
                    "equipped item temp-container transfer attempted/failed; releasing pre-transfer reservation");
            }
            else {
                ClearHitFrameThrow(
                    "throw reached outbound movement without an equipped-item transfer");
            }

            if (g_state.noReturnDynamic) {
                BeginNoReturnDown();
            }

            g_state.phase = Phase::Out;
            return false;
        }

        case Phase::Out:
        {
            ++g_state.outTickCount;

            if (g_state.forceReturn) {
                g_state.forceReturn = false;

                if (g_state.noReturnDynamic) {
                    const RE::NiPoint3 stopPos = ref->GetPosition();

                    if (g_state.modName == "ThrowableWeaponsSKSEFTG") {
                        constexpr float kTeleportSnap = 128.0f;

                        BlinkToThrow(
                            ref,
                            nullptr,
                            kTeleportSnap,
                            &stopPos);
                    }

                    FinishNoReturnStop(
                        ref,
                        "no-return throw stopped on geometry hit",
                        &stopPos,
                        nullptr);

                    return false;
                }

                g_state.phase = Phase::Back;
                return false;
            }

            if (g_state.noReturnDynamic && NoReturnDownTimedOut()) {
                if (g_state.npcNoReturn) {
                    const RE::NiPoint3 stopPos = ref->GetPosition();
                    FinishNoReturnStop(
                        ref,
                        "npc no-return throw timed out",
                        &stopPos,
                        nullptr,
                        true);
                    return false;
                }

                g_state.noReturnDynamic = false;
                g_state.parabolaActive = false;
                g_state.launchSpeed = 0.0f;
                g_state.downStartTime = std::chrono::steady_clock::time_point{};
                g_state.travelDist = 0.0f;
                g_state.gravityStartDist = 0.0f;
                g_state.gravityEnabled = false;
                g_state.aimDamping = 0.0f;
                g_state.phase = Phase::Back;
                return false;
            }

            bool blockedByGeometry = false;
            RE::Actor* hitActor = nullptr;
            RE::TESObjectREFR* hitRef = nullptr;
            RE::NiPoint3 hitPos{};
            RE::NiPoint3 hitDir{};

            bool reached = false;
            const bool shieldOutbound =
                g_state.shieldFullDist && !g_state.noReturnDynamic;

            if (g_state.noReturnDynamic || shieldOutbound) {
                StepNoReturnDown(
                    ref,
                    blockedByGeometry,
                    &hitActor,
                    &hitRef,
                    &hitPos,
                    &hitDir);

                if (shieldOutbound && !blockedByGeometry) {
                    reached = g_state.gravityEnabled;
                }
            }
            else {
                const auto settings = ::SaberThrow::Settings::Get();
                const bool actorHitsBlock = settings.continueThrow != 1;

                reached = g_state.npcReturning ?
                    StepNPCReturnOut(
                        ref,
                        g_state.farPos,
                        g_state.stepDist,
                        blockedByGeometry,
                        actorHitsBlock,
                        &hitActor,
                        &hitRef,
                        &hitPos,
                        &hitDir) :
                    StepToward3D(
                        ref,
                        g_state.farPos,
                        g_state.stepDist,
                        blockedByGeometry,
                        true,
                        actorHitsBlock,
                        &hitActor,
                        &hitRef,
                        &hitPos,
                        &hitDir);
            }

            if (!blockedByGeometry && hitActor) {
                if (RememberDamagedActor(hitActor)) {
                    OnSaberHitActor(hitActor, hitRef, ref, hitPos, hitDir);

                    if (!g_state.noReturnDynamic) {
                        SendPapyrusReturnStop(
                            ref,
                            "return throw stopped on actor hit",
                            hitActor);
                    }
                }
            }

            if (blockedByGeometry) {
                if (hitActor) {
                    if (RememberDamagedActor(hitActor)) {
                        OnSaberHitActor(hitActor, hitRef, ref, hitPos, hitDir);
                    }
                }
                else if (shieldOutbound && IsNoReturnSmoothingDown(hitDir)) {
                    SendPlayerWorldHitEvent(hitRef, ref);
                    PlayThrownGeomHitSound(hitPos, ref);
                    g_state.noReturnDynamic = true;
                    g_state.shieldFullDist = false;
                    g_state.noReturnSpearMode = false;
                }
                else {
                    SendPlayerWorldHitEvent(hitRef, ref);
                    PlayThrownGeomHitSound(hitPos, ref);
                }

                if (g_state.noReturnDynamic) {
                    StopLoopSound(hitActor ? "actor impact" : "geometry impact");

                    DoSpinTick(ref);

                    if (g_state.modName == "ThrowableWeaponsSKSEFTG") {
                        constexpr float kTeleportSnap = 128.0f;

                        BlinkToThrow(
                            ref,
                            &hitDir,
                            kTeleportSnap,
                            &hitPos);
                    }

                    FinishNoReturnStop(
                        ref,
                        hitActor ? "no-return throw stopped on actor hit" : "no-return throw stopped on geometry hit",
                        &hitPos,
                        hitActor,
                        !hitActor);
                    return false;
                }

                SendPapyrusReturnStop(
                    ref,
                    hitActor ? "return throw stopped on actor hit" : "return throw stopped on geometry hit",
                    hitActor);

                g_state.phase = Phase::Back;
                DoSpinTick(ref);
                return false;
            }

            DoSpinTick(ref);

            if (!g_state.noReturnDynamic && reached) {
                if (g_state.shieldFullDist) {
                    g_state.noReturnDynamic = true;
                    g_state.shieldFullDist = false;
                    g_state.noReturnSpearMode = false;
                    return true;
                }

                g_state.phase = Phase::Back;
                return false;
            }

            return true;
        }

        case Phase::Back:
        {
            ++g_state.backTickCount;

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                ClearState("no player during BACK");
                return false;
            }

            RE::Actor* returnActor = player;
            if (g_state.npcReturning) {
                auto sourcePtr = g_state.sourceActorHandle.get();
                auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;
                if (!sourceActor || ActorDead(sourceActor)) {
                    ref->SetActivationBlocked(false);
                    ForceDynamicMotion(
                        ref,
                        "NPC returning throw owner unavailable during BACK");
                    ClearState("NPC returning throw owner unavailable during BACK");
                    return false;
                }

                returnActor = sourceActor;
            }

            RE::NiPoint3 returnPos{};
            if (g_state.npcReturning) {
                returnPos = GetHandThrowSource(returnActor, false);
            }
            else {
                const RE::NiPoint3 p = player->GetPosition();
                const float yaw = player->data.angle.z;
                const float fx = std::sin(yaw);
                const float fy = std::cos(yaw);

                const auto settings = ::SaberThrow::Settings::Get();
                returnPos = RE::NiPoint3{
                    p.x + fx * 1.80f,
                    p.y + fy * 1.80f,
                    p.z + 100.0f + settings.zOffset
                };
            }

            {
                const RE::NiPoint3 cur = ref->GetPosition();
                const float dx = returnPos.x - cur.x;
                const float dy = returnPos.y - cur.y;
                const float dz = returnPos.z - cur.z;
                const float dist2 = (dx * dx) + (dy * dy) + (dz * dz);

                if (dist2 > 1e-8f) {
                    const float dist = std::sqrt(dist2);
                    const float scale = (dist <= g_state.stepDist) ? 1.0f : (g_state.stepDist / dist);

                    const RE::NiPoint3 nextPos{
                        cur.x + (dx * scale),
                        cur.y + (dy * scale),
                        cur.z + (dz * scale)
                    };

                    std::vector<RE::NiAVObject*> ignoreList;
                    if (auto* selfNode = ref->Get3D(false)) {
                        ignoreList.push_back(selfNode);
                    }
                    IgnoreThrowSource(ignoreList);

                    if (!g_state.stopEventSent) {
                        RE::Actor* probeActor = nullptr;
                        RE::TESObjectREFR* geometryProbeRef = nullptr;
                        RE::NiPoint3 probeHit{};

                        if (!RayIsClear(
                            cur,
                            nextPos,
                            returnActor,
                            ignoreList,
                            &probeActor,
                            &geometryProbeRef,
                            &probeHit) &&
                            !probeActor) {
                            g_state.stopEventSent = true;
                            SendPlayerWorldHitEvent(geometryProbeRef, ref);
                            PlayThrownGeomHitSound(probeHit, ref);
                            SendPapyrusReturnStop(
                                ref,
                                "return throw stopped on geometry hit",
                                nullptr);
                        }
                    }

                    RE::Actor* hitActor = nullptr;
                    RE::TESObjectREFR* hitRef = nullptr;
                    RE::NiPoint3 hitPos{};
                    if (RayHitActorSwept(
                        cur,
                        nextPos,
                        returnActor,
                        ignoreList,
                        &hitActor,
                        &hitRef,
                        &hitPos,
                        IsThrowFromPlayer()) &&
                        !IgnoreActorNPCThrow(hitActor)) {
                        RE::NiPoint3 hitDir{
                            nextPos.x - cur.x,
                            nextPos.y - cur.y,
                            nextPos.z - cur.z
                        };
                        NormalizePoint(hitDir);

                        if (RememberDamagedActor(hitActor)) {
                            OnSaberHitActor(hitActor, hitRef, ref, hitPos, hitDir);
                            SendPapyrusReturnStop(
                                ref,
                                "return throw stopped on actor hit",
                                hitActor);
                        }
                    }
                }
            }

            DoSpinTick(ref);

            bool ignored = false;
            if (StepToward3D(ref, returnPos, g_state.stepDist, ignored, false)) {
                if (g_state.npcReturning) {
                    auto sourcePtr = g_state.sourceActorHandle.get();
                    auto* sourceActor = sourcePtr ? sourcePtr.get() : nullptr;

                    if (!RestoreNPCThrownWeapon(
                        sourceActor,
                        ref,
                        "NPC returning throw arrived at owner")) {
                        ref->SetActivationBlocked(false);
                        ForceDynamicMotion(
                            ref,
                            "NPC returning throw arrival pickup failed");
                        AddNPCRecovery(
                            sourceActor,
                            ref,
                            "NPC returning throw arrival pickup failed");
                    }

                    ClearState("NPC returning throw arrived at owner");
                    return false;
                }

                NotifyPlayerAnimGraph(kTelekEndAnim);

                RestoreThrowItem(
                    ref,
                    "returning throw arrived at player");

                if (!ref->IsDisabled()) {
                    ref->Disable();
                }
                RemovePickupBlock(ref);
                ref->SetDelete(true);
                SendPapyrusRestore("arrived at returnPos");
                ClearState("arrived at returnPos");
                return false;
            }

            return true;
        }

        case Phase::Idle:
        default:
            ClearState("idle phase");
            return false;
        }
    }

    inline void UpdateThrow()
    {
        if (!g_saberRunning.load(std::memory_order_acquire)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (g_forceReturn.exchange(false, std::memory_order_acq_rel)) {
            for (auto& state : g_throwStates) {
                if (state) {
                    state->forceReturn = true;
                }
            }
        }

        for (auto it = g_throwStates.begin(); it != g_throwStates.end();) {
            if (!*it) {
                it = g_throwStates.erase(it);
                continue;
            }

            g_currentState = it->get();
            StopLoopSoundIfTimedOut(now);

            const float fixedDeltaSeconds = GetThrowFixedDt();
            const std::uint32_t maxTicksPerUpdate = GetThrowMaxTicks();

            if (g_state.lastSimTime == std::chrono::steady_clock::time_point{}) {
                g_state.lastSimTime = now;
                g_state.simAccumSec += fixedDeltaSeconds;
            }
            else {
                float deltaSeconds = std::chrono::duration<float>(now - g_state.lastSimTime).count();
                g_state.lastSimTime = now;

                deltaSeconds = std::clamp(deltaSeconds, 0.0f, kMaxAccumSec);
                g_state.simAccumSec += deltaSeconds;
            }

            std::uint32_t ticksThisUpdate = 0;
            while (g_state.simAccumSec >= fixedDeltaSeconds &&
                ticksThisUpdate < maxTicksPerUpdate) {
                g_state.simAccumSec -= fixedDeltaSeconds;
                ++ticksThisUpdate;

                if (!TickOnce()) {
                    break;
                }
            }

            if (g_state.phase == Phase::Idle || !g_state.refHandle) {
                it = g_throwStates.erase(it);
                continue;
            }

            ++it;
        }

        g_currentState = &g_fallbackState;

        if (g_throwStates.empty()) {
            g_saberRunning.store(false, std::memory_order_release);
            g_forceReturn.store(false, std::memory_order_release);
        }
    }

    inline void StartThrow(
        std::string modName,
        RE::NiPointer<RE::TESObjectREFR> refHandle,
        float snapForwardDist,
        float totalTravel,
        float stepDist,
        float spinRateRadTick,
        bool noReturnDynamic,
        bool noReturnSpearMode,
        bool moveEquipToTemp,
        std::uint32_t mySession,
        RE::Actor* throwSourceActor = nullptr,
        bool useCachedPoints = false,
        RE::NiPoint3 preSnapPos = RE::NiPoint3{},
        RE::NiPoint3 precomputedFarPos = RE::NiPoint3{},
        bool hasThrowHand = false,
        bool throwWasLeft = false)
    {

        if (g_saberSession.load(std::memory_order_acquire) != mySession) {
            ClearHitFrameThrow("start skipped: stale session before inventory transfer");
            return;
        }

        if (!refHandle) {
            SKSE::log::warn("[SaberThrow] Start skipped: null refHandle.");
            ClearHitFrameThrow("start skipped: null ref before inventory transfer");
            return;
        }

        auto* ref = refHandle.get();

        if ((!throwSourceActor || throwSourceActor->IsPlayerRef()) &&
            ForceBoomerangReturn(
                ref,
                modName,
                noReturnDynamic)) {
            noReturnDynamic = false;
            ApplyThrowTravelSettings(
                modName,
                noReturnDynamic,
                totalTravel,
                stepDist);

            SKSE::log::debug(
                "[SaberThrow] WeaponTypeBoomerang detected in StartSaberThrowMainThread; forcing non-FTG throw to returning behavior: distance={} speed={}.",
                totalTravel,
                stepDist);
        }

        ThrowPoisonState throwPoison = TakeThrowPoison();

        const auto settings = ::SaberThrow::Settings::Get();
        const bool startingNPCThrow =
            throwSourceActor && !throwSourceActor->IsPlayerRef();
        const bool torchNoReturn = IsThrownTorch(ref);
        const bool shieldNoReturn = IsThrownShield(ref);
        const bool movementNoReturn =
            (noReturnDynamic || torchNoReturn) && !shieldNoReturn;

        const bool spearOverride = ThrownHasSpearKeyword(ref);
        const bool spearMode = noReturnSpearMode || spearOverride;

        auto throwOrient = ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        if (shieldNoReturn) {
            throwOrient = ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        }
        else if (torchNoReturn) {
            throwOrient = ::SaberThrow::Settings::ThrowOrientation::Vertical;
        }
        else if (startingNPCThrow) {
            throwOrient = movementNoReturn ?
                ::SaberThrow::Settings::ThrowOrientation::Vertical :
                ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        }
        else {
            throwOrient = movementNoReturn ?
                settings.weaponOrient :
                settings.telekOrient;
        }

        if (ThrownIsBoomerang(ref)) {
            throwOrient = ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        }
        if (spearMode) {
            throwOrient = ::SaberThrow::Settings::ThrowOrientation::SpearLike;
        }

        auto throwMults = GetWeaponThrowMults(
            GetWeaponBase(ref),
            spearMode);

        if (IsThrownShield(ref)) {
            throwMults = GetThrowShieldMults(settings);
        }
        else if (IsThrownTorch(ref)) {
            throwMults = GetThrowTorchMults(settings);
        }

        if (startingNPCThrow) {
            throwMults = ApplyNPCThrowMults(
                throwMults,
                settings);
        }

        if (!ref->IsDisabled()) {
            ForceKeyframedMotion(ref, "StartSaberThrowMainThread initial");
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::warn("[SaberThrow] Start skipped: no player.");
            ClearHitFrameThrow("start skipped: no player before inventory transfer");
            return;
        }

        RE::NiPoint3 snapPos{};
        RE::NiPoint3 farPos{};
        TDMLockStatus tdmLockStatus{};
        if (useCachedPoints) {
            snapPos = preSnapPos;
            farPos = precomputedFarPos;
        }
        else if (!GetBlinkThrowPoints(player, snapForwardDist, totalTravel, snapPos, farPos, &tdmLockStatus)) {
            if (!GetFallbackPoints(player, snapForwardDist, totalTravel, snapPos, farPos)) {
                SKSE::log::warn("[SaberThrow] Start skipped: could not build throw points.");
                ClearHitFrameThrow("start skipped: could not build throw points before inventory transfer");
                return;
            }
        }

        const float throwZOffset = settings.zOffset;
        float snapZOffset = throwZOffset;

        snapPos.z += throwZOffset;
        farPos.z += throwZOffset;

        constexpr float kReturnZOffset = 20.0f;
        if (!movementNoReturn) {
            snapZOffset += kReturnZOffset;
            snapPos.z += kReturnZOffset;
            farPos.z += kReturnZOffset;

            SKSE::log::debug(
                "[SaberThrow] Applied returning throw extra Z offset: +{}",
                kReturnZOffset);
        }

        if (throwOrient == ::SaberThrow::Settings::ThrowOrientation::SpearLike &&
            kSpearSnapExtra > 0.0f) {
            RE::NiPoint3 spearSnapDir = SubPoint(farPos, snapPos);
            if (NormalizePoint(spearSnapDir)) {
                const RE::NiPoint3 spearSnapOffset = MulPoint(
                    spearSnapDir,
                    kSpearSnapExtra);

                snapPos = AddPoint(snapPos, spearSnapOffset);
                farPos = AddPoint(farPos, spearSnapOffset);
            }
        }

        const float distMult = std::clamp(throwMults.distance, 0.0f, 100.0f);
        if (std::fabs(distMult - 1.0f) > 0.000001f) {
            RE::NiPoint3 travelDir = SubPoint(farPos, snapPos);
            const float baseTravel = LengthPoint(travelDir);
            if (baseTravel > 1e-4f && NormalizePoint(travelDir)) {
                farPos = AddPoint(snapPos, MulPoint(travelDir, baseTravel * distMult));
            }
        }

        const float rawTravel = std::max(LengthPoint(SubPoint(farPos, snapPos)), 1e-4f);

        const float aimOffset = startingNPCThrow ?
            0.0f :
            GetAimOffsetHeightThrow(
                player,
                tdmLockStatus.validTarget,
                movementNoReturn);
        ApplyAimHeight(farPos, aimOffset);

        auto newThrowState = std::make_unique<State>();
        g_currentState = newThrowState.get();
        g_throwStates.push_back(std::move(newThrowState));

        g_state.modName = modName;
        g_state.refHandle = refHandle;
        if (throwSourceActor) {
            g_state.sourceActorHandle = RE::ActorHandle(throwSourceActor);
            const bool npcThrow = !throwSourceActor->IsPlayerRef();
            g_state.npcNoReturn = npcThrow && movementNoReturn;
            g_state.npcReturning = npcThrow && !movementNoReturn;
            g_state.skipPickupReg = g_state.npcNoReturn;
            g_state.unblockOnStop = g_state.npcNoReturn;
        }
        g_state.snapPos = snapPos;
        g_state.farPos = farPos;
        g_state.appliedAimOffset = aimOffset;
        g_state.snapForwardDist = snapForwardDist;
        g_state.snapZOffset = snapZOffset;
        g_state.totalTravel = rawTravel;
        const float perTickScale = kTickScale;
        const float rotMult = startingNPCThrow ?
            std::clamp(settings.npcRotMult, 0.0f, 100.0f) :
            1.0f;
        g_state.stepDist = stepDist * 10.0f * throwMults.speed * perTickScale;
        g_state.spinRateRadTick =
            spinRateRadTick * 10.0f * perTickScale * rotMult;
        g_state.spinZ = 0.0f;
        g_state.throwPitchRad = GetThrowPitchFromDir(snapPos, farPos);
        g_state.throwYawRad = GetThrowYawFromDir(snapPos, farPos);
        MakeThrowEuler(g_state.throwPitchRad, g_state.throwYawRad, g_state.visualPitchRad, g_state.visualRollRad);


        g_state.session = mySession;
        g_state.snapTickCount = 0;
        g_state.spinTickCount = 0;
        g_state.outTickCount = 0;
        g_state.backTickCount = 0;
        g_state.stopEventSent = false;
        g_state.noReturnVelocity = RE::NiPoint3{};
        g_state.launchSpeed = 0.0f;
        g_state.noReturnStartTime = std::chrono::steady_clock::time_point{};
        g_state.downStartTime = std::chrono::steady_clock::time_point{};

        g_state.lastSimTime = std::chrono::steady_clock::now();
        g_state.simAccumSec = GetThrowFixedDt();

        g_state.parabolaActive = false;
        g_state.travelDist = 0.0f;
        g_state.gravityStartDist = 0.0f;
        g_state.gravityEnabled = false;
        g_state.aimDamping = 0.0f;
        g_state.damagedActorIDs.clear();
        g_state.animFallbackActors.clear();
        g_state.animFallbackStage = 0;
        g_state.moveEquipToTemp =
            moveEquipToTemp;
        g_state.hasThrowHand = hasThrowHand;
        g_state.throwHandLeft = throwWasLeft;
        g_state.throwPoison = throwPoison;
        g_state.noReturnDynamic = movementNoReturn;
        g_state.shieldFullDist = shieldNoReturn;
        g_state.visualOrientation = throwOrient;
        g_state.noReturnSpearMode = spearMode;
        g_state.spearFixedValid = false;
        g_state.forceReturn = false;

        if (g_state.visualOrientation == ::SaberThrow::Settings::ThrowOrientation::SpearLike) {
            CacheSpearFixedPose();
        }

        if (IsThrowFromPlayer()) {
            NotifyPlayerAnimGraph(kWeaponThrowAnim);
        }
        StartLoopSound(ref);

        g_state.phase = Phase::Snap;

        g_saberRunning.store(true, std::memory_order_release);

        QueueUpdateTask();
        StartScheduler(mySession);
    }



}

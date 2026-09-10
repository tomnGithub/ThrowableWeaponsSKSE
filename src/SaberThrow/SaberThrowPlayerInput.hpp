#pragma once

#include "SaberThrow/SaberThrowEquipment.hpp"

#include "SKSE/InputMap.h"

namespace SaberThrow
{
    inline constexpr float kDefaultSnapDist = 1.8f;

    inline void Throw(
        const char* modName,
        RE::TESObjectREFR* ref,
        float totalTravel,
        float stepDist,
        float spinRateRadTick,
        bool noReturnDynamic,
        bool noReturnSpearMode,
        bool moveEquipToTemp,
        bool hasThrowHand,
        bool throwWasLeft);

    inline void ThrowWithPrecomputedPoints(
        const char* modName,
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& snapPos,
        const RE::NiPoint3& farPos,
        float totalTravel,
        float stepDist,
        float spinRateRadTick,
        bool noReturnDynamic,
        bool noReturnSpearMode,
        bool moveEquipToTemp,
        bool hasThrowHand,
        bool throwWasLeft);

    inline bool WeaponIsBoomerang(RE::TESObjectWEAP* weapon);

    inline bool WeaponHasSpearKeyword(
        RE::TESObjectWEAP* weapon,
        const ::SaberThrow::Settings::Values& settings);

    inline bool ForceBoomerangReturn(
        RE::TESObjectREFR* thrownRef,
        const std::string& modName,
        bool noReturnDynamic);

    inline void SendThrowStartPayload(
        RE::TESObjectREFR* thrownRef,
        const std::string& payload,
        float numericArg)
    {
        if (!thrownRef) {
            SKSE::log::warn("[SaberThrow] Start-flying ModEvent skipped: null thrown ref.");
            return;
        }

        auto* eventSource = SKSE::GetModCallbackEventSource();
        if (!eventSource) {
            SKSE::log::warn("[SaberThrow] Start-flying ModEvent skipped: no ModCallbackEventSource.");
            return;
        }

        SKSE::ModCallbackEvent event{
            RE::BSFixedString(kThrowStartEvent),
            RE::BSFixedString(payload.c_str()),
            numericArg,
            thrownRef
        };

        eventSource->SendEvent(&event);
    }

    inline void SendThrowStartEvent(
        RE::TESObjectREFR* thrownRef,
        const char* modName,
        const char* triggerTag,
        bool noReturnDynamic)
    {
        const std::string payload =
            std::string(modName ? modName : "") + "|" +
            std::string(triggerTag ? triggerTag : "");

        SendThrowStartPayload(
            thrownRef,
            payload,
            noReturnDynamic ? 1.0f : 0.0f);
    }

    inline constexpr std::uint32_t kAnimDebugMs = 1000;

    inline constexpr std::uint64_t kImmediateHitMs = 460;
    inline constexpr std::uint64_t kHitFrameAnchorMs = 500;
    inline constexpr std::uint64_t kThirdHitDelayMs = 140;

    struct PendingAnimationThrowRequest
    {
        bool active{ false };
        std::uint32_t session{ 0 };
        std::uint32_t loadShutdownGen{ 0 };
        std::string triggerEventName{};
        std::string modName{};
        RE::NiPointer<RE::TESObjectREFR> refHandle{};
        float totalTravel{ 10.0f };
        float stepDist{ 0.15f };
        float spinRateRadTick{ 0.10f };
        bool noReturnDynamic{ false };
        std::string animEvent{};
        std::chrono::steady_clock::time_point animRequestTime{};
        bool legacyHitDelay{ false };
        std::uint64_t hitDelayMs{ 0 };
        bool hasThrowHand{ false };
        bool throwWasLeft{ false };
        bool cachePoison{ false };
        bool drainOnAnim{ false };
        float animStaminaCost{ 0.0f };
        std::string staminaReason{};
        bool hasThrowPoints{ false };
        RE::NiPoint3 cachedSnapPos{};
        RE::NiPoint3 cachedFarPos{};
        std::uint32_t attempt{ 1 };
        std::uint32_t maxAttempts{ 2 };
    };

    inline bool DrainPendingStam(
        const PendingAnimationThrowRequest& request);

    inline std::atomic_bool g_animDebug{ false };
    inline std::atomic_bool g_playerAnimReady{ false };
    inline std::atomic_uint32_t g_animSession{ 0 };
    inline std::mutex g_animDebugLock{};
    inline std::string g_animDebugFilter{};
    inline std::chrono::steady_clock::time_point g_animDebugStart{};
    inline PendingAnimationThrowRequest g_animThrowReq{};
    inline std::atomic_uint32_t g_loadShutdownGen{ 1 };

    inline std::atomic_bool g_waitingHitFrame{ false };

    inline bool ReserveHitFrameThrow(const char* reason)
    {
        bool expected = false;
        if (!g_waitingHitFrame.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            SKSE::log::debug(
                "[SaberThrow] animation-triggered throw request skipped: previous request has not moved its equipped item to the temp container yet ({}).",
                reason ? reason : "no reason");
            return false;
        }

        return true;
    }

    inline void ClearHitFrameThrow(const char* reason)
    {
        const bool wasSet = g_waitingHitFrame.exchange(false, std::memory_order_acq_rel);
        if (wasSet && !AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] cleared animation-triggered pre-transfer reservation: {}.",
                reason ? reason : "no reason");
        }
    }

    inline bool IsHitFrameThrowWaiting()
    {
        return g_waitingHitFrame.load(std::memory_order_acquire);
    }

    inline bool ReserveThrowSpell(const char* reason)
    {
        return ReserveHitFrameThrow(
            reason ? reason : "SaberThrow spell cast before validation");
    }

    inline bool AnimEventNameEquals(
        std::string_view left,
        std::string_view right)
    {
        if (left.size() != right.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left.size(); ++i) {
            const unsigned char leftChar = static_cast<unsigned char>(left[i]);
            const unsigned char rightChar = static_cast<unsigned char>(right[i]);
            if (std::tolower(leftChar) != std::tolower(rightChar)) {
                return false;
            }
        }

        return true;
    }

    inline bool AnimEventPassesFilter(const char* tag, const char* payload)
    {
        std::string filter;
        {
            std::scoped_lock lock(g_animDebugLock);
            filter = g_animDebugFilter;
        }

        if (filter.empty()) {
            return true;
        }

        const std::string tagText = tag ? tag : "";
        const std::string payloadText = payload ? payload : "";

        return tagText.find(filter) != std::string::npos ||
            payloadText.find(filter) != std::string::npos;
    }


    inline bool TakePendingAnimThrow(
        const char* tag,
        PendingAnimationThrowRequest& outRequest)
    {
        if (!tag || tag[0] == '\0') {
            return false;
        }

        std::scoped_lock lock(g_animDebugLock);
        if (!g_animThrowReq.active) {
            return false;
        }

        if (g_animThrowReq.triggerEventName != tag &&
            !(AnimEventNameEquals(
                g_animThrowReq.triggerEventName,
                "HitFrame") &&
                AnimEventNameEquals(tag, "HitFrame"))) {
            return false;
        }

        outRequest = g_animThrowReq;
        g_animThrowReq.active = false;
        g_animThrowReq.refHandle = nullptr;
        return true;
    }

    inline bool UseFTGThrowSettings(const std::string& modName)
    {
        return modName.find("ThrowableWeaponsSKSE") != std::string::npos;
    }

    inline void ApplyThrowTravelSettings(
        const std::string& modName,
        bool noReturnDynamic,
        float& totalTravel,
        float& stepDist)
    {
        if (!UseFTGThrowSettings(modName)) {
            return;
        }

        const auto settings = ::SaberThrow::Settings::Get();

        const float configDist = noReturnDynamic ?
            settings.noReturnDist :
            settings.throwDist;

        const float configuredSpeed = noReturnDynamic ?
            settings.noReturnSpeed :
            settings.throwSpeed;

        totalTravel = std::clamp(
            std::isfinite(configDist) ? configDist : totalTravel,
            0.0f,
            100000.0f);

        stepDist = std::clamp(
            std::isfinite(configuredSpeed) ? configuredSpeed : stepDist,
            0.0f,
            100000.0f);

        SKSE::log::debug(
            "[SaberThrow] ThrowEquippedWeapon modName='{}' contains ThrowableWeaponsSKSE; using {} INI travel: distance={} speed={}.",
            modName,
            noReturnDynamic ? "no-return" : "returning",
            totalTravel,
            stepDist);
    }

    inline void RunPendingAnimThrow(
        const std::string& triggerCopy,
        PendingAnimationThrowRequest request)
    {
        if (!request.refHandle) {
            SKSE::log::warn(
                "[SaberThrow] Animation-triggered throw skipped on '{}': lost dropped ref.",
                triggerCopy);
            ClearHitFrameThrow("lost dropped ref before inventory transfer");
            return;
        }

        if (request.loadShutdownGen !=
            g_loadShutdownGen.load(std::memory_order_acquire)) {
            ScopedLoadScreenCleanupLogSuppression suppressLoadLogs;
            DisableDeleteThrownRef(
                request.refHandle.get(),
                "stale delayed animation throw invalidated by load-screen cleanup");
            ClearHitFrameThrow(
                "stale delayed animation throw invalidated by load-screen cleanup");
            return;
        }

        if (request.drainOnAnim) {
            if (!DrainPendingStam(request)) {
                SKSE::log::warn(
                    "[SaberThrow] Animation-triggered throw skipped on '{}': could not drain deferred stamina.",
                    triggerCopy);
                if (request.refHandle && !request.refHandle->IsDisabled()) {
                    request.refHandle->Disable();
                }
                if (request.refHandle) {
                    RemovePickupBlock(request.refHandle.get());
                    request.refHandle->SetDelete(true);
                }
                ClearHitFrameThrow("deferred stamina drain failed before forced inventory transfer");
                return;
            }
        }

        if (request.cachePoison) {
            AutoCacheThrowPoison(
                request.refHandle.get(),
                "ThrowEquippedWeapon native-spawned weapon before temp-container transfer");
        }

        SendThrowStartEvent(
            request.refHandle.get(),
            request.modName.c_str(),
            triggerCopy.c_str(),
            request.noReturnDynamic);

        if (request.hasThrowPoints) {
            ThrowWithPrecomputedPoints(
                request.modName.c_str(),
                request.refHandle.get(),
                request.cachedSnapPos,
                request.cachedFarPos,
                request.totalTravel,
                request.stepDist,
                request.spinRateRadTick,
                request.noReturnDynamic,
                kDefaultSpearMode,
                true,
                request.hasThrowHand,
                request.throwWasLeft);
        }
        else {
            Throw(
                request.modName.c_str(),
                request.refHandle.get(),
                request.totalTravel,
                request.stepDist,
                request.spinRateRadTick,
                request.noReturnDynamic,
                kDefaultSpearMode,
                true,
                request.hasThrowHand,
                request.throwWasLeft);
        }
    }

    inline void QueuePendingAnimThrow(
        const char* triggerTag,
        PendingAnimationThrowRequest request)
    {
        const std::string triggerCopy =
            (triggerTag && triggerTag[0] != '\0') ? triggerTag : "";

        if (!DrainPendingStam(request)) {
            SKSE::log::warn(
                "[SaberThrow] Animation-triggered throw skipped on '{}': could not drain deferred stamina.",
                triggerCopy);
            if (request.refHandle && !request.refHandle->IsDisabled()) {
                request.refHandle->Disable();
            }
            if (request.refHandle) {
                RemovePickupBlock(request.refHandle.get());
                request.refHandle->SetDelete(true);
            }
            ClearHitFrameThrow("deferred stamina drain failed before inventory transfer");
            return;
        }
        request.drainOnAnim = false;

        auto queueLaunchTask = [triggerCopy, request]() mutable {
            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask([triggerCopy, request]() mutable {
                    RunPendingAnimThrow(triggerCopy, request);
                    });
            }
            else {
                SKSE::log::warn(
                    "[SaberThrow] Animation-triggered throw has no SKSE task interface; running immediately.");
                RunPendingAnimThrow(triggerCopy, request);
            }
        };

        if (!request.legacyHitDelay ||
            request.animRequestTime == std::chrono::steady_clock::time_point{}) {
            queueLaunchTask();
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMsSigned = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - request.animRequestTime).count();
        const std::uint64_t elapsedMs = elapsedMsSigned > 0 ?
            static_cast<std::uint64_t>(elapsedMsSigned) :
            0;

        if (elapsedMs >= kImmediateHitMs) {
            SKSE::log::debug(
                "[SaberThrow] HitFrame arrived at {} ms; launching immediately without post-anchor delay.",
                elapsedMs);
            queueLaunchTask();
            return;
        }

        const std::uint64_t waitToAnchorMs = elapsedMs < kHitFrameAnchorMs ?
            (kHitFrameAnchorMs - elapsedMs) :
            0;
        const std::uint64_t fixedDelayMs = request.hitDelayMs;
        const std::uint64_t totalDelayMs = waitToAnchorMs + fixedDelayMs;

        if (totalDelayMs == 0) {
            queueLaunchTask();
            return;
        }

        SKSE::log::debug(
            "[SaberThrow] Early HitFrame at {} ms; waiting {} ms to the 500 ms anchor plus {} ms fixed delay.",
            elapsedMs,
            waitToAnchorMs,
            fixedDelayMs);

        std::thread([queueLaunchTask = std::move(queueLaunchTask), totalDelayMs]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(totalDelayMs));
            queueLaunchTask();
            }).detach();
    }

    class PlayerAnimationEventDebugSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
        {
            if (!event || !g_animDebug.load(std::memory_order_acquire)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || event->holder != player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const char* tag = event->tag.c_str();
            const char* payload = event->payload.c_str();

            if (!AnimEventPassesFilter(tag, payload)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            PendingAnimationThrowRequest request{};
            if (TakePendingAnimThrow(tag, request)) {
                QueuePendingAnimThrow(tag, request);
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline PlayerAnimationEventDebugSink g_animDebugSink{};

    inline bool AddAnimDebugSink()
    {
        if (g_playerAnimReady.load(std::memory_order_acquire)) {
            return true;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::warn("[SaberThrow] Could not install player animation event debug sink: no player.");
            return false;
        }

        const bool ok = player->AddAnimationGraphEventSink(&g_animDebugSink);
        if (ok) {
            g_playerAnimReady.store(true, std::memory_order_release);
        }
        else {
            SKSE::log::warn("[SaberThrow] Player animation event debug sink install failed.");
        }

        return ok;
    }

    inline bool RemoveAnimDebugSink()
    {
        if (!g_playerAnimReady.load(std::memory_order_acquire)) {
            return true;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            g_playerAnimReady.store(false, std::memory_order_release);
            return false;
        }

        player->RemoveAnimationGraphEventSink(&g_animDebugSink);
        g_playerAnimReady.store(false, std::memory_order_release);
        return true;
    }

    inline std::uint32_t StartAnimDebug(const char* filter = nullptr)
    {
        {
            std::scoped_lock lock(g_animDebugLock);
            g_animDebugFilter = (filter && filter[0] != '\0') ? filter : "";
            g_animDebugStart = std::chrono::steady_clock::now();
        }

        const bool installed = AddAnimDebugSink();
        if (!installed) {
            g_animDebug.store(false, std::memory_order_release);
            return 0;
        }

        const std::uint32_t session = g_animSession.fetch_add(1, std::memory_order_acq_rel) + 1;
        g_animDebug.store(true, std::memory_order_release);

        return session;
    }

    inline void QueueStopAnimDebug(std::uint32_t session);

    inline bool StopAnimDebug(std::uint32_t session)
    {
        if (session == 0) {
            return false;
        }

        if (g_animSession.load(std::memory_order_acquire) != session) {
            return false;
        }

        bool retryPendingThrow = false;
        bool forcePendingThrow = false;
        std::string retryAnimEvent{};
        std::uint32_t retryMaxAttempts = 0;
        PendingAnimationThrowRequest forcedRequest{};

        {
            std::scoped_lock lock(g_animDebugLock);

            if (g_animThrowReq.active && g_animThrowReq.session == session) {
                retryMaxAttempts = std::max<std::uint32_t>(g_animThrowReq.maxAttempts, 1);

                if (g_animThrowReq.attempt < retryMaxAttempts) {
                    ++g_animThrowReq.attempt;
                    retryAnimEvent = g_animThrowReq.animEvent;
                    const auto retryRequestedAt = std::chrono::steady_clock::now();
                    g_animDebugStart = retryRequestedAt;
                    g_animThrowReq.animRequestTime = retryRequestedAt;
                    retryPendingThrow = true;
                }
                else {
                    forcePendingThrow = true;
                    forcedRequest = g_animThrowReq;

                    g_animThrowReq = PendingAnimationThrowRequest{};
                }
            }
        }

        if (retryPendingThrow) {
            TrySendPlayerShoutAnim(retryAnimEvent.c_str());
            QueueStopAnimDebug(session);
            return false;
        }

        if (forcePendingThrow) {
            if (forcedRequest.refHandle) {
                RunPendingAnimThrow(
                    forcedRequest.triggerEventName,
                    forcedRequest);
            }
            else {
                SKSE::log::warn("[SaberThrow] Could not force animation-triggered throw: lost dropped ref.");
                ClearHitFrameThrow("forced animation-triggered throw lost dropped ref before inventory transfer");
            }
        }

        g_animDebug.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(g_animDebugLock);
            g_animDebugFilter.clear();
            g_animDebugStart = std::chrono::steady_clock::time_point{};
        }

        RemoveAnimDebugSink();
        return true;
    }

    inline void QueueStopAnimDebug(std::uint32_t session)
    {
        if (session == 0) {
            return;
        }

        std::thread([session]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(kAnimDebugMs));

            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask([session]() {
                    StopAnimDebug(session);
                    });
            }
            else {
                if (g_animSession.load(std::memory_order_acquire) == session) {
                    g_animDebug.store(false, std::memory_order_release);
                }
            }
            }).detach();
    }

    inline bool QueueShoutAnim(const char* eventName = kShoutStartAnim)
    {
        const std::string eventCopy =
            (eventName && eventName[0] != '\0')
            ? eventName
            : kShoutStartAnim;

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return TrySendPlayerShoutAnim(eventCopy.c_str());
        }

        taskInterface->AddTask([eventCopy]() {
            TrySendPlayerShoutAnim(eventCopy.c_str());
            });

        return true;
    }


    enum class NativeEquippedThrowPreference : std::uint8_t
    {
        Any,
        Weapon,
        Shield
    };

    inline NativeEquippedThrowPreference GetThrowHandHint(
        RE::TESObjectREFR* hintRef)
    {
        if (!hintRef) {
            return NativeEquippedThrowPreference::Any;
        }

        auto* baseObj = hintRef->GetObjectReference();
        if (!baseObj) {
            return NativeEquippedThrowPreference::Any;
        }

        if (baseObj->As<RE::TESObjectWEAP>()) {
            return NativeEquippedThrowPreference::Weapon;
        }

        if (auto* armor = baseObj->As<RE::TESObjectARMO>(); IsShieldForTransfer(armor)) {
            return NativeEquippedThrowPreference::Shield;
        }

        if (IsThrowTorchBase(baseObj)) {
            return NativeEquippedThrowPreference::Shield;
        }

        return NativeEquippedThrowPreference::Any;
    }

    enum class ThrowWeaponHandedness : std::uint8_t
    {
        Unknown,
        OneHanded,
        TwoHanded
    };

    inline ThrowWeaponHandedness GetWeaponThrowHand(
        RE::TESObjectWEAP* weapon,
        const ::SaberThrow::Settings::Values& settings);

    inline RE::TESObjectWEAP* GetPlayerRHEquipWeapon(RE::PlayerCharacter* player)
    {
        if (!player) {
            return nullptr;
        }

        if (auto* rightObj = player->GetEquippedObject(false)) {
            return rightObj->As<RE::TESObjectWEAP>();
        }

        return nullptr;
    }

    inline RE::TESObjectWEAP* GetPlayerLHEquipWeapon(RE::PlayerCharacter* player)
    {
        if (!player) {
            return nullptr;
        }

        if (auto* leftObj = player->GetEquippedObject(true)) {
            return leftObj->As<RE::TESObjectWEAP>();
        }

        return nullptr;
    }

    inline RE::TESObjectARMO* GetPlayerShield(RE::PlayerCharacter* player)
    {
        if (!player) {
            return nullptr;
        }

        if (auto* leftObj = player->GetEquippedObject(true)) {
            if (auto* armor = leftObj->As<RE::TESObjectARMO>(); IsShieldForTransfer(armor)) {
                return armor;
            }
        }

        return nullptr;
    }

    inline RE::TESBoundObject* GetPlayerTorch(RE::PlayerCharacter* player)
    {
        if (!player) {
            return nullptr;
        }

        auto* leftObj = player->GetEquippedObject(true);
        auto* leftBoundObject = leftObj ? leftObj->As<RE::TESBoundObject>() : nullptr;
        return IsThrowTorchBase(leftBoundObject) ? leftBoundObject : nullptr;
    }

    inline bool RHHasSpellOrStaff(RE::PlayerCharacter* player)
    {
        if (!player) {
            return false;
        }

        auto* rightObj = player->GetEquippedObject(false);
        if (!rightObj) {
            return false;
        }

        if (rightObj->As<RE::SpellItem>()) {
            return true;
        }

        auto* rightWeapon = rightObj->As<RE::TESObjectWEAP>();
        return rightWeapon && rightWeapon->IsStaff();
    }

    inline const std::string& GetShieldThrowAnim(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        return RHHasSpellOrStaff(player) ?
            settings.shieldBashAnim :
            settings.shieldAnim;
    }

    inline RE::TESBoundObject* GetPlayerShieldOrTorch(
        RE::PlayerCharacter* player,
        const char*& outLabel)
    {
        if (auto* shield = GetPlayerShield(player)) {
            outLabel = "equipped shield";
            return shield;
        }

        if (auto* torch = GetPlayerTorch(player)) {
            outLabel = "equipped torch";
            return torch;
        }

        outLabel = "equipped shield or torch";
        return nullptr;
    }

    inline RE::TESBoundObject* GetEquippedThrowBase(
        RE::PlayerCharacter* player,
        NativeEquippedThrowPreference preference,
        const std::string& animEvent,
        bool allowLeftFallback,
        bool forceShieldTorch,
        const char*& outLabel)
    {
        (void)preference;

        if (!player) {
            outLabel = "equipped item";
            return nullptr;
        }

        if (forceShieldTorch ||
            ((animEvent == "attackStartLeftHand" || animEvent == "BashStart") &&
                !allowLeftFallback)) {
            return GetPlayerShieldOrTorch(player, outLabel);
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const bool preferLeftHand =
            allowLeftFallback &&
            PreferLHThrowWeapon(settings);

        if (preferLeftHand) {
            if (auto* leftWeapon = GetPlayerLHEquipWeapon(player)) {
                outLabel = "left-hand equipped weapon";
                return leftWeapon;
            }

            if (auto* rightWeapon = GetPlayerRHEquipWeapon(player)) {
                outLabel = "right-hand equipped weapon";
                return rightWeapon;
            }
        }
        else {
            if (auto* rightWeapon = GetPlayerRHEquipWeapon(player)) {
                outLabel = "right-hand equipped weapon";
                return rightWeapon;
            }

            if (allowLeftFallback) {
                if (auto* leftWeapon = GetPlayerLHEquipWeapon(player)) {
                    outLabel = "left-hand equipped weapon";
                    return leftWeapon;
                }
            }
        }

        outLabel = allowLeftFallback ?
            (preferLeftHand ? "left/right-hand equipped weapon" : "right/left-hand equipped weapon") :
            "right-hand equipped weapon";
        return nullptr;
    }

    inline void SetupThrowRef(RE::TESObjectREFR* ref)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ref || !player) {
            return;
        }

        if (!ref->IsDisabled()) {
            ref->Disable();
        }

        const RE::NiPoint3 playerPos = player->GetPosition();
        ref->SetPosition(RE::NiPoint3{
            playerPos.x + 300.0f,
            playerPos.y + 300.0f,
            playerPos.z + 300.0f
            });

        BlockThrowActivation(ref, "ConfigureNativeEquippedThrowDroppedRefMainThread");
        ref->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, false);
    }

    inline void ClearLegacyRefHint(
        RE::TESObjectREFR* hintRef,
        RE::TESObjectREFR* createdRef)
    {
        if (!hintRef || hintRef == createdRef) {
            return;
        }

        auto* baseObj = hintRef->GetObjectReference();
        const bool legacyDropRef =
            baseObj &&
            (baseObj->As<RE::TESObjectWEAP>() ||
                (baseObj->As<RE::TESObjectARMO>() && IsShieldForTransfer(baseObj->As<RE::TESObjectARMO>())) ||
                IsThrowTorchBase(baseObj));

        if (!legacyDropRef) {
            return;
        }

        if (!hintRef->IsDisabled()) {
            hintRef->Disable();
        }
        RemovePickupBlock(hintRef);
        hintRef->SetDelete(true);
    }

    inline RE::NiPointer<RE::TESObjectREFR> CreateThrowRef(
        RE::TESObjectREFR* hintRef,
        const std::string& animEvent,
        bool allowLeftFallback,
        bool forceShieldTorch)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::warn("[SaberThrow] ThrowEquippedWeapon skipped: no player while creating native dropped ref.");
            return nullptr;
        }

        (void)hintRef;

        const char* itemLabel = "right-hand equipped weapon";
        auto* baseToPlace = GetEquippedThrowBase(
            player,
            NativeEquippedThrowPreference::Weapon,
            animEvent,
            allowLeftFallback,
            forceShieldTorch,
            itemLabel);

        if (!baseToPlace) {
            SKSE::log::warn(
                "[SaberThrow] ThrowEquippedWeapon skipped: could not resolve {} to create native dropped ref.",
                itemLabel ? itemLabel : "equipped item");
            return nullptr;
        }

        RE::NiPointer<RE::TESObjectREFR> droppedRef = player->PlaceObjectAtMe(baseToPlace, true);
        if (!droppedRef) {
            SKSE::log::warn(
                "[SaberThrow] ThrowEquippedWeapon skipped: PlaceObjectAtMe failed for {} 0x{:08X}.",
                itemLabel ? itemLabel : "equipped item",
                baseToPlace->GetFormID());
            return nullptr;
        }

        SetupThrowRef(droppedRef.get());

        SKSE::log::debug(
            "[SaberThrow] created native dropped ref 0x{:08X} from {} 0x{:08X} for ThrowEquippedWeapon.",
            droppedRef->GetFormID(),
            itemLabel ? itemLabel : "equipped item",
            baseToPlace->GetFormID());

        return droppedRef;
    }

    inline bool QueueAnimThrow(
        const char* animEvent,
        const char* triggerEventName,
        const char* modName,
        RE::TESObjectREFR* ref,
        float totalTravel,
        float stepDist,
        float spinRateRadTick,
        bool noReturnDynamic,
        bool allowLeftFallback = false,
        bool reservationHeld = false,
        bool drainOnAnim = false,
        float animStaminaCost = 0.0f,
        const char* staminaReason = nullptr,
        bool hasHandOverride = false,
        bool handOverrideLeft = false,
        bool shieldTorch = false)
    {
        if (!reservationHeld &&
            !ReserveHitFrameThrow("QueueSendPlayerAnimationAndThrowOnEvent")) {
            return false;
        }

        const std::string animEventCopy =
            (animEvent && animEvent[0] != '\0')
            ? animEvent
            : kShoutStartAnim;

        const std::string triggerEventCopy =
            (triggerEventName && triggerEventName[0] != '\0')
            ? triggerEventName
            : "HitFrame";

        std::string modNameCopy = modName ? modName : "";
        modNameCopy.erase(
            std::remove(modNameCopy.begin(), modNameCopy.end(), '|'),
            modNameCopy.end());

        if (modNameCopy.empty()) {
            SKSE::log::warn("[SaberThrow] ThrowEquippedWeapon skipped: empty mod name.");
            ClearHitFrameThrow("empty mod name");
            return false;
        }

        float finalTravel = totalTravel;
        float resolvedStepDist = stepDist;
        ApplyThrowTravelSettings(
            modNameCopy,
            noReturnDynamic,
            finalTravel,
            resolvedStepDist);

        RE::NiPointer<RE::TESObjectREFR> suppliedRefHint(ref);

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            SKSE::log::warn("[SaberThrow] ThrowEquippedWeapon failed: no SKSE task interface.");
            ClearHitFrameThrow("no SKSE task interface");
            return false;
        }

        taskInterface->AddTask([
            animEventCopy,
            triggerEventCopy,
            modNameCopy,
            suppliedRefHint,
            finalTravel,
            resolvedStepDist,
            spinRateRadTick,
            noReturnDynamic,
            allowLeftFallback,
            drainOnAnim,
            animStaminaCost,
            hasHandOverride,
            handOverrideLeft,
            shieldTorch,
            staminaReasonCopy = std::string(staminaReason ? staminaReason : "")]() mutable
        {
            RE::NiPointer<RE::TESObjectREFR> refHandle = suppliedRefHint;
            const bool nativeSpawnedRef = !refHandle;

            if (nativeSpawnedRef) {
                refHandle = CreateThrowRef(
                    nullptr,
                    animEventCopy,
                    allowLeftFallback,
                    shieldTorch);
            }

            if (!refHandle) {
                SKSE::log::warn(
                    "[SaberThrow] ThrowEquippedWeapon skipped: no dropped ref was supplied and native equipped-item spawn failed.");
                ClearHitFrameThrow("no dropped ref / native spawn failed");
                return;
            }

            bool effectiveNoReturn = noReturnDynamic;
            float effectiveTravel = finalTravel;
            float effectiveStepDist = resolvedStepDist;
            if (ForceBoomerangReturn(
                refHandle.get(),
                modNameCopy,
                effectiveNoReturn)) {
                effectiveNoReturn = false;
                ApplyThrowTravelSettings(
                    modNameCopy,
                    effectiveNoReturn,
                    effectiveTravel,
                    effectiveStepDist);

                SKSE::log::debug(
                    "[SaberThrow] WeaponTypeBoomerang detected for '{}'; forcing non-FTG throw to returning behavior: distance={} speed={}.",
                    modNameCopy,
                    effectiveTravel,
                    effectiveStepDist);
            }

            const bool legacyHitDelay =
                AnimEventNameEquals(triggerEventCopy, "HitFrame");
            auto* playerCamera = RE::PlayerCamera::GetSingleton();
            const bool thirdPerson =
                !playerCamera || !playerCamera->IsInFirstPerson();
            const std::uint64_t hitDelayMs =
                legacyHitDelay &&
                    effectiveNoReturn &&
                    thirdPerson ?
                kThirdHitDelayMs :
                0;

            auto* nativeSpawnedBase = refHandle->GetObjectReference();
            const bool cachePoison =
                nativeSpawnedRef &&
                nativeSpawnedBase &&
                nativeSpawnedBase->As<RE::TESObjectWEAP>() != nullptr;

            const std::uint32_t session = StartAnimDebug();
            if (session == 0) {
                SKSE::log::warn("[SaberThrow] ThrowEquippedWeapon failed: could not start animation event watcher.");
                ClearHitFrameThrow("could not start animation event watcher");
                if (nativeSpawnedRef) {
                    if (!refHandle->IsDisabled()) {
                        refHandle->Disable();
                    }
                    RemovePickupBlock(refHandle.get());
                    refHandle->SetDelete(true);
                }
                return;
            }

            RE::NiPoint3 cachedSnapPos{};
            RE::NiPoint3 cachedFarPos{};
            const bool hasThrowPoints = false;

            const auto animRequestTime = std::chrono::steady_clock::now();
            {
                std::scoped_lock lock(g_animDebugLock);
                g_animDebugStart = animRequestTime;
                g_animThrowReq.active = true;
                g_animThrowReq.session = session;
                g_animThrowReq.loadShutdownGen =
                    g_loadShutdownGen.load(std::memory_order_acquire);
                g_animThrowReq.triggerEventName = triggerEventCopy;
                g_animThrowReq.modName = modNameCopy;
                g_animThrowReq.refHandle = refHandle;
                g_animThrowReq.totalTravel = effectiveTravel;
                g_animThrowReq.stepDist = effectiveStepDist;
                g_animThrowReq.spinRateRadTick = spinRateRadTick;
                g_animThrowReq.noReturnDynamic = effectiveNoReturn;
                g_animThrowReq.animEvent = animEventCopy;
                g_animThrowReq.animRequestTime = animRequestTime;
                g_animThrowReq.legacyHitDelay = legacyHitDelay;
                g_animThrowReq.hitDelayMs =
                    hitDelayMs;
                if (hasHandOverride) {
                    g_animThrowReq.hasThrowHand = true;
                    g_animThrowReq.throwWasLeft =
                        handOverrideLeft;
                }
                else {
                    g_animThrowReq.hasThrowHand =
                        allowLeftFallback || animEventCopy == "BashStart";
                    g_animThrowReq.throwWasLeft =
                        animEventCopy == "attackStartLeftHand" ||
                        animEventCopy == "BashStart";
                }
                g_animThrowReq.cachePoison =
                    cachePoison;
                g_animThrowReq.drainOnAnim =
                    drainOnAnim;
                g_animThrowReq.animStaminaCost =
                    animStaminaCost;
                g_animThrowReq.staminaReason = staminaReasonCopy;
                g_animThrowReq.hasThrowPoints = hasThrowPoints;
                g_animThrowReq.cachedSnapPos = cachedSnapPos;
                g_animThrowReq.cachedFarPos = cachedFarPos;
                g_animThrowReq.attempt = 1;
                g_animThrowReq.maxAttempts = 2;
            }

            TrySendPlayerShoutAnim(animEventCopy.c_str());
            QueueStopAnimDebug(session);
        });

        return true;
    }


    inline RE::Setting* GetPowerCooldownSetting()
    {
        auto* gameSettings = RE::GameSettingCollection::GetSingleton();
        if (!gameSettings) {
            return nullptr;
        }

        return gameSettings->GetSetting(kPowerCooldownKey);
    }

    inline bool CachePowerCooldown(const char* reason)
    {
        if (g_cooldownCached.load(std::memory_order_acquire)) {
            return true;
        }

        auto* setting = GetPowerCooldownSetting();
        if (!setting) {
            SKSE::log::warn(
                "[SaberThrow] could not cache game setting '{}': setting not found. Reason: {}",
                kPowerCooldownKey,
                reason ? reason : "unknown");
            return false;
        }

        const float value = setting->GetFloat();
        g_powerCooldown.store(value, std::memory_order_release);
        g_cooldownCached.store(true, std::memory_order_release);

        SKSE::log::info(
            "[SaberThrow] cached game setting '{}': {}. Reason: {}",
            kPowerCooldownKey,
            value,
            reason ? reason : "unknown");

        return true;
    }

    inline void SetPowerCooldown(float value, const char* reason)
    {
        auto* setting = GetPowerCooldownSetting();
        if (!setting) {
            SKSE::log::warn(
                "[SaberThrow] could not set game setting '{}' to {}: setting not found. Reason: {}",
                kPowerCooldownKey,
                value,
                reason ? reason : "unknown");
            return;
        }

        setting->data.f = value;

        SKSE::log::debug(
            "[SaberThrow] set game setting '{}' to {}. Reason: {}",
            kPowerCooldownKey,
            value,
            reason ? reason : "unknown");
    }

    inline void ZeroThrowPowerCooldown(const char* reason)
    {
        CachePowerCooldown(reason);
        SetPowerCooldown(0.0f, reason);
        g_cooldownOverride.store(true, std::memory_order_release);
    }

    inline void RestorePowerCooldown(const char* reason)
    {
        if (!g_cooldownOverride.load(std::memory_order_acquire)) {
            return;
        }

        if (!g_cooldownCached.load(std::memory_order_acquire) &&
            !CachePowerCooldown(reason)) {
            return;
        }

        const float cachedValue = g_powerCooldown.load(std::memory_order_acquire);
        SetPowerCooldown(cachedValue, reason);
        g_cooldownOverride.store(false, std::memory_order_release);
    }

    inline RE::SpellItem* GetSpellFromEvent(const RE::TESSpellCastEvent* event)
    {
        if (!event || event->spell == 0) {
            return nullptr;
        }

        auto* rawForm = RE::TESForm::LookupByID(event->spell);
        return rawForm ? rawForm->As<RE::SpellItem>() : nullptr;
    }

    inline bool IsLesserPowerSpell(RE::SpellItem* spell)
    {
        return spell && spell->GetSpellType() == RE::MagicSystem::SpellType::kLesserPower;
    }

    inline RE::SpellItem* GetThrowTriggerSpell(RE::FormID localFormID)
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        return dataHandler->LookupForm<RE::SpellItem>(
            localFormID,
            kSpellCastPlugin);
    }

    template <class Caster, class = void>
    struct HasNPCThrowCompatibleCastSpellImmediate : std::false_type
    {
    };

    template <class Caster>
    struct HasNPCThrowCompatibleCastSpellImmediate<Caster, std::void_t<decltype(
        std::declval<Caster*>()->CastSpellImmediate(
            std::declval<RE::SpellItem*>(),
            false,
            std::declval<RE::TESObjectREFR*>(),
            1.0f,
            false,
            0.0f,
            std::declval<RE::Actor*>()))>> : std::true_type
    {};

    template <class Caster>
    inline bool CastNPCThrowSignalNow(
        Caster* caster,
        RE::SpellItem* spell,
        RE::Actor* sourceActor)
    {
        if (!caster || !spell || !sourceActor) {
            return false;
        }

        if constexpr (HasNPCThrowCompatibleCastSpellImmediate<Caster>::value) {
            caster->CastSpellImmediate(
                spell,
                false,
                static_cast<RE::TESObjectREFR*>(sourceActor),
                1.0f,
                false,
                0.0f,
                sourceActor);
            return true;
        }

        return false;
    }

    inline bool CastNPCSignalSelf(
        RE::Actor* sourceActor,
        RE::FormID localFormID)
    {
        if (!sourceActor || ActorDead(sourceActor)) {
            return false;
        }

        auto* spell = GetThrowTriggerSpell(localFormID);
        if (!spell) {
            return false;
        }

        if (auto* caster = sourceActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            CastNPCThrowSignalNow(caster, spell, sourceActor)) {
            return true;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            return false;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            return false;
        }

        const RE::VMHandle spellHandle = handlePolicy->GetHandleForObject(
            RE::FormType::Spell,
            spell);
        if (spellHandle == handlePolicy->EmptyHandle()) {
            return false;
        }

        auto args = std::unique_ptr<RE::BSScript::IFunctionArguments>(
            RE::MakeFunctionArguments(
                static_cast<RE::TESObjectREFR*>(sourceActor),
                static_cast<RE::TESObjectREFR*>(sourceActor)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        return vm->DispatchMethodCall(
            spellHandle,
            RE::BSFixedString("Spell"),
            RE::BSFixedString("Cast"),
            args.get(),
            callback);
    }

    inline bool DispelNPCSignal(
        RE::Actor* sourceActor,
        RE::FormID localFormID)
    {
        if (!sourceActor) {
            return false;
        }

        auto* spell = GetThrowTriggerSpell(localFormID);
        auto* magicTarget = sourceActor->AsMagicTarget();
        auto* activeEffects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!spell || !activeEffects) {
            return false;
        }

        std::vector<RE::ActiveEffect*> matchingEffects;
        for (auto* activeEffect : *activeEffects) {
            if (activeEffect && activeEffect->spell == spell) {
                matchingEffects.push_back(activeEffect);
            }
        }

        for (auto* activeEffect : matchingEffects) {
            activeEffect->Dispel(true);
        }

        return !matchingEffects.empty();
    }

    inline bool ThrowSpellEventMatches(
        const RE::TESSpellCastEvent* event,
        RE::FormID localFormID)
    {
        if (!event || event->spell == 0) {
            return false;
        }

        if (event->spell == localFormID) {
            return true;
        }

        auto* spell = GetThrowTriggerSpell(localFormID);
        return spell && event->spell == spell->GetFormID();
    }

    inline bool IsThrowSpellEvent(const RE::TESSpellCastEvent* event)
    {
        return ThrowSpellEventMatches(
            event,
            kReturnSpellID) ||
            ThrowSpellEventMatches(
                event,
                kNoReturnSpellID) ||
            ThrowSpellEventMatches(
                event,
                kNoReturnFTGSpellID) ||
            ThrowSpellEventMatches(
                event,
                kShieldSpellID);
    }

    inline bool ThrowSpellMatchesTrigger(
        RE::FormID formID,
        RE::FormID localFormID)
    {
        if (formID == 0) {
            return false;
        }

        if (formID == localFormID) {
            return true;
        }

        auto* spell = GetThrowTriggerSpell(localFormID);
        return spell && formID == spell->GetFormID();
    }

    inline bool ThrowSpellIDIsTrigger(RE::FormID formID)
    {
        return ThrowSpellMatchesTrigger(
            formID,
            kReturnSpellID) ||
            ThrowSpellMatchesTrigger(
                formID,
                kNoReturnSpellID) ||
            ThrowSpellMatchesTrigger(
                formID,
                kNoReturnFTGSpellID) ||
            ThrowSpellMatchesTrigger(
                formID,
                kShieldSpellID);
    }

    inline bool ThrowSpellFormIsTrigger(RE::TESForm* form)
    {
        return form && ThrowSpellIDIsTrigger(form->GetFormID());
    }

    inline void HandlePowerCooldown(
        RE::TESForm* equippedForm,
        const char* reason)
    {
        if (!equippedForm) {
            return;
        }

        if (ThrowSpellFormIsTrigger(equippedForm)) {
            ZeroThrowPowerCooldown(reason);
            return;
        }

        auto* spell = equippedForm->As<RE::SpellItem>();
        if (IsLesserPowerSpell(spell)) {
            RestorePowerCooldown(reason);
        }
    }

    inline void PrimePowerCooldown(const char* reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        auto* selectedPower = player->GetActorRuntimeData().selectedPower;
        if (!selectedPower) {
            return;
        }

        HandlePowerCooldown(
            selectedPower,
            reason ? reason : "startup/current selected player power");
    }

    inline void HandleSpellCooldown(
        const RE::TESSpellCastEvent* event,
        bool watchedSpell)
    {
        if (watchedSpell) {
            ZeroThrowPowerCooldown(
                "watched SaberThrow lesser-power spell cast");
            return;
        }

        auto* spell = GetSpellFromEvent(event);
        if (IsLesserPowerSpell(spell)) {
            RestorePowerCooldown(
                "non-SaberThrow lesser power cast");
        }
    }

    inline RE::TESObjectWEAP* GetThrowSpellWeapon(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!player) {
            return nullptr;
        }

        const bool preferLeftHand = PreferLHThrowWeapon(settings);
        if (preferLeftHand) {
            if (auto* leftWeapon = GetPlayerLHEquipWeapon(player)) {
                return leftWeapon;
            }
            return GetPlayerRHEquipWeapon(player);
        }

        if (auto* rightWeapon = GetPlayerRHEquipWeapon(player)) {
            return rightWeapon;
        }
        return GetPlayerLHEquipWeapon(player);
    }

    inline bool HasThrowSpellWeapon(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings,
        const char* reason)
    {
        const bool preferLeftHand = PreferLHThrowWeapon(settings);

        if (preferLeftHand) {
            if (GetPlayerLHEquipWeapon(player)) {
                SKSE::log::debug(
                    "[SaberThrow] player spell-cast trigger '{}': imadSaberThrowLeftHand=1; using left-hand weapon.",
                    reason ? reason : "unknown");
                return true;
            }

            if (GetPlayerRHEquipWeapon(player)) {
                SKSE::log::debug(
                    "[SaberThrow] player spell-cast trigger '{}': imadSaberThrowLeftHand=1 but no left-hand weapon equipped; using right-hand weapon fallback.",
                    reason ? reason : "unknown");
                return true;
            }
        }
        else {
            if (GetPlayerRHEquipWeapon(player)) {
                return true;
            }

            if (GetPlayerLHEquipWeapon(player)) {
                SKSE::log::debug(
                    "[SaberThrow] player spell-cast trigger '{}': no right-hand weapon equipped; using left-hand weapon fallback.",
                    reason ? reason : "unknown");
                return true;
            }
        }

        SKSE::log::debug(
            "[SaberThrow] player spell-cast trigger '{}' ignored: no right-hand or left-hand weapon equipped.",
            reason ? reason : "unknown");
        return false;
    }

    inline bool IsThrowWeaponLeft(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!player) {
            return false;
        }

        const bool preferLeftHand = PreferLHThrowWeapon(settings);
        auto* leftWeapon = GetPlayerLHEquipWeapon(player);
        auto* rightWeapon = GetPlayerRHEquipWeapon(player);

        if (preferLeftHand) {
            return leftWeapon != nullptr;
        }

        return !rightWeapon && leftWeapon != nullptr;
    }

    inline bool UseLHThrowAnim(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!player) {
            return false;
        }

        auto* selectedWeapon = GetThrowSpellWeapon(player, settings);

        if (selectedWeapon && selectedWeapon->IsBow()) {
            SKSE::log::debug(
                "[SaberThrow] selected spell-cast weapon is a bow; using configured left-hand animation.");
            return true;
        }

        if (selectedWeapon &&
            GetWeaponThrowHand(selectedWeapon, settings) == ThrowWeaponHandedness::TwoHanded) {
            SKSE::log::debug(
                "[SaberThrow] selected spell-cast weapon is two-handed; using configured standard/right animation despite left-hand preference.");
            return false;
        }

        return IsThrowWeaponLeft(player, settings);
    }

    inline ::SaberThrow::Settings::ThrowOrientation GetThrowOrientAnim(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings,
        bool noReturnDynamic)
    {
        auto* weapon = GetThrowSpellWeapon(player, settings);

        auto orientation = noReturnDynamic ?
            settings.weaponOrient :
            settings.telekOrient;

        if (WeaponIsBoomerang(weapon)) {
            orientation = ::SaberThrow::Settings::ThrowOrientation::Horizontal;
        }
        if (WeaponHasSpearKeyword(weapon, settings)) {
            orientation = ::SaberThrow::Settings::ThrowOrientation::SpearLike;
        }

        return orientation;
    }

    inline const std::string& GetThrowAnimEvent(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings,
        bool noReturnDynamic)
    {
        if (UseLHThrowAnim(player, settings)) {
            return settings.weaponAnimLeft;
        }

        const auto orientation =
            GetThrowOrientAnim(
                player,
                settings,
                noReturnDynamic);

        return orientation == ::SaberThrow::Settings::ThrowOrientation::Horizontal ?
            settings.telekAnim :
            settings.weaponAnimRight;
    }

    inline bool HasThrowSpellShield(
        RE::PlayerCharacter* player,
        const char* reason)
    {
        const char* itemLabel = "equipped shield or torch";
        if (GetPlayerShieldOrTorch(player, itemLabel)) {
            SKSE::log::debug(
                "[SaberThrow] player spell-cast trigger '{}': using {}.",
                reason ? reason : "unknown",
                itemLabel ? itemLabel : "equipped shield or torch");
            return true;
        }

        SKSE::log::debug(
            "[SaberThrow] player spell-cast trigger '{}' ignored: no shield or torch equipped in the left hand.",
            reason ? reason : "unknown");
        return false;
    }

    inline void ShowLowStamNotify();
    inline void NotifyMissingThrowPerk();

    inline bool PlayerHasAnyPerk(
        RE::PlayerCharacter* player,
        const std::vector<::SaberThrow::Settings::PerkFormSpec>& perkSpecs);

    inline float GetThrowShieldStamMult(
        const ::SaberThrow::Settings::Values& settings);

    inline float GetThrowTorchStamMult(
        const ::SaberThrow::Settings::Values& settings);

    inline float GetWeaponThrowStamCost(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings);

    inline float GetShieldThrowStamCost(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings);

    inline bool HasThrowStamina(
        RE::PlayerCharacter* player,
        float staminaCost,
        const char* reason)
    {
        if (!player) {
            return false;
        }

        staminaCost = std::max(staminaCost, 0.0f);
        if (staminaCost <= 0.0f) {
            return true;
        }

        const float stamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        if (stamina + 0.001f < staminaCost) {
            SKSE::log::debug(
                "[SaberThrow] player spell-cast trigger '{}' ignored: stamina {}/{} is too low.",
                reason ? reason : "unknown",
                stamina,
                staminaCost);
            ShowLowStamNotify();
            return false;
        }

        return true;
    }

    inline bool DrainThrowStamina(
        RE::PlayerCharacter* player,
        float staminaCost,
        const char* reason)
    {
        if (!player) {
            return false;
        }

        staminaCost = std::max(staminaCost, 0.0f);
        if (staminaCost <= 0.0f) {
            return true;
        }

        const float stamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);

        player->AsActorValueOwner()->DamageActorValue(
            RE::ActorValue::kStamina,
            staminaCost);

        const float staminaAfter = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        SKSE::log::debug(
            "[SaberThrow] player spell-cast trigger '{}' consumed {} stamina after animation trigger: {} -> {}.",
            reason ? reason : "unknown",
            staminaCost,
            stamina,
            staminaAfter);

        return true;
    }

    inline bool DrainPendingStam(
        const PendingAnimationThrowRequest& request)
    {
        if (!request.drainOnAnim) {
            return true;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        return DrainThrowStamina(
            player,
            request.animStaminaCost,
            request.staminaReason.empty() ? "animation-triggered SaberThrow spell" : request.staminaReason.c_str());
    }

    inline bool HasThrowPerk(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings,
        const char* reason)
    {
        if (!settings.weaponNeedsPerk) {
            return true;
        }

        auto* weapon = GetThrowSpellWeapon(player, settings);
        if (!weapon) {
            return false;
        }

        const ThrowWeaponHandedness handedness =
            GetWeaponThrowHand(weapon, settings);
        const auto& requiredPerks = handedness == ThrowWeaponHandedness::TwoHanded ?
            settings.weaponPerks2H :
            settings.weaponPerks1H;

        if (PlayerHasAnyPerk(player, requiredPerks)) {
            return true;
        }

        SKSE::log::debug(
            "[SaberThrow] player spell-cast trigger '{}' ignored: missing a configured {} throw perk ({} candidate perks).",
            reason ? reason : "unknown",
            handedness == ThrowWeaponHandedness::TwoHanded ? "2H" : "1H",
            requiredPerks.size());
        NotifyMissingThrowPerk();
        return false;
    }

    inline bool ValidateThrowSpell(
        RE::PlayerCharacter* player,
        bool requireRight,
        bool requireShield,
        bool checkStamina,
        bool checkWeaponPerk,
        const ::SaberThrow::Settings::Values& settings,
        const char* reason)
    {
        if (!player) {
            return false;
        }

        if (requireRight &&
            !HasThrowSpellWeapon(player, settings, reason)) {
            return false;
        }

        if (requireRight &&
            checkWeaponPerk &&
            !HasThrowPerk(player, settings, reason)) {
            return false;
        }

        if (requireShield &&
            !HasThrowSpellShield(player, reason)) {
            return false;
        }

        if (requireShield && settings.shieldNeedsPerk) {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            auto* perk = dataHandler ?
                dataHandler->LookupForm<RE::BGSPerk>(
                    settings.shieldPerkID,
                    settings.shieldPerkPlugin) :
                nullptr;
            if (!perk || !player->HasPerk(perk)) {
                SKSE::log::debug(
                    "[SaberThrow] player spell-cast trigger '{}' ignored: missing configured shield throw perk 0x{:08X}/'{}'.",
                    reason ? reason : "unknown",
                    settings.shieldPerkID,
                    settings.shieldPerkPlugin);
                return false;
            }
        }

        if (checkStamina) {
            const float staminaCost = requireRight ?
                GetWeaponThrowStamCost(player, settings) :
                (requireShield ?
                    GetShieldThrowStamCost(player, settings) :
                    settings.staminaCost);

            if (!HasThrowStamina(
                player,
                staminaCost,
                reason)) {
                return false;
            }
        }

        return true;
    }

    inline bool StopFTGThrow(const char* reason)
    {
        if (!g_saberRunning.load(std::memory_order_acquire)) {
            return false;
        }

        for (auto& state : g_throwStates) {
            if (!state) {
                continue;
            }

            g_currentState = state.get();

            if (g_state.modName != "ThrowableWeaponsSKSEFTG" ||
                !g_state.noReturnDynamic ||
                (g_state.phase != Phase::Snap && g_state.phase != Phase::Out)) {
                continue;
            }

            auto* ref = g_state.refHandle.get();
            if (!ref || ref->IsDisabled() || !GetWeaponBase(ref)) {
                continue;
            }

            SKSE::log::debug(
                "[SaberThrow] player spell-cast trigger '{}': active FTG throw detected mid-air; requesting FTG stop/teleport instead of starting a new throw.",
                reason ? reason : "unknown");

            g_state.forceReturn = true;
            g_currentState = &g_fallbackState;
            QueueUpdateTask();
            return true;
        }

        g_currentState = &g_fallbackState;
        return false;
    }

    inline bool QueueThrowSpell(
        const char* animEvent,
        const char* modName,
        bool noReturnDynamic,
        const ::SaberThrow::Settings::Values& settings,
        const char* reason,
        bool allowLeftFallback = false,
        bool reservationHeld = false,
        bool drainStaminaOnHit = false,
        bool useShieldStamina = false,
        bool throwWasLeft = false,
        float spinMultiplier = 1.0f)
    {
        const float totalTravel = noReturnDynamic ?
            settings.noReturnDist :
            settings.throwDist;

        const float stepDist = noReturnDynamic ?
            settings.noReturnSpeed :
            settings.throwSpeed;

        const float staminaCost =
            drainStaminaOnHit ?
            (allowLeftFallback ?
                GetWeaponThrowStamCost(
                    RE::PlayerCharacter::GetSingleton(),
                    settings) :
                (useShieldStamina ?
                    GetShieldThrowStamCost(
                        RE::PlayerCharacter::GetSingleton(),
                        settings) :
                    settings.staminaCost)) :
            settings.staminaCost;

        const float spinMult = std::clamp(spinMultiplier, 0.0f, 100.0f);

        SKSE::log::debug(
            "[SaberThrow] player spell-cast trigger '{}': animation='{}' listenFor='{}' modName='{}' noReturn={} distance={} speed={} spinMultiplier={} pendingStaminaCost={}",
            reason ? reason : "unknown",
            animEvent ? animEvent : "",
            settings.throwTriggerEvent,
            modName ? modName : "",
            noReturnDynamic,
            totalTravel,
            stepDist,
            spinMult,
            staminaCost);

        return QueueAnimThrow(
            animEvent,
            settings.throwTriggerEvent.c_str(),
            modName,
            nullptr,
            totalTravel,
            stepDist,
            0.04f * spinMult,
            noReturnDynamic,
            allowLeftFallback,
            reservationHeld,
            drainStaminaOnHit,
            staminaCost,
            reason,
            true,
            throwWasLeft,
            useShieldStamina);
    }

    class SaberThrowEquipEventSink final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEquipEvent* event,
            RE::BSTEventSource<RE::TESEquipEvent>*) override
        {
            if (!event || !event->equipped || event->baseObject == 0) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* actorRef = event->actor.get();
            auto* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
            if (!actor || !actor->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* equippedForm = RE::TESForm::LookupByID(event->baseObject);
            HandlePowerCooldown(
                equippedForm,
                "player equipped/selected lesser power");

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class SaberThrowSpellCastEventSink final : public RE::BSTEventSink<RE::TESSpellCastEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSpellCastEvent* event,
            RE::BSTEventSource<RE::TESSpellCastEvent>*) override
        {
            if (!event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* casterRef = event->object.get();
            auto* caster = casterRef ? casterRef->As<RE::Actor>() : nullptr;
            if (!caster || !caster->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const bool watchedSpell = IsThrowSpellEvent(event);
            HandleSpellCooldown(event, watchedSpell);

            const auto settings = ::SaberThrow::Settings::Get();

            if (ThrowSpellEventMatches(
                event,
                kReturnSpellID)) {
                constexpr const char* reason = "0x000800 return weapon spell";
                if (!ReserveThrowSpell(reason)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (ValidateThrowSpell(
                    player,
                    true,
                    false,
                    false,
                    false,
                    settings,
                    reason)) {
                    const bool selectedLeft =
                        IsThrowWeaponLeft(player, settings);
                    const std::string& animEvent =
                        GetThrowAnimEvent(
                            player,
                            settings,
                            false);

                    QueueThrowSpell(
                        animEvent.c_str(),
                        "ThrowableWeaponsSKSE",
                        false,
                        settings,
                        reason,
                        true,
                        true,
                        false,
                        false,
                        selectedLeft,
                        settings.telekSpinMult);
                }
                else {
                    ClearHitFrameThrow("return weapon spell validation failed before inventory transfer");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            if (ThrowSpellEventMatches(
                event,
                kNoReturnSpellID)) {
                constexpr const char* reason = "0x00080C no-return weapon spell";
                if (!ReserveThrowSpell(reason)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (ValidateThrowSpell(
                    player,
                    true,
                    false,
                    true,
                    true,
                    settings,
                    reason)) {
                    const bool selectedLeft =
                        IsThrowWeaponLeft(player, settings);
                    const std::string& animEvent =
                        GetThrowAnimEvent(
                            player,
                            settings,
                            true);

                    QueueThrowSpell(
                        animEvent.c_str(),
                        "ThrowableWeaponsSKSE",
                        true,
                        settings,
                        reason,
                        true,
                        true,
                        true,
                        false,
                        selectedLeft,
                        settings.weaponSpinMult);
                }
                else {
                    ClearHitFrameThrow("no-return weapon spell validation failed before inventory transfer");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            if (ThrowSpellEventMatches(
                event,
                kNoReturnFTGSpellID)) {
                constexpr const char* reason = "0x00081F no-return FTG weapon spell";

                if (StopFTGThrow(reason)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (!ReserveThrowSpell(reason)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (ValidateThrowSpell(
                    player,
                    true,
                    false,
                    false,
                    false,
                    settings,
                    reason)) {
                    const bool selectedLeft =
                        IsThrowWeaponLeft(player, settings);
                    const std::string& animEvent =
                        GetThrowAnimEvent(
                            player,
                            settings,
                            true);

                    QueueThrowSpell(
                        animEvent.c_str(),
                        "ThrowableWeaponsSKSEFTG",
                        true,
                        settings,
                        reason,
                        true,
                        true,
                        false,
                        false,
                        selectedLeft,
                        settings.weaponSpinMult);
                }
                else {
                    ClearHitFrameThrow("FTG weapon spell validation failed before inventory transfer");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            if (ThrowSpellEventMatches(
                event,
                kShieldSpellID)) {
                constexpr const char* reason = "0x000831 shield spell";
                if (!ReserveThrowSpell(reason)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (ValidateThrowSpell(
                    player,
                    false,
                    true,
                    true,
                    false,
                    settings,
                    reason)) {
                    const std::string& animEvent =
                        GetShieldThrowAnim(player, settings);

                    QueueThrowSpell(
                        animEvent.c_str(),
                        "ThrowableWeaponsSKSE",
                        false,
                        settings,
                        reason,
                        false,
                        true,
                        true,
                        true,
                        true);
                }
                else {
                    ClearHitFrameThrow("shield/torch spell validation failed before inventory transfer");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void TriggerWeaponHotkey(
        const ::SaberThrow::Settings::Values& settings)
    {
        constexpr const char* reason = "Throw Weapon hotkey";
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !ReserveThrowSpell(reason)) {
            return;
        }

        if (!ValidateThrowSpell(
                player,
                true,
                false,
                true,
                true,
                settings,
                reason)) {
            ClearHitFrameThrow(
                "Throw Weapon hotkey validation failed before inventory transfer");
            return;
        }

        const bool selectedLeft =
            IsThrowWeaponLeft(player, settings);
        const bool returning = settings.returnOnHit;
        const std::string& animEvent =
            GetThrowAnimEvent(
                player,
                settings,
                !returning);

        QueueThrowSpell(
            animEvent.c_str(),
            "ThrowableWeaponsSKSE",
            !returning,
            settings,
            reason,
            true,
            true,
            true,
            false,
            selectedLeft,
            settings.weaponSpinMult);
    }

    inline void TriggerShieldHotkey(
        const ::SaberThrow::Settings::Values& settings)
    {
        constexpr const char* reason = "Throw Shield hotkey";
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !ReserveThrowSpell(reason)) {
            return;
        }

        if (!ValidateThrowSpell(
                player,
                false,
                true,
                true,
                false,
                settings,
                reason)) {
            ClearHitFrameThrow(
                "Throw Shield hotkey validation failed before inventory transfer");
            return;
        }

        const std::string& animEvent =
            GetShieldThrowAnim(player, settings);

        QueueThrowSpell(
            animEvent.c_str(),
            "ThrowableWeaponsSKSE",
            false,
            settings,
            reason,
            false,
            true,
            true,
            true,
            true);
    }

    inline std::atomic_uint32_t g_weaponHotkeyModifierDown{ 0 };
    inline std::atomic_uint32_t g_weaponGamepadHotkeyModifierDown{ 0 };
    inline std::atomic_uint32_t g_shieldHotkeyModifierDown{ 0 };
    inline std::atomic_uint32_t g_shieldGamepadHotkeyModifierDown{ 0 };

    class SaberThrowHotkeyInputEventSink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!events) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* ui = RE::UI::GetSingleton();
            if (ui && (ui->GameIsPaused() || ui->IsItemMenuOpen() ||
                          ui->IsModalMenuOpen() || ui->IsApplicationMenuOpen())) {
                g_weaponHotkeyModifierDown.store(0, std::memory_order_release);
                g_weaponGamepadHotkeyModifierDown.store(0, std::memory_order_release);
                g_shieldHotkeyModifierDown.store(0, std::memory_order_release);
                g_shieldGamepadHotkeyModifierDown.store(0, std::memory_order_release);
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto settings = ::SaberThrow::Settings::Get();
            const auto gamepadHotkeys = ::SaberThrow::Settings::GetGamepadHotkeys();
            if (!settings.weaponHotkeyOn ||
                !settings.weaponHotkeyUseModifier ||
                settings.weaponHotkeyModifier == 0) {
                g_weaponHotkeyModifierDown.store(0, std::memory_order_release);
            }
            if (!gamepadHotkeys.weaponHotkeyOn ||
                !gamepadHotkeys.weaponHotkeyUseModifier ||
                gamepadHotkeys.weaponHotkeyModifier == 0) {
                g_weaponGamepadHotkeyModifierDown.store(0, std::memory_order_release);
            }
            if (!settings.shieldHotkeyOn ||
                !settings.shieldHotkeyUseModifier ||
                settings.shieldHotkeyModifier == 0) {
                g_shieldHotkeyModifierDown.store(0, std::memory_order_release);
            }
            if (!gamepadHotkeys.shieldHotkeyOn ||
                !gamepadHotkeys.shieldHotkeyUseModifier ||
                gamepadHotkeys.shieldHotkeyModifier == 0) {
                g_shieldGamepadHotkeyModifierDown.store(0, std::memory_order_release);
            }
            if (!settings.weaponHotkeyOn && !gamepadHotkeys.weaponHotkeyOn &&
                !settings.shieldHotkeyOn && !gamepadHotkeys.shieldHotkeyOn) {
                return RE::BSEventNotifyControl::kContinue;
            }

            for (auto* inputEvent = *events; inputEvent; inputEvent = inputEvent->next) {
                if (inputEvent->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                    continue;
                }

                const auto* buttonEvent = inputEvent->AsButtonEvent();
                if (!buttonEvent || (!buttonEvent->IsDown() && !buttonEvent->IsUp())) {
                    continue;
                }

                const bool gamepad = inputEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad;
                std::uint32_t keyCode = buttonEvent->GetIDCode();
                switch (inputEvent->GetDevice()) {
                case RE::INPUT_DEVICE::kKeyboard:
                    break;
                case RE::INPUT_DEVICE::kMouse:
                    keyCode += SKSE::InputMap::kMacro_MouseButtonOffset;
                    break;
                case RE::INPUT_DEVICE::kGamepad:
                    keyCode = SKSE::InputMap::GamepadMaskToKeycode(keyCode);
                    break;
                default:
                    continue;
                }

                if (keyCode >= SKSE::InputMap::kMaxMacros) {
                    continue;
                }

                if (!gamepad && settings.weaponHotkeyUseModifier &&
                    keyCode == settings.weaponHotkeyModifier) {
                    g_weaponHotkeyModifierDown.store(
                        buttonEvent->IsDown() ? keyCode : 0,
                        std::memory_order_release);
                }
                if (gamepad && gamepadHotkeys.weaponHotkeyUseModifier &&
                    keyCode == gamepadHotkeys.weaponHotkeyModifier) {
                    g_weaponGamepadHotkeyModifierDown.store(
                        buttonEvent->IsDown() ? keyCode : 0,
                        std::memory_order_release);
                }
                if (!gamepad && settings.shieldHotkeyUseModifier &&
                    keyCode == settings.shieldHotkeyModifier) {
                    g_shieldHotkeyModifierDown.store(
                        buttonEvent->IsDown() ? keyCode : 0,
                        std::memory_order_release);
                }
                if (gamepad && gamepadHotkeys.shieldHotkeyUseModifier &&
                    keyCode == gamepadHotkeys.shieldHotkeyModifier) {
                    g_shieldGamepadHotkeyModifierDown.store(
                        buttonEvent->IsDown() ? keyCode : 0,
                        std::memory_order_release);
                }

                if (!buttonEvent->IsDown()) {
                    continue;
                }

                if (!gamepad && settings.weaponHotkeyOn &&
                    settings.weaponHotkey != 0 &&
                    keyCode == settings.weaponHotkey &&
                    (!settings.weaponHotkeyUseModifier ||
                        (settings.weaponHotkeyModifier != 0 &&
                            g_weaponHotkeyModifierDown.load(std::memory_order_acquire) ==
                                settings.weaponHotkeyModifier))) {
                    TriggerWeaponHotkey(settings);
                    break;
                }

                if (gamepad && gamepadHotkeys.weaponHotkeyOn &&
                    gamepadHotkeys.weaponHotkey != 0 &&
                    keyCode == gamepadHotkeys.weaponHotkey &&
                    (!gamepadHotkeys.weaponHotkeyUseModifier ||
                        (gamepadHotkeys.weaponHotkeyModifier != 0 &&
                            g_weaponGamepadHotkeyModifierDown.load(std::memory_order_acquire) ==
                                gamepadHotkeys.weaponHotkeyModifier))) {
                    TriggerWeaponHotkey(settings);
                    break;
                }

                if (!gamepad && settings.shieldHotkeyOn &&
                    settings.shieldHotkey != 0 &&
                    keyCode == settings.shieldHotkey &&
                    (!settings.shieldHotkeyUseModifier ||
                        (settings.shieldHotkeyModifier != 0 &&
                            g_shieldHotkeyModifierDown.load(std::memory_order_acquire) ==
                                settings.shieldHotkeyModifier))) {
                    TriggerShieldHotkey(settings);
                    break;
                }

                if (gamepad && gamepadHotkeys.shieldHotkeyOn &&
                    gamepadHotkeys.shieldHotkey != 0 &&
                    keyCode == gamepadHotkeys.shieldHotkey &&
                    (!gamepadHotkeys.shieldHotkeyUseModifier ||
                        (gamepadHotkeys.shieldHotkeyModifier != 0 &&
                            g_shieldGamepadHotkeyModifierDown.load(std::memory_order_acquire) ==
                                gamepadHotkeys.shieldHotkeyModifier))) {
                    TriggerShieldHotkey(settings);
                    break;
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline SaberThrowHotkeyInputEventSink g_hotkeySink{};
    inline std::atomic_bool g_hotkeyInstalled{ false };

    inline void AddThrowHotkeySink()
    {
        bool expected = false;
        if (!g_hotkeyInstalled.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;
        }

        auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
        if (!inputManager) {
            g_hotkeyInstalled.store(false, std::memory_order_release);
            SKSE::log::warn(
                "[SaberThrow] failed to install hotkey input sink: no BSInputDeviceManager.");
            return;
        }

        inputManager->AddEventSink(&g_hotkeySink);
        SKSE::log::info(
            "[SaberThrow] installed hotkey input sink for Throw Weapon and Throw Shield.");
    }

    inline SaberThrowEquipEventSink g_equipSink{};
    inline SaberThrowSpellCastEventSink g_spellCastSink{};
    inline std::atomic_bool g_spellSinkReady{ false };

    inline void AddThrowSpellSink()
    {
        bool expected = false;
        if (!g_spellSinkReady.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            return;
        }

        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            g_spellSinkReady.store(false, std::memory_order_release);
            SKSE::log::warn("[SaberThrow] failed to install TESSpellCastEvent sink: no ScriptEventSourceHolder.");
            return;
        }

        holder->AddEventSink<RE::TESEquipEvent>(&g_equipSink);
        holder->AddEventSink<RE::TESSpellCastEvent>(&g_spellCastSink);
        CachePowerCooldown("spell-cast/equip event sink install/startup cache");
        PrimePowerCooldown(
            "spell-cast/equip event sink install/current selected player power");

        SKSE::log::info(
            "[SaberThrow] installed TESEquipEvent/TESSpellCastEvent sinks for madSaberThrow.esp spell triggers: 0x800, 0x80C, 0x81F, 0x831.");
    }


}

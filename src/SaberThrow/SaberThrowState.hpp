#pragma once

#include "SaberThrow/SaberThrowAim.hpp"

namespace SaberThrow
{
    enum class Phase : std::uint8_t
    {
        Idle,
        Snap,
        Out,
        Back
    };

    struct ThrowPoisonState
    {
        RE::AlchemyItem* poison{ nullptr };
        RE::TESObjectWEAP* sourceWeapon{ nullptr };
        bool wasLeftHand{ false };

        [[nodiscard]] bool HasPoison() const
        {
            return poison && poison->IsPoison();
        }
    };

    struct EquippedWeaponPoisonExtra
    {
        RE::TESObjectWEAP* weapon{ nullptr };
        RE::AlchemyItem* poison{ nullptr };
        RE::InventoryChanges* inventoryChanges{ nullptr };
        RE::ExtraDataList* extraList{ nullptr };
        RE::ExtraPoison* poisonExtra{ nullptr };
        bool wasLeftHand{ false };

        [[nodiscard]] bool IsValid() const
        {
            return weapon && poison && poison->IsPoison() && inventoryChanges && extraList && poisonExtra && poisonExtra->count > 0;
        }
    };

    struct EquippedWeaponChargeExtra
    {
        RE::TESObjectWEAP* weapon{ nullptr };
        RE::EnchantmentItem* enchantment{ nullptr };
        RE::InventoryChanges* inventoryChanges{ nullptr };
        RE::ExtraDataList* extraList{ nullptr };
        RE::ExtraCharge* chargeExtra{ nullptr };
        float currentCharge{ 0.0f };
        float costPerUse{ 0.0f };
        std::uint16_t maxCharge{ 0 };
        bool wasLeftHand{ false };

        [[nodiscard]] bool IsValid() const
        {
            return weapon && enchantment && inventoryChanges && extraList &&
                maxCharge > 0 && currentCharge > 0.0f && costPerUse > 0.0f;
        }
    };

    struct State
    {
        RE::NiPointer<RE::TESObjectREFR> refHandle{};
        RE::ActorHandle sourceActorHandle{};
        bool npcNoReturn{ false };
        bool npcReturning{ false };
        bool skipPickupReg{ false };
        bool unblockOnStop{ false };
        std::string modName{};
        RE::NiPoint3 snapPos{};
        RE::NiPoint3 farPos{};
        float appliedAimOffset{ 0.0f };
        float snapForwardDist{ 0.0f };
        float snapZOffset{ 0.0f };
        float totalTravel{ 0.0f };
        float stepDist{ 0.15f };
        float spinRateRadTick{ 0.10f };
        float spinZ{ 0.0f };
        float throwPitchRad{ 0.0f };
        float throwYawRad{ 0.0f };
        float visualPitchRad{ 0.0f };
        float visualRollRad{ 0.0f };
        float baseYawRad{ 0.0f };
        RE::NiPoint3 noReturnVelocity{};
        float launchSpeed{ 0.0f };
        std::chrono::steady_clock::time_point noReturnStartTime{};
        std::chrono::steady_clock::time_point downStartTime{};

        RE::BSSoundHandle loopSound{};
        bool loopSoundPlaying{ false };
        std::chrono::steady_clock::time_point loopSoundStart{};

        std::chrono::steady_clock::time_point lastSimTime{};
        float simAccumSec{ 0.0f };

        bool parabolaActive{ false };
        float travelDist{ 0.0f };
        float gravityStartDist{ 0.0f };
        bool gravityEnabled{ false };
        float aimDamping{ 0.0f };
        std::uint32_t session{ 0 };
        std::uint32_t snapTickCount{ 0 };
        std::uint32_t spinTickCount{ 0 };
        std::uint32_t outTickCount{ 0 };
        std::uint32_t backTickCount{ 0 };
        bool stopEventSent{ false };
        std::vector<RE::FormID> damagedActorIDs{};

        std::vector<RE::ActorHandle> animFallbackActors{};
        std::uint8_t animFallbackStage{ 0 };

        bool moveEquipToTemp{ false };

        bool hasThrowHand{ false };
        bool throwHandLeft{ false };

        RE::TESObjectWEAP* weaponToReequip{ nullptr };
        bool weaponUnequipped{ false };
        bool weaponWasLeft{ false };

        ThrowPoisonState throwPoison{};
        float throwTemperMult{ 1.0f };
        RE::EnchantmentItem* throwInstanceEnchantment{ nullptr };

        bool noReturnDynamic{ false };

        bool shieldFullDist{ false };

        ::SaberThrow::Settings::ThrowOrientation visualOrientation{
            ::SaberThrow::Settings::ThrowOrientation::Horizontal
        };

        bool noReturnSpearMode{ kDefaultSpearMode };

        RE::NiMatrix3 spearFixedMatrix{};
        RE::NiPoint3 spearFixedEuler{};
        RE::NiPoint3 spearEuler{};
        bool spearFixedValid{ false };
        bool spearEulerValid{ false };

        RE::NiMatrix3 diagExpectedMatrix{};
        float diagExpectedSpin{ 0.0f };
        std::uint32_t diagLoggedTick{ static_cast<std::uint32_t>(-1) };
        bool diagExpectedValid{ false };

        bool forceReturn{ false };

        Phase phase{ Phase::Idle };
    };

    inline State g_fallbackState{};
    inline State* g_currentState = &g_fallbackState;
    inline std::vector<std::unique_ptr<State>> g_throwStates{};

#define g_state (*::SaberThrow::g_currentState)

    struct ThrownInventoryTransferRecord
    {
        RE::ObjectRefHandle thrownRefHandle{};
        RE::FormID thrownRefFormID{ 0 };
        RE::FormID itemFormID{ 0 };
        RE::MagicItem* boundMagicItem{ nullptr };
        RE::Effect* boundEffect{ nullptr };
        RE::TESBoundObject* boundEffectSource{ nullptr };
        float boundEffectRemaining{ 0.0f };
        float boundEffectMagnitude{ 0.0f };
        std::chrono::steady_clock::time_point boundEffectTime{};
        RE::MagicSystem::CastingSource boundCastingSource{ RE::MagicSystem::CastingSource::kNone };
        bool boundEffectTimed{ false };
        bool boundEffectDual{ false };
        bool usedEquippedExtra{ false };
        bool wasLeftHand{ false };
    };

    inline std::mutex g_invTransferLock;
    inline std::vector<ThrownInventoryTransferRecord> g_invTransfers;

    struct BoundThrowRecord
    {
        RE::FormID thrownRefFormID{ 0 };
        RE::FormID sourceWeaponFormID{ 0 };
    };

    inline std::mutex g_boundThrowLock;
    inline std::vector<BoundThrowRecord> g_boundThrows;

    inline RE::TESBoundObject* GetBoundThrowBase(RE::TESObjectWEAP* sourceWeapon)
    {
        if (!sourceWeapon || !sourceWeapon->IsBound()) {
            return nullptr;
        }

        if (sourceWeapon->formEnchanting) {
            for (auto* effect : sourceWeapon->formEnchanting->effects) {
                if (!effect || !effect->baseEffect || !effect->baseEffect->data.enchantEffectArt) {
                    continue;
                }

                auto* enchantArt = effect->baseEffect->data.enchantEffectArt;
                auto* enchantArtModel = static_cast<RE::TESModelTextureSwap*>(enchantArt);
                if (enchantArtModel->model.empty()) {
                    continue;
                }

                return enchantArt;
            }
        }

        if (sourceWeapon->firstPersonModelObject) {
            auto* firstPersonModel = static_cast<RE::TESModelTextureSwap*>(sourceWeapon->firstPersonModelObject);
            if (!firstPersonModel->model.empty()) {
                return sourceWeapon->firstPersonModelObject;
            }
        }

        return nullptr;
    }

    inline void RememberBoundThrow(
        RE::TESObjectREFR* thrownRef,
        RE::TESObjectWEAP* sourceWeapon)
    {
        if (!thrownRef || !sourceWeapon) {
            return;
        }

        BoundThrowRecord record{};
        record.thrownRefFormID = thrownRef->GetFormID();
        record.sourceWeaponFormID = sourceWeapon->GetFormID();

        std::scoped_lock lock(g_boundThrowLock);
        auto it = std::find_if(
            g_boundThrows.begin(),
            g_boundThrows.end(),
            [&](const BoundThrowRecord& entry) {
                return entry.thrownRefFormID == record.thrownRefFormID;
            });

        if (it != g_boundThrows.end()) {
            *it = record;
        }
        else {
            g_boundThrows.push_back(record);
        }
    }

    inline RE::TESObjectWEAP* GetBoundThrowInvItem(RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef) {
            return nullptr;
        }

        RE::FormID sourceWeaponFormID = 0;
        {
            std::scoped_lock lock(g_boundThrowLock);
            const auto it = std::find_if(
                g_boundThrows.begin(),
                g_boundThrows.end(),
                [&](const BoundThrowRecord& entry) {
                    return entry.thrownRefFormID == thrownRef->GetFormID();
                });

            if (it != g_boundThrows.end()) {
                sourceWeaponFormID = it->sourceWeaponFormID;
            }
        }

        return sourceWeaponFormID ?
            RE::TESForm::LookupByID<RE::TESObjectWEAP>(sourceWeaponFormID) :
            nullptr;
    }

    inline void RemoveBoundThrowRecord(RE::FormID thrownRefFormID)
    {
        if (thrownRefFormID == 0) {
            return;
        }

        std::scoped_lock lock(g_boundThrowLock);
        g_boundThrows.erase(
            std::remove_if(
                g_boundThrows.begin(),
                g_boundThrows.end(),
                [thrownRefFormID](const BoundThrowRecord& entry) {
                    return entry.thrownRefFormID == thrownRefFormID;
                }),
            g_boundThrows.end());
    }

    inline void ClearBoundThrowMemory()
    {
        std::scoped_lock lock(g_boundThrowLock);
        g_boundThrows.clear();
    }

    struct NPCDroppedWeaponRecoveryRecord
    {
        std::uint32_t recoveryID{ 0 };
        std::uint32_t loadShutdownGen{ 0 };
        RE::ActorHandle sourceActorHandle{};
        RE::FormID sourceActorFormID{ 0 };
        RE::ObjectRefHandle droppedRefHandle{};
        RE::FormID droppedRefFormID{ 0 };
        RE::FormID weaponFormID{ 0 };
        bool pathRequested{ false };
        std::chrono::steady_clock::time_point expiresAt{};
    };

    inline std::mutex g_npcDropLock;
    inline std::vector<NPCDroppedWeaponRecoveryRecord> g_npcDropRecords;
    inline std::atomic_uint32_t g_nextNPCDropID{ 1 };
    inline std::atomic_bool g_npcDropMonitor{ false };
    inline std::atomic_bool g_npcDropQueued{ false };

    inline constexpr float kNPCDropDist = 200.0f;
    inline constexpr float kNPCDropDistSq =
        kNPCDropDist * kNPCDropDist;
    inline constexpr std::uint32_t kNPCDropMaxSec = 30;
    inline constexpr std::uint32_t kNPCDropPollMs = 100;
    inline constexpr float kNPCDropRunPct = 1.0f;

    inline std::atomic_bool g_pickupMonitor{ false };
    inline std::atomic_bool g_pickupPending{ false };
    inline std::atomic_bool g_invRecoveryPending{ false };

    using ThrowInventoryPostLoadObserver = void (*)();
    inline ThrowInventoryPostLoadObserver g_postLoadObserver{ nullptr };

    inline void SetInvLoadObserver(
        ThrowInventoryPostLoadObserver observer) noexcept
    {
        g_postLoadObserver = observer;
    }

    inline thread_local bool g_suppressLogs{ false };

    [[nodiscard]] inline bool AreLoadScreenCleanupLogsSuppressed() noexcept
    {
        return g_suppressLogs;
    }

    class ScopedLoadScreenCleanupLogSuppression
    {
    public:
        ScopedLoadScreenCleanupLogSuppression() noexcept :
            previous_(g_suppressLogs)
        {
            g_suppressLogs = true;
        }

        ~ScopedLoadScreenCleanupLogSuppression()
        {
            g_suppressLogs = previous_;
        }

        ScopedLoadScreenCleanupLogSuppression(const ScopedLoadScreenCleanupLogSuppression&) = delete;
        ScopedLoadScreenCleanupLogSuppression& operator=(const ScopedLoadScreenCleanupLogSuppression&) = delete;

    private:
        bool previous_{ false };
    };

    inline constexpr float kDropPickupDist = 200.0f;
    inline constexpr float kDropPickupDistSq =
        kDropPickupDist * kDropPickupDist;
    inline constexpr std::uint32_t kDropPickupPollMs = 250;

    inline constexpr RE::FormID kLoopSoundLocalID = 0x000809;
    inline constexpr RE::FormID kLoopSoundFullID = 0xFE000809;
    inline constexpr const char* kLoopSoundPlugin = "madSaberThrow.esp";
    inline constexpr float kLoopSoundMaxSec = 10.0f;

    inline constexpr RE::FormID kPickupListID = 0x000827;
    inline constexpr RE::FormID kPickupListFullID = 0xFE000827;
    inline constexpr const char* kPickupListPlugin = "madSaberThrow.esp";

    inline constexpr RE::FormID kNPCDropListID = 0x000800;
    inline constexpr RE::FormID kNPCDropListFullID = 0xFE001800;
    inline constexpr const char* kNPCDropListPlugin = "madSaberThrow_NPCs.esp";

    inline constexpr RE::FormID kTempLocalID = 0x000805;
    inline constexpr RE::FormID kTempFullID = 0xFE000805;
    inline constexpr const char* kTempPlugin = "madSaberThrow.esp";

    inline constexpr const char* kSkyrimPluginName = "Skyrim.esm";
    inline constexpr RE::FormID kRightEquipSlotID = 0x00013F42;
    inline constexpr RE::FormID kLeftEquipSlotID = 0x00013F43;
    inline constexpr RE::FormID kTorchID = 0x0001D4EC;
    inline constexpr RE::FormID kShadowTorchID = 0x00036343;
    inline constexpr RE::FormID kTorchBashSpellID = 0x000FEAAB;

    inline constexpr const char* kSpellCastPlugin = "madSaberThrow.esp";
    inline constexpr RE::FormID kReturnSpellID = 0x000800;
    inline constexpr RE::FormID kNPCReturnStartID = 0x000837;
    inline constexpr RE::FormID kNPCReturnEndID = 0x000839;
    inline constexpr RE::FormID kNoReturnSpellID = 0x00080C;
    inline constexpr RE::FormID kNoReturnFTGSpellID = 0x00081F;
    inline constexpr RE::FormID kShieldSpellID = 0x000831;

    inline constexpr const char* kPowerCooldownKey = "fMagicLesserPowerCooldownTimer";
    inline std::atomic_bool g_cooldownCached{ false };
    inline std::atomic<float> g_powerCooldown{ 0.0f };
    inline std::atomic_bool g_cooldownOverride{ false };

    inline constexpr const char* kDummyEnchantPlugin = "madSaberThrow.esp";
    inline constexpr RE::FormID kDummyEnchantLocal = 0x000833;
    inline constexpr RE::FormID kDummyEnchantFullID = 0xFE000833;

    inline constexpr const char* kPickupEvent = "OnWeaponPickUpEvent";

    inline void StopLoopSound(const char* reason);
    inline void ClearState(const char* reason);
    inline void DisableDeleteThrownRef(RE::TESObjectREFR* ref, const char* reason);
    inline void ShutdownThrowsForLoad(const char* reason);
    inline void StartPickupMonitor();
    inline void StartNPCRecoveryMonitor();
    inline void AddNPCRecovery(
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef,
        const char* reason);
    inline void ClearNPCRecoveryRecords(const char* reason);
    inline void QueueUpdateTask();
    inline RE::TESObjectWEAP* GetWeaponBase(RE::TESObjectREFR* thrownRef);
    inline bool CachePowerCooldown(const char* reason);
    inline RE::Actor* GetThrowSourceActor();
    inline bool IsThrowFromPlayer();
    inline bool IsThrowFromNPC();
    inline bool IgnoreActorNPCThrow(RE::Actor* actor);
    inline void IgnoreThrowSource(std::vector<RE::NiAVObject*>& ignoreList);
    inline bool GetNPCThrowPoints(RE::Actor* sourceActor, float totalTravel, RE::NiPoint3& outSnapPos, RE::NiPoint3& outFarPos);
    inline bool IsHitFrameThrowWaiting();
    inline void ClearNPCAnimRequests(const char* reason);
    inline void ClearNPCAnimThrowSinks(const char* reason);
    inline void ZeroThrowPowerCooldown(const char* reason);
    inline void PrimePowerCooldown(const char* reason);
    inline bool StartNPCNoReturnRightHandWeaponThrowMainThread(
        RE::Actor* sourceActor,
        float totalTravel,
        float stepDist,
        float spinRateRadTick);
    inline bool StartNPCReturnThrowNow(
        RE::Actor* sourceActor,
        float totalTravel,
        float stepDist,
        float spinRateRadTick);
    inline bool RestoreNPCThrownWeapon(
        RE::Actor* sourceActor,
        RE::TESObjectREFR* droppedRef,
        const char* reason);


    inline RE::BGSListForm* GetPickupTriggerList()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        if (auto* listForm = dataHandler->LookupForm<RE::BGSListForm>(
            kPickupListID,
            kPickupListPlugin)) {
            return listForm;
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kPickupListFullID)) {
            if (auto* listForm = rawForm->As<RE::BGSListForm>()) {
                return listForm;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kPickupListID)) {
            return rawForm->As<RE::BGSListForm>();
        }

        return nullptr;
    }

    inline void AddPickupTriggerRef(
        RE::TESObjectREFR* ref,
        const char* reason)
    {
        if (!ref) {
            return;
        }

        auto* listForm = GetPickupTriggerList();
        if (!listForm) {
            SKSE::log::warn(
                "[SaberThrow] could not resolve active pickup trigger formlist 0x{:06X}/'{}' while stopping no-return throw: {}",
                kPickupListID,
                kPickupListPlugin,
                reason ? reason : "unknown reason");
            return;
        }

        if (!listForm->HasForm(ref)) {
            listForm->AddForm(ref);
        }

        StartPickupMonitor();
    }


    inline RE::Actor* GetThrowSourceActor()
    {
        if (g_state.sourceActorHandle) {
            auto sourcePtr = g_state.sourceActorHandle.get();
            if (sourcePtr) {
                return sourcePtr.get();
            }
        }

        return RE::PlayerCharacter::GetSingleton();
    }

    inline bool IsThrowFromPlayer()
    {
        auto* sourceActor = GetThrowSourceActor();
        return sourceActor && sourceActor->IsPlayerRef();
    }

    inline bool IsThrowFromNPC()
    {
        auto* sourceActor = GetThrowSourceActor();
        return sourceActor && !sourceActor->IsPlayerRef();
    }

    inline float GetFallbackActorProgress()
    {
        if (g_state.phase == Phase::Snap) {
            return 0.0f;
        }
        if (g_state.phase == Phase::Back) {
            return 1.0f;
        }

        const float totalTravel = std::max(g_state.totalTravel, 1e-4f);
        if (g_state.noReturnDynamic && g_state.travelDist > 0.0f) {
            return std::clamp(g_state.travelDist / totalTravel, 0.0f, 1.0f);
        }

        auto* ref = g_state.refHandle.get();
        if (!ref) {
            return 0.0f;
        }

        return std::clamp(
            LengthPoint(SubPoint(ref->GetPosition(), g_state.snapPos)) / totalTravel,
            0.0f,
            1.0f);
    }

    inline void RefreshFallbackActors(RE::Actor* caster)
    {
        g_state.animFallbackActors.clear();

        if (!caster || !caster->IsPlayerRef()) {
            return;
        }

        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) {
            return;
        }

        processLists->ForEachHighActor([&](RE::Actor* actor) {
            if (!actor || actor == caster || actor->GetParentCell() != caster->GetParentCell()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            if (!NeedsAnimBoundsFallback(*actor)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            g_state.animFallbackActors.emplace_back(actor);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    inline void RefreshFallbackActorsDue()
    {
        if (!IsThrowFromPlayer() ||
            g_state.animFallbackStage >= 3) {
            return;
        }

        const float progress = GetFallbackActorProgress();
        bool refreshNow = false;

        if (g_state.animFallbackStage == 0) {
            refreshNow = true;
        }
        else if (g_state.animFallbackStage == 1 && progress >= 0.50f) {
            refreshNow = true;
        }
        else if (g_state.animFallbackStage == 2 && progress >= 0.90f) {
            refreshNow = true;
        }

        if (!refreshNow) {
            return;
        }

        auto* caster = GetThrowSourceActor();
        RefreshFallbackActors(caster);

        if (progress >= 0.90f) {
            g_state.animFallbackStage = 3;
        }
        else if (progress >= 0.50f) {
            g_state.animFallbackStage = 2;
        }
        else {
            g_state.animFallbackStage = 1;
        }
    }

    inline bool ForEachFallbackActor(
        RE::Actor* caster,
        const AnimatedActorFallbackVisitor& visitor)
    {
        if (!caster || !caster->IsPlayerRef() || !visitor) {
            return false;
        }

        bool visitedAny = false;
        for (auto& handle : g_state.animFallbackActors) {
            auto actorPtr = handle.get();
            auto* actor = actorPtr ? actorPtr.get() : nullptr;
            if (!actor || actor == caster || actor->GetParentCell() != caster->GetParentCell()) {
                continue;
            }

            if (!NeedsAnimBoundsFallback(*actor)) {
                continue;
            }

            visitedAny = true;
            if (visitor(actor) == RE::BSContainer::ForEachResult::kStop) {
                break;
            }
        }

        return visitedAny;
    }

    inline void IgnoreThrowSource(std::vector<RE::NiAVObject*>& ignoreList)
    {
        auto* sourceActor = GetThrowSourceActor();
        if (!sourceActor) {
            return;
        }

        AddIgnoreNode(ignoreList, sourceActor->Get3D(false));
        AddIgnoreNode(ignoreList, sourceActor->Get3D(true));
    }

    inline RE::TESObjectWEAP* GetActorRHEquipWeapon(RE::Actor* actor)
    {
        if (!actor) {
            return nullptr;
        }

        if (auto* rightObj = actor->GetEquippedObject(false)) {
            return rightObj->As<RE::TESObjectWEAP>();
        }

        return nullptr;
    }

    inline constexpr float kNPCAimZOffset = 50.0f;

    inline bool GetNPCThrowPoints(
        RE::Actor* sourceActor,
        float totalTravel,
        RE::NiPoint3& outSnapPos,
        RE::NiPoint3& outFarPos)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!sourceActor || !player || sourceActor == player) {
            return false;
        }

        outSnapPos = GetHandThrowSource(sourceActor, false);

        RE::NiPoint3 target = player->GetPosition();
        target.z += kNPCAimZOffset;

        RE::NiPoint3 aimDir = SubPoint(target, outSnapPos);
        if (!NormalizePoint(aimDir)) {
            return false;
        }

        outFarPos = AddPoint(outSnapPos, MulPoint(aimDir, std::max(totalTravel, 1.0f)));
        return true;
    }

    inline RE::TESObjectREFR* GetThrowTemp()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        if (auto* tempContainer = dataHandler->LookupForm<RE::TESObjectREFR>(
            kTempLocalID,
            kTempPlugin)) {
            return tempContainer;
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kTempFullID)) {
            return rawForm->As<RE::TESObjectREFR>();
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kTempLocalID)) {
            return rawForm->As<RE::TESObjectREFR>();
        }

        return nullptr;
    }

    inline RE::ActiveEffect* GetBoundThrowEffect(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* weapon,
        bool wasLeftHand)
    {
        auto* magicTarget = player ? player->AsMagicTarget() : nullptr;
        auto* activeEffects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!activeEffects || !weapon) {
            return nullptr;
        }

        const auto wantedSource = wasLeftHand ?
            RE::MagicSystem::CastingSource::kLeftHand :
            RE::MagicSystem::CastingSource::kRightHand;

        RE::ActiveEffect* handMatch = nullptr;
        RE::ActiveEffect* fallback = nullptr;
        float handRemaining = -1.0f;
        float fallbackRemaining = -1.0f;

        for (auto* activeEffect : *activeEffects) {
            if (!activeEffect ||
                !activeEffect->spell ||
                !activeEffect->effect ||
                !activeEffect->effect->baseEffect ||
                activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
                continue;
            }

            auto* baseEffect = activeEffect->effect->baseEffect;
            if (baseEffect->GetArchetype() != RE::EffectArchetypes::ArchetypeID::kBoundWeapon ||
                baseEffect->data.associatedForm != weapon) {
                continue;
            }

            const bool timed = !baseEffect->data.flags.all(
                RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
            const float remaining = timed ?
                std::max(0.0f, activeEffect->duration - activeEffect->elapsedSeconds) :
                activeEffect->duration;

            if (activeEffect->castingSource == wantedSource) {
                if (!handMatch || remaining > handRemaining) {
                    handMatch = activeEffect;
                    handRemaining = remaining;
                }
            }
            else if (!fallback || remaining > fallbackRemaining) {
                fallback = activeEffect;
                fallbackRemaining = remaining;
            }
        }

        return handMatch ? handMatch : fallback;
    }

    inline void RememberBoundThrowEffect(
        ThrownInventoryTransferRecord& record,
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* weapon)
    {
        auto* activeEffect = GetBoundThrowEffect(player, weapon, record.wasLeftHand);
        if (!activeEffect || !activeEffect->effect || !activeEffect->effect->baseEffect) {
            return;
        }

        auto* baseEffect = activeEffect->effect->baseEffect;
        record.boundMagicItem = activeEffect->spell;
        record.boundEffect = activeEffect->effect;
        record.boundEffectSource = activeEffect->source;
        record.boundEffectMagnitude = activeEffect->magnitude;
        record.boundCastingSource = activeEffect->castingSource;
        record.boundEffectDual = activeEffect->flags.all(RE::ActiveEffect::Flag::kDual);
        record.boundEffectTimed = !baseEffect->data.flags.all(
            RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
        record.boundEffectRemaining = record.boundEffectTimed ?
            std::max(0.0f, activeEffect->duration - activeEffect->elapsedSeconds) :
            activeEffect->duration;
        record.boundEffectTime = std::chrono::steady_clock::now();

    }

    inline bool RestoreBoundThrowEffect(
        const ThrownInventoryTransferRecord& record,
        RE::PlayerCharacter* player,
        const char* reason)
    {
        (void)reason;
        if (!player || !record.boundMagicItem || !record.boundEffect) {
            return false;
        }

        float remaining = record.boundEffectRemaining;
        if (record.boundEffectTimed &&
            record.boundEffectTime != std::chrono::steady_clock::time_point{}) {
            remaining -= std::chrono::duration<float>(
                std::chrono::steady_clock::now() - record.boundEffectTime).count();

            if (remaining <= 0.0f) {
                return true;
            }
        }

        auto* magicTarget = player->AsMagicTarget();
        auto* activeEffects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!magicTarget || !activeEffects) {
            return false;
        }

        for (auto* activeEffect : *activeEffects) {
            if (activeEffect &&
                activeEffect->spell == record.boundMagicItem &&
                activeEffect->effect == record.boundEffect &&
                activeEffect->castingSource == record.boundCastingSource &&
                !activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
                return true;
            }
        }

        RE::MagicTarget::AddTargetData data{};
        data.caster = player;
        data.magicItem = record.boundMagicItem;
        data.effect = record.boundEffect;
        data.source = record.boundEffectSource;
        data.magnitude = record.boundEffectMagnitude;
        data.power = 1.0f;
        data.castingSource = record.boundCastingSource;
        data.areaTarget = false;
        data.dualCasted = record.boundEffectDual;

        if (!magicTarget->AddTarget(data)) {
            return false;
        }

        activeEffects = magicTarget->GetActiveEffectList();
        RE::ActiveEffect* restoredEffect = nullptr;
        if (activeEffects) {
            for (auto* activeEffect : *activeEffects) {
                if (activeEffect &&
                    activeEffect->spell == record.boundMagicItem &&
                    activeEffect->effect == record.boundEffect &&
                    activeEffect->castingSource == record.boundCastingSource &&
                    !activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
                    restoredEffect = activeEffect;
                    break;
                }
            }
        }

        if (restoredEffect) {
            restoredEffect->magnitude = record.boundEffectMagnitude;
            if (record.boundEffectTimed) {
                restoredEffect->duration = remaining;
                restoredEffect->elapsedSeconds = 0.0f;
            }
        }

        return true;
    }

    inline void RememberThrowInv(
        RE::TESObjectREFR* thrownRef,
        RE::TESBoundObject* item,
        bool usedEquippedExtra,
        bool wasLeftHand)
    {
        if (!thrownRef || !item) {
            return;
        }

        ThrownInventoryTransferRecord record{};
        record.thrownRefHandle = thrownRef->GetHandle();
        record.thrownRefFormID = thrownRef->GetFormID();
        record.itemFormID = item->GetFormID();
        record.usedEquippedExtra = usedEquippedExtra;
        record.wasLeftHand = wasLeftHand;

        if (auto* weapon = item->As<RE::TESObjectWEAP>(); weapon && weapon->IsBound()) {
            RememberBoundThrowEffect(
                record,
                RE::PlayerCharacter::GetSingleton(),
                weapon);
        }

        std::scoped_lock lock(g_invTransferLock);

        auto existing = std::find_if(
            g_invTransfers.begin(),
            g_invTransfers.end(),
            [&](const ThrownInventoryTransferRecord& entry) {
                return entry.thrownRefFormID == record.thrownRefFormID;
            });

        if (existing != g_invTransfers.end()) {
            *existing = record;
        }
        else {
            g_invTransfers.push_back(record);
        }
    }

    inline void ClearThrowInvMemory(const char* reason)
    {
        std::scoped_lock lock(g_invTransferLock);
        if (!g_invTransfers.empty() && !AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] clearing {} thrown inventory transfer record(s): {}",
                g_invTransfers.size(),
                reason ? reason : "unknown");
        }
        g_invTransfers.clear();
        ClearBoundThrowMemory();
    }

    inline bool HasThrowInvRecords()
    {
        std::scoped_lock lock(g_invTransferLock);
        return !g_invTransfers.empty();
    }

    inline std::vector<ThrownInventoryTransferRecord> SnapshotThrowInvRecords()
    {
        std::scoped_lock lock(g_invTransferLock);
        return g_invTransfers;
    }

    inline bool GetThrowInvRecord(
        RE::FormID thrownRefFormID,
        ThrownInventoryTransferRecord& outRecord)
    {
        if (thrownRefFormID == 0) {
            return false;
        }

        std::scoped_lock lock(g_invTransferLock);
        const auto it = std::find_if(
            g_invTransfers.begin(),
            g_invTransfers.end(),
            [thrownRefFormID](const ThrownInventoryTransferRecord& entry) {
                return entry.thrownRefFormID == thrownRefFormID;
            });

        if (it == g_invTransfers.end()) {
            return false;
        }

        outRecord = *it;
        return true;
    }

    inline bool RemoveThrowInvRecord(RE::FormID thrownRefFormID)
    {
        if (thrownRefFormID == 0) {
            return false;
        }

        std::scoped_lock lock(g_invTransferLock);
        const auto oldSize = g_invTransfers.size();

        g_invTransfers.erase(
            std::remove_if(
                g_invTransfers.begin(),
                g_invTransfers.end(),
                [thrownRefFormID](const ThrownInventoryTransferRecord& entry) {
                    return entry.thrownRefFormID == thrownRefFormID;
                }),
            g_invTransfers.end());

        const bool removed = g_invTransfers.size() != oldSize;
        if (removed) {
            RemoveBoundThrowRecord(thrownRefFormID);
        }
        return removed;
    }

    inline RE::BGSEquipSlot* GetHandEquipSlot(bool leftHand)
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        return dataHandler->LookupForm<RE::BGSEquipSlot>(
            leftHand ? kLeftEquipSlotID : kRightEquipSlotID,
            kSkyrimPluginName);
    }

    inline bool IsShieldForTransfer(RE::TESObjectARMO* armor)
    {
        return armor &&
            (armor->formFlags & RE::TESObjectARMO::RecordFlags::kShield) != 0;
    }

    inline bool IsThrowTorchBase(RE::TESBoundObject* item)
    {
        if (!item) {
            return false;
        }

        const RE::FormID formID = item->GetFormID();
        return formID == kTorchID ||
            formID == kShadowTorchID;
    }

    inline RE::BGSEquipSlot* GetEquipSlotThrownHand(
        RE::TESBoundObject* item,
        bool leftHand)
    {
        if (!item) {
            return nullptr;
        }

        if (item->As<RE::TESObjectWEAP>()) {
            return GetHandEquipSlot(leftHand);
        }

        if (auto* armor = item->As<RE::TESObjectARMO>(); IsShieldForTransfer(armor)) {
            return GetHandEquipSlot(true);
        }

        if (IsThrowTorchBase(item)) {
            return GetHandEquipSlot(true);
        }

        return nullptr;
    }

    inline bool PreferLHThrowWeapon(const ::SaberThrow::Settings::Values& settings)
    {
        return settings.preferLeftHand;
    }

    inline RE::TESBoundObject* GetThrownRefInvItem(
        RE::TESObjectREFR* thrownRef,
        bool& outLeftHand,
        const char*& outLabel)
    {
        outLeftHand = false;
        outLabel = "right-hand weapon";

        if (!thrownRef) {
            return nullptr;
        }

        RE::TESBoundObject* baseObj = GetBoundThrowInvItem(thrownRef);
        if (!baseObj) {
            baseObj = thrownRef->GetObjectReference();
        }
        if (!baseObj) {
            return nullptr;
        }

        if (auto* weapon = baseObj->As<RE::TESObjectWEAP>()) {
            const RE::FormID weaponFormID = weapon->GetFormID();
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                const auto settings = ::SaberThrow::Settings::Get();
                const bool preferLeftHand = PreferLHThrowWeapon(settings);

                auto tryEquipMatch = [&](bool leftHand) -> bool {
                    if (auto* equippedObj = player->GetEquippedObject(leftHand);
                        equippedObj && equippedObj->GetFormID() == weaponFormID) {
                        outLeftHand = leftHand;
                        outLabel = leftHand ? "left-hand weapon" : "right-hand weapon";
                        return true;
                    }

                    return false;
                    };

                if (preferLeftHand) {
                    if (tryEquipMatch(true) || tryEquipMatch(false)) {
                        return weapon;
                    }
                }
                else {
                    if (tryEquipMatch(false) || tryEquipMatch(true)) {
                        return weapon;
                    }
                }
            }

            outLeftHand = false;
            outLabel = "right-hand weapon";
            return weapon;
        }

        if (auto* armor = baseObj->As<RE::TESObjectARMO>(); IsShieldForTransfer(armor)) {
            outLeftHand = true;
            outLabel = "equipped shield";
            return armor;
        }

        if (IsThrowTorchBase(baseObj)) {
            outLeftHand = true;
            outLabel = "equipped torch";
            return baseObj;
        }

        return nullptr;
    }

    inline RE::ExtraDataList* FindPlayerThrowExtra(
        RE::Actor* player,
        RE::TESBoundObject* item,
        bool leftHand)
    {
        if (!player || !item) {
            return nullptr;
        }

        auto* inventoryChanges = player->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return nullptr;
        }

        RE::ExtraDataList* fallbackExtra = nullptr;

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || entry->object != item || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (!extraList) {
                    continue;
                }

                if (leftHand) {
                    if (extraList->HasType<RE::ExtraWornLeft>()) {
                        return extraList;
                    }
                    if (!fallbackExtra && extraList->HasType<RE::ExtraWorn>()) {
                        fallbackExtra = extraList;
                    }
                }
                else {
                    if (extraList->HasType<RE::ExtraWorn>()) {
                        return extraList;
                    }
                    if (!fallbackExtra && extraList->HasType<RE::ExtraWornLeft>()) {
                        fallbackExtra = extraList;
                    }
                }
            }
        }

        return fallbackExtra;
    }

    inline RE::TESBoundObject* FindInvItemByID(
        RE::TESObjectREFR* owner,
        RE::FormID baseFormID,
        std::int32_t* outCount = nullptr)
    {
        if (outCount) {
            *outCount = 0;
        }

        if (!owner || baseFormID == 0) {
            return nullptr;
        }

        const auto inventoryCounts = owner->GetInventoryCounts();
        for (const auto& [candidate, count] : inventoryCounts) {
            if (!candidate || count <= 0) {
                continue;
            }

            if (candidate->GetFormID() == baseFormID) {
                if (outCount) {
                    *outCount = count;
                }
                return candidate;
            }
        }

        return nullptr;
    }

    inline RE::ExtraDataList* FindInvExtraForID(
        RE::TESObjectREFR* owner,
        RE::FormID baseFormID)
    {
        if (!owner || baseFormID == 0) {
            return nullptr;
        }

        auto* inventoryChanges = owner->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return nullptr;
        }

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || !entry->object || entry->object->GetFormID() != baseFormID || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (extraList) {
                    return extraList;
                }
            }
        }

        return nullptr;
    }

    inline RE::ExtraDataList* FindUnequippedExtra(
        RE::TESObjectREFR* owner,
        RE::FormID baseFormID)
    {
        if (!owner || baseFormID == 0) {
            return nullptr;
        }

        auto* inventoryChanges = owner->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return nullptr;
        }

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || !entry->object || entry->object->GetFormID() != baseFormID || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (!extraList ||
                    extraList->HasType<RE::ExtraWorn>() ||
                    extraList->HasType<RE::ExtraWornLeft>()) {
                    continue;
                }

                return extraList;
            }
        }

        return nullptr;
    }

    inline bool ShouldReequipStack(
        RE::TESBoundObject* removedItem)
    {
        const auto settings = ::SaberThrow::Settings::Get();
        if (!settings.equipNextStack || !removedItem) {
            return false;
        }

        if (!removedItem->As<RE::TESObjectWEAP>() ||
            removedItem->As<RE::TESObjectARMO>() ||
            IsThrowTorchBase(removedItem)) {
            return false;
        }

        return g_state.noReturnDynamic &&
            g_state.modName == "ThrowableWeaponsSKSE" &&
            !g_state.shieldFullDist;
    }

    inline bool EquipRemainingStack(
        RE::PlayerCharacter* player,
        RE::TESBoundObject* removedItem,
        bool leftHand,
        const char* itemLabel,
        const char* reason)
    {
        if (!player || !removedItem) {
            return false;
        }

        const RE::FormID baseFormID = removedItem->GetFormID();
        std::int32_t remainingCount = 0;
        auto* remainingItem = FindInvItemByID(player, baseFormID, &remainingCount);
        if (!remainingItem || remainingCount <= 0) {
            SKSE::log::debug(
                "[SaberThrow] no remaining stack copy to re-equip for {} 0x{:08X} after temp transfer: {}",
                itemLabel ? itemLabel : "equipped item",
                baseFormID,
                reason ? reason : "throw");
            return false;
        }

        if (player->GetEquippedObject(leftHand) && player->GetEquippedObject(leftHand)->GetFormID() == baseFormID) {
            SKSE::log::debug(
                "[SaberThrow] remaining stack copy already equipped for {} 0x{:08X} after temp transfer: {}",
                itemLabel ? itemLabel : "equipped item",
                baseFormID,
                reason ? reason : "throw");
            return true;
        }

        std::int32_t equipMatchCount = 0;
        if (auto* rightEquipped = player->GetEquippedObject(false);
            rightEquipped && rightEquipped->GetFormID() == baseFormID) {
            ++equipMatchCount;
        }
        if (auto* leftEquipped = player->GetEquippedObject(true);
            leftEquipped && leftEquipped->GetFormID() == baseFormID) {
            ++equipMatchCount;
        }

        if (remainingCount <= equipMatchCount) {
            SKSE::log::debug(
                "[SaberThrow] no unequipped stack copy available for {} 0x{:08X}; remainingCount={}, equippedMatchingCount={}, requestedHand={}: {}",
                itemLabel ? itemLabel : "equipped item",
                baseFormID,
                remainingCount,
                equipMatchCount,
                leftHand ? "left" : "right",
                reason ? reason : "throw");
            return false;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return false;
        }

        auto* remainingExtra = FindUnequippedExtra(
            player,
            baseFormID);
        auto* equipSlot = GetEquipSlotThrownHand(remainingItem, leftHand);

        constexpr bool kQueueEquip = false;
        constexpr bool kForceEquip = false;
        constexpr bool kPlaySounds = false;
        constexpr bool kApplyNow = true;

        equipManager->EquipObject(
            player,
            remainingItem,
            remainingExtra,
            1,
            equipSlot,
            kQueueEquip,
            kForceEquip,
            kPlaySounds,
            kApplyNow);

        auto* equippedAfter = player->GetEquippedObject(leftHand);
        const bool inRequestedHand =
            equippedAfter && equippedAfter->GetFormID() == baseFormID;

        if (!inRequestedHand) {
            SKSE::log::warn(
                "[SaberThrow] stack copy equip did not populate requested {} hand for {} base 0x{:08X}; count={}, extraList={}, slot={}: {}",
                leftHand ? "left" : "right",
                itemLabel ? itemLabel : "equipped item",
                baseFormID,
                remainingCount,
                remainingExtra != nullptr,
                equipSlot ? equipSlot->GetFormID() : 0,
                reason ? reason : "throw");
            return false;
        }

        SKSE::log::debug(
            "[SaberThrow] re-equipped remaining stack copy for {} base 0x{:08X} after temp transfer; count={}, extraList={}, hand={}, slot={}: {}",
            itemLabel ? itemLabel : "equipped item",
            baseFormID,
            remainingCount,
            remainingExtra != nullptr,
            leftHand ? "left" : "right",
            equipSlot ? equipSlot->GetFormID() : 0,
            reason ? reason : "throw");

        return true;
    }

    inline bool MoveThrowItemToTemp(
        RE::TESObjectREFR* thrownRef,
        const char* reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !thrownRef) {
            return false;
        }

        bool leftHand = false;
        const char* itemLabel = "equipped item";
        auto* item = GetThrownRefInvItem(thrownRef, leftHand, itemLabel);
        if (!item) {
            SKSE::log::debug(
                "[SaberThrow] temp inventory transfer skipped: thrown ref 0x{:08X} is not a weapon, shield armor, or torch for {}",
                thrownRef->GetFormID(),
                reason ? reason : "throw");
            return false;
        }

        if (g_state.hasThrowHand && item->As<RE::TESObjectWEAP>()) {
            const bool inferredLeftHand = leftHand;
            leftHand = g_state.throwHandLeft;
            itemLabel = leftHand ? "left-hand weapon" : "right-hand weapon";

            if (inferredLeftHand != leftHand) {
                SKSE::log::debug(
                    "[SaberThrow] overriding inferred thrown hand from {} to {} using queued animation hand for item 0x{:08X}.",
                    inferredLeftHand ? "left" : "right",
                    leftHand ? "left" : "right",
                    item->GetFormID());
            }
        }

        auto* equippedExtraList = FindPlayerThrowExtra(player, item, leftHand);

        if (auto* weapon = item->As<RE::TESObjectWEAP>(); weapon && weapon->IsBound()) {
            RememberThrowInv(thrownRef, item, equippedExtraList != nullptr, leftHand);

            player->RemoveItem(
                item,
                1,
                RE::ITEM_REMOVE_REASON::kRemove,
                equippedExtraList,
                nullptr);

            return true;
        }

        auto* tempContainer = GetThrowTemp();
        if (!tempContainer) {
            SKSE::log::warn(
                "[SaberThrow] temp inventory transfer failed: could not resolve temp container 0x{:06X}/'{}'",
                kTempLocalID,
                kTempPlugin);
            return false;
        }

        player->RemoveItem(
            item,
            1,
            RE::ITEM_REMOVE_REASON::kStoreInContainer,
            equippedExtraList,
            tempContainer);

        if (ShouldReequipStack(item)) {
            EquipRemainingStack(
                player,
                item,
                leftHand,
                itemLabel,
                reason);
        }
        else {
            SKSE::log::debug(
                "[SaberThrow] skipped remaining stack copy re-equip for {} 0x{:08X}; modName='{}' noReturn={} shieldHybrid={}: {}",
                itemLabel ? itemLabel : "equipped item",
                item->GetFormID(),
                g_state.modName,
                g_state.noReturnDynamic,
                g_state.shieldFullDist,
                reason ? reason : "throw");
        }

        RememberThrowInv(thrownRef, item, equippedExtraList != nullptr, leftHand);

        SKSE::log::debug(
            "[SaberThrow] moved {} 0x{:08X} to temp container for thrown ref 0x{:08X}{}",
            itemLabel ? itemLabel : "equipped item",
            item->GetFormID(),
            thrownRef->GetFormID(),
            equippedExtraList ? (leftHand ? " using left-hand equipped extra list" : " using equipped extra list") : "");

        return true;
    }

    inline bool MoveRHItemToTemp(
        RE::TESObjectREFR* thrownRef,
        const char* reason)
    {
        return MoveThrowItemToTemp(thrownRef, reason);
    }

    inline void ReturnThrowTempItems(const char* reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* tempContainer = GetThrowTemp();
        if (!player || !tempContainer) {
            if (!AreLoadScreenCleanupLogsSuppressed()) {
                SKSE::log::debug(
                    "[SaberThrow] temp-container return skipped: player={} tempContainer={} reason={}",
                    player != nullptr,
                    tempContainer != nullptr,
                    reason ? reason : "unknown");
            }
            return;
        }

        tempContainer->InitInventoryIfRequired();

        std::int32_t totalMoved = 0;
        constexpr std::uint32_t kMaxReturnPasses = 16;

        for (std::uint32_t pass = 0; pass < kMaxReturnPasses; ++pass) {
            auto inventoryCounts = tempContainer->GetInventoryCounts();
            bool movedThisPass = false;

            for (const auto& entry : inventoryCounts) {
                auto* item = entry.first;
                const auto count = entry.second;
                if (!item || count <= 0) {
                    continue;
                }

                tempContainer->RemoveItem(
                    item,
                    count,
                    RE::ITEM_REMOVE_REASON::kRemove,
                    nullptr,
                    player);

                totalMoved += count;
                movedThisPass = true;
            }

            if (!movedThisPass) {
                break;
            }
        }

        if (!AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] returned {} item(s) from actual temp container 0x{:08X} to player: {}",
                totalMoved,
                tempContainer->GetFormID(),
                reason ? reason : "unknown");
        }
    }

    inline void RemovePickupTriggerID(
        RE::BGSListForm* listForm,
        RE::FormID formID)
    {
        if (!listForm || formID == 0) {
            return;
        }

        for (auto it = listForm->forms.begin(); it != listForm->forms.end();) {
            auto* form = *it;
            if (form && form->GetFormID() == formID) {
                it = listForm->forms.erase(it);
            }
            else {
                ++it;
            }
        }

        if (listForm->scriptAddedTempForms) {
            for (auto it = listForm->scriptAddedTempForms->begin(); it != listForm->scriptAddedTempForms->end();) {
                if (*it == formID) {
                    it = listForm->scriptAddedTempForms->erase(it);
                }
                else {
                    ++it;
                }
            }

            listForm->scriptAddedFormCount = listForm->scriptAddedTempForms->size();
        }
    }

    inline RE::BGSListForm* GetNPCNoReturnList()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        if (auto* listForm = dataHandler->LookupForm<RE::BGSListForm>(
            kNPCDropListID,
            kNPCDropListPlugin)) {
            return listForm;
        }

        if (auto* rawForm = RE::TESForm::LookupByID(
            kNPCDropListFullID)) {
            if (auto* listForm = rawForm->As<RE::BGSListForm>()) {
                return listForm;
            }
        }

        return nullptr;
    }

    inline void AddNPCNoReturnRef(
        RE::TESObjectREFR* ref,
        const char* reason)
    {
        if (!ref || ref->GetFormID() == 0) {
            return;
        }

        auto* listForm = GetNPCNoReturnList();
        if (!listForm) {
            if (!AreLoadScreenCleanupLogsSuppressed()) {
                SKSE::log::warn(
                    "[SaberThrow] could not resolve NPC no-return dropped-weapon formlist 0x{:06X}/'{}': {}",
                    kNPCDropListID,
                    kNPCDropListPlugin,
                    reason ? reason : "unknown reason");
            }
            return;
        }

        if (!listForm->HasForm(ref)) {
            listForm->AddForm(ref);
        }
    }

    inline void RemoveNPCNoReturnRef(RE::FormID formID)
    {
        if (formID == 0) {
            return;
        }

        RemovePickupTriggerID(
            GetNPCNoReturnList(),
            formID);
    }

    inline void RemoveNPCNoReturnRef(RE::TESObjectREFR* ref)
    {
        if (ref) {
            RemoveNPCNoReturnRef(ref->GetFormID());
        }
    }

    inline void ClearNPCNoReturnRefs(
        const char* reason)
    {
        auto* listForm = GetNPCNoReturnList();
        if (!listForm) {
            return;
        }

        std::vector<RE::FormID> refFormIDs;

        for (auto* form : listForm->forms) {
            const RE::FormID formID = form ? form->GetFormID() : 0;
            if (formID != 0 &&
                std::find(refFormIDs.begin(), refFormIDs.end(), formID) == refFormIDs.end()) {
                refFormIDs.push_back(formID);
            }
        }

        if (listForm->scriptAddedTempForms) {
            for (const auto formID : *listForm->scriptAddedTempForms) {
                if (formID != 0 &&
                    std::find(refFormIDs.begin(), refFormIDs.end(), formID) == refFormIDs.end()) {
                    refFormIDs.push_back(formID);
                }
            }
        }

        for (const auto formID : refFormIDs) {
            auto* rawForm = RE::TESForm::LookupByID(formID);
            auto* ref = rawForm ? rawForm->As<RE::TESObjectREFR>() : nullptr;
            if (ref) {
                if (!ref->IsDisabled()) {
                    ref->Disable();
                }
                RemovePickupBlock(ref);
                ref->SetDelete(true);
            }

            RemovePickupTriggerID(listForm, formID);
        }

        if (!refFormIDs.empty() && !AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] disabled/deleted {} NPC no-return dropped weapon ref(s) and removed them from madThrowableWeapDropList: {}",
                refFormIDs.size(),
                reason ? reason : "unknown");
        }
    }

    inline RE::ExtraDataList* FindInvExtraForItem(
        RE::TESObjectREFR* owner,
        RE::TESBoundObject* item)
    {
        if (!owner || !item) {
            return nullptr;
        }

        auto* inventoryChanges = owner->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return nullptr;
        }

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || entry->object != item || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (extraList) {
                    return extraList;
                }
            }
        }

        return nullptr;
    }

    inline void SendPapyrusPickupEvent(
        const ThrownInventoryTransferRecord& record,
        RE::TESObjectREFR* droppedRef,
        const char* reason)
    {
        auto* eventSource = SKSE::GetModCallbackEventSource();
        if (!eventSource) {
            SKSE::log::debug("[SaberThrow] proximity pickup ModEvent skipped: no ModCallbackEventSource.");
            return;
        }

        auto* itemForm = record.itemFormID ? RE::TESForm::LookupByID(record.itemFormID) : nullptr;
        auto* item = itemForm ? itemForm->As<RE::TESBoundObject>() : nullptr;

        const bool isShield =
            item &&
            item->As<RE::TESObjectARMO>() &&
            IsShieldForTransfer(item->As<RE::TESObjectARMO>());
        const bool isTorch = item && IsThrowTorchBase(item);
        const bool isWeapon = item && item->As<RE::TESObjectWEAP>() != nullptr;

        char payload[256]{};
        std::snprintf(
            payload,
            sizeof(payload),
            "%s|%s|hand=%s|item=0x%08X|ref=0x%08X",
            isShield ? "shield" : (isTorch ? "torch" : (isWeapon ? "weapon" : "item")),
            reason ? reason : "player entered dropped throw pickup radius",
            record.wasLeftHand ? "left" : "right",
            record.itemFormID,
            record.thrownRefFormID);

        RE::TESForm* sender = itemForm ? itemForm : static_cast<RE::TESForm*>(droppedRef);

        SKSE::ModCallbackEvent event{
            RE::BSFixedString(kPickupEvent),
            RE::BSFixedString(payload),
            (isShield || isTorch) ? 1.0f : 0.0f,
            sender
        };

        eventSource->SendEvent(&event);

        SKSE::log::debug(
            "[SaberThrow] sent proximity pickup ModEvent '{}' payload='{}' sender=0x{:08X}",
            kPickupEvent,
            payload,
            sender ? sender->GetFormID() : 0);
    }

    inline void ShowDebugNotifyNoSound(const char* message);

    inline bool ReturnTempItemToPlayer(
        const ThrownInventoryTransferRecord& record,
        bool equipItem,
        const char* reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || record.itemFormID == 0) {
            return false;
        }

        auto* itemForm = RE::TESForm::LookupByID(record.itemFormID);
        auto* item = itemForm ? itemForm->As<RE::TESBoundObject>() : nullptr;
        if (!item) {
            SKSE::log::warn(
                "[SaberThrow] proximity pickup failed: could not resolve stored item 0x{:08X} for dropped ref 0x{:08X}",
                record.itemFormID,
                record.thrownRefFormID);
            return false;
        }

        const bool boundWeapon =
            item->As<RE::TESObjectWEAP>() &&
            item->As<RE::TESObjectWEAP>()->IsBound();

        if (boundWeapon) {
            RestoreBoundThrowEffect(record, player, reason);
            return true;
        }

        auto* tempContainer = GetThrowTemp();
        if (!tempContainer) {
            return false;
        }

        const auto tempCounts = tempContainer->GetInventoryCounts();
        const auto countIt = tempCounts.find(item);
        if (countIt == tempCounts.end() || countIt->second <= 0) {
            SKSE::log::warn(
                "[SaberThrow] proximity pickup failed: temp container does not contain item 0x{:08X} for dropped ref 0x{:08X}",
                record.itemFormID,
                record.thrownRefFormID);
            return false;
        }

        auto* extraList = FindInvExtraForItem(tempContainer, item);

        tempContainer->RemoveItem(
            item,
            1,
            RE::ITEM_REMOVE_REASON::kRemove,
            extraList,
            player);

        if (equipItem) {
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (equipManager) {
                auto* equipSlot = GetEquipSlotThrownHand(item, record.wasLeftHand);

                constexpr bool kQueueEquip = false;
                constexpr bool kForceEquip = false;
                constexpr bool kPlaySounds = false;
                constexpr bool kApplyNow = true;

                equipManager->EquipObject(
                    player,
                    item,
                    extraList,
                    1,
                    equipSlot,
                    kQueueEquip,
                    kForceEquip,
                    kPlaySounds,
                    kApplyNow);
            }
        }
        else {
            ShowDebugNotifyNoSound("Equipment retrieved");
        }

        SKSE::log::debug(
            "[SaberThrow] proximity pickup returned item 0x{:08X} for dropped ref 0x{:08X} {}: {}{}",
            record.itemFormID,
            record.thrownRefFormID,
            equipItem ? (record.wasLeftHand ? "and equipped it in the left hand" : "and equipped it in the right hand") : "to inventory without equipping",
            reason ? reason : "unknown",
            extraList ? " using preserved extra list" : "");

        return true;
    }

    inline bool RestoreThrowItem(
        RE::TESObjectREFR* thrownRef,
        const char* reason)
    {
        if (!thrownRef) {
            return false;
        }

        const RE::FormID thrownRefFormID = thrownRef->GetFormID();
        if (thrownRefFormID == 0) {
            return false;
        }

        ThrownInventoryTransferRecord record{};
        if (!GetThrowInvRecord(thrownRefFormID, record)) {
            return false;
        }

        if (!ReturnTempItemToPlayer(record, true, reason)) {
            return false;
        }

        RemoveThrowInvRecord(thrownRefFormID);
        return true;
    }

    inline bool IsNearDroppedThrow(
        RE::PlayerCharacter* player,
        RE::TESObjectREFR* droppedRef)
    {
        if (!player || !droppedRef || droppedRef->IsDisabled()) {
            return false;
        }

        const RE::NiPoint3 playerPos = player->GetPosition();
        const RE::NiPoint3 droppedPos = droppedRef->GetPosition();

        const float dx = playerPos.x - droppedPos.x;
        const float dy = playerPos.y - droppedPos.y;
        const float dz = playerPos.z - droppedPos.z;
        const float distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        const float pickupRadiusMult =
            ::SaberThrow::Settings::GetPickupRadiusMultiplier();

        return distanceSquared <=
            kDropPickupDistSq * pickupRadiusMult * pickupRadiusMult;
    }

    inline void ClearDroppedThrowRef(RE::TESObjectREFR* ref, const char* reason)
    {
        if (!ref) {
            return;
        }

        if (!ref->IsDisabled()) {
            ref->Disable();
        }
        RemovePickupBlock(ref);
        ref->SetDelete(true);

        SKSE::log::debug(
            "[SaberThrow] disabled/deleted proximity-picked dropped ref 0x{:08X}: {}",
            ref->GetFormID(),
            reason ? reason : "unknown");
    }

    inline void TickPickupMonitor()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        const auto records = SnapshotThrowInvRecords();
        if (records.empty()) {
            return;
        }

        auto* listForm = GetPickupTriggerList();
        const bool autoEquipPickup =
            ::SaberThrow::Settings::Get().autoEquipPickup;

        for (const auto& record : records) {
            if (record.thrownRefFormID == 0 || record.itemFormID == 0) {
                continue;
            }

            auto* rawForm = RE::TESForm::LookupByID(record.thrownRefFormID);
            auto* droppedRef = rawForm ? rawForm->As<RE::TESObjectREFR>() : nullptr;
            if (!droppedRef) {
                RemovePickupTriggerID(listForm, record.thrownRefFormID);
                RemoveThrowInvRecord(record.thrownRefFormID);
                continue;
            }

            if (listForm && !listForm->HasForm(droppedRef)) {
                continue;
            }

            if (!IsNearDroppedThrow(player, droppedRef)) {
                continue;
            }

            if (IsHitFrameThrowWaiting()) {
                SKSE::log::debug(
                    "[SaberThrow] deferred proximity pickup return for dropped ref 0x{:08X}; another throw is still waiting to move its equipped item to the temp container.",
                    record.thrownRefFormID);
                continue;
            }

            if (ReturnTempItemToPlayer(
                record,
                autoEquipPickup,
                "player entered dropped throw pickup radius")) {
                SendPapyrusPickupEvent(
                    record,
                    droppedRef,
                    "player entered dropped throw pickup radius");
                ClearDroppedThrowRef(
                    droppedRef,
                    "player entered dropped throw pickup radius");
                RemovePickupTriggerID(listForm, record.thrownRefFormID);
                RemoveThrowInvRecord(record.thrownRefFormID);
            }
        }
    }

    inline void QueuePickupMonitorTask()
    {
        bool expected = false;
        if (!g_pickupPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            g_pickupPending.store(false, std::memory_order_release);
            return;
        }

        taskInterface->AddTask([]()
            {
                g_pickupPending.store(false, std::memory_order_release);
                TickPickupMonitor();
            });
    }

    inline void StartPickupMonitor()
    {
        bool expected = false;
        if (!g_pickupMonitor.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        std::thread([]()
            {
                while (HasThrowInvRecords()) {
                    QueuePickupMonitorTask();
                    std::this_thread::sleep_for(std::chrono::milliseconds(kDropPickupPollMs));
                }

                g_pickupMonitor.store(false, std::memory_order_release);

                if (HasThrowInvRecords()) {
                    StartPickupMonitor();
                }
            }).detach();
    }

    inline void ClearPickupTriggerRefs(const char* reason)
    {
        auto* listForm = GetPickupTriggerList();
        if (!listForm) {
            return;
        }

        std::vector<RE::FormID> refFormIDs;

        for (auto* form : listForm->forms) {
            const RE::FormID formID = form ? form->GetFormID() : 0;
            if (formID != 0 && std::find(refFormIDs.begin(), refFormIDs.end(), formID) == refFormIDs.end()) {
                refFormIDs.push_back(formID);
            }
        }

        if (listForm->scriptAddedTempForms) {
            for (const auto formID : *listForm->scriptAddedTempForms) {
                if (formID != 0 && std::find(refFormIDs.begin(), refFormIDs.end(), formID) == refFormIDs.end()) {
                    refFormIDs.push_back(formID);
                }
            }
        }

        for (const auto formID : refFormIDs) {
            auto* rawForm = RE::TESForm::LookupByID(formID);
            auto* ref = rawForm ? rawForm->As<RE::TESObjectREFR>() : nullptr;
            if (ref) {
                if (!ref->IsDisabled()) {
                    ref->Disable();
                }
                RemovePickupBlock(ref);
                ref->SetDelete(true);
            }

            RemovePickupTriggerID(listForm, formID);
        }

        if (!refFormIDs.empty() && !AreLoadScreenCleanupLogsSuppressed()) {
            SKSE::log::debug(
                "[SaberThrow] disabled/deleted {} active pickup trigger ref(s) and removed them from the list: {}",
                refFormIDs.size(),
                reason ? reason : "unknown");
        }
    }

    inline void CleanupThrownInvLoad(const char* reason)
    {
        ScopedLoadScreenCleanupLogSuppression suppressLoadLogs;
        ShutdownThrowsForLoad(
            reason ? reason : "load-screen transient throw reset");
    }

    inline void RunInvRecovery(const char* reason)
    {
        ScopedLoadScreenCleanupLogSuppression suppressLoadLogs;
        const char* recoveryReason = reason ? reason : "post-load temp-container recovery";

        ClearPickupTriggerRefs(recoveryReason);
        ClearNPCNoReturnRefs(recoveryReason);
        ClearPickupBlockRefs();
        ReturnThrowTempItems(recoveryReason);
        ClearThrowInvMemory(recoveryReason);

        if (g_postLoadObserver) {
            g_postLoadObserver();
        }
    }

    inline void RecoverThrowInvFromTemp(const char* reason)
    {
        ReturnThrowTempItems(reason);
        ClearThrowInvMemory(reason);

        if (g_postLoadObserver) {
            g_postLoadObserver();
        }
    }

    inline void QueueInvRecoveryTask(const char* reason)
    {
        bool expected = false;
        if (!g_invRecoveryPending.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            return;
        }

        const std::string reasonCopy = reason ? reason : "post-loading-menu actual chest recovery";

        std::thread([reasonCopy]() {
            constexpr std::array<std::uint32_t, 2> kRecoveryDelaysMs{ 250, 750 };
            std::uint32_t previousDelayMs = 0;

            for (std::size_t i = 0; i < kRecoveryDelaysMs.size(); ++i) {
                const std::uint32_t delayMs = kRecoveryDelaysMs[i];
                if (delayMs > previousDelayMs) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs - previousDelayMs));
                }
                previousDelayMs = delayMs;

                auto* taskInterface = SKSE::GetTaskInterface();
                if (!taskInterface) {
                    g_invRecoveryPending.store(false, std::memory_order_release);
                    return;
                }

                const bool lastPass = (i + 1) == kRecoveryDelaysMs.size();
                taskInterface->AddTask([reasonCopy, delayMs, lastPass]() {
                    (void)delayMs;
                    RunInvRecovery(reasonCopy.c_str());

                    if (lastPass) {
                        g_invRecoveryPending.store(false, std::memory_order_release);
                    }
                    });
            }
            }).detach();
    }

    inline void ResetLoopSoundState()
    {
        g_state.loopSound = RE::BSSoundHandle{};
        g_state.loopSoundPlaying = false;
        g_state.loopSoundStart = std::chrono::steady_clock::time_point{};
    }

    inline RE::BGSSoundDescriptorForm* GetLoopSoundDesc()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        if (auto* soundForm = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(
            kLoopSoundLocalID,
            kLoopSoundPlugin)) {
            return soundForm;
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kLoopSoundFullID)) {
            if (auto* soundForm = rawForm->As<RE::BGSSoundDescriptorForm>()) {
                return soundForm;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kLoopSoundLocalID)) {
            return rawForm->As<RE::BGSSoundDescriptorForm>();
        }

        return nullptr;
    }

    inline void StopLoopSound(const char* reason)
    {
        if (g_state.loopSoundPlaying) {
            g_state.loopSound.Stop();

            if (!AreLoadScreenCleanupLogsSuppressed()) {
                SKSE::log::debug(
                    "[SaberThrow] stopped throw loop sound: {}",
                    reason ? reason : "unknown");
            }
        }

        ResetLoopSoundState();
    }

    inline void StopLoopSoundIfTimedOut(std::chrono::steady_clock::time_point now)
    {
        if (!g_state.loopSoundPlaying ||
            g_state.loopSoundStart == std::chrono::steady_clock::time_point{}) {
            return;
        }

        const float elapsedSeconds = std::chrono::duration<float>(now - g_state.loopSoundStart).count();
        if (elapsedSeconds >= kLoopSoundMaxSec) {
            StopLoopSound("10-second failsafe");
        }
    }

    class ThrowLoopSoundLoadingMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const char* menuName = event->menuName.c_str();
            if (menuName && std::strcmp(menuName, "Loading Menu") == 0) {
                if (event->opening) {
                    CleanupThrownInvLoad("Loading Menu opened");
                }
                else {
                    QueueInvRecoveryTask(
                        "Loading Menu closed actual temp-container recovery");
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline ThrowLoopSoundLoadingMenuSink g_loopMenuSink{};
    inline std::atomic_bool g_loopSafetyReady{ false };

    inline void AddLoopSoundListeners()
    {
        bool expected = false;
        if (!g_loopSafetyReady.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            return;
        }

        CachePowerCooldown("load-safety/chest recovery listener install/startup cache");
        PrimePowerCooldown(
            "load-safety/chest recovery listener install/current selected player power");
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_loopMenuSink);
        }

        if (auto* messaging = SKSE::GetMessagingInterface()) {
            messaging->RegisterListener([](SKSE::MessagingInterface::Message* message) {
                if (!message) {
                    return;
                }

                switch (message->type) {
                case SKSE::MessagingInterface::kPreLoadGame:
                    CleanupThrownInvLoad("SKSE PreLoadGame");
                    break;
                case SKSE::MessagingInterface::kNewGame:
                    CleanupThrownInvLoad("SKSE NewGame");
                    break;
                case SKSE::MessagingInterface::kPostLoadGame:
                    break;
                default:
                    break;
                }
                });
        }
    }

    inline void StartLoopSound(RE::TESObjectREFR* thrownRef)
    {
        StopLoopSound("restart/new throw");
        AddLoopSoundListeners();

        auto* soundForm = GetLoopSoundDesc();
        if (!soundForm) {
            SKSE::log::warn(
                "[SaberThrow] throw loop sound lookup failed: id=0x{:08X}, plugin='{}'",
                kLoopSoundLocalID,
                kLoopSoundPlugin);
            return;
        }

        auto* audioManager = RE::BSAudioManager::GetSingleton();
        if (!audioManager) {
            SKSE::log::warn("[SaberThrow] throw loop sound failed: no BSAudioManager.");
            return;
        }

        RE::BSSoundHandle handle{};
        handle.soundID = static_cast<std::uint32_t>(-1);
        handle.assumeSuccess = false;
        *reinterpret_cast<std::uint32_t*>(&handle.state) = 0;

        (void)audioManager->GetSoundHandle(
            handle,
            static_cast<RE::BSISoundDescriptor*>(soundForm),
            16);

        bool attachedToPlayer = false;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            handle.SetPosition(player->GetPosition());
            if (auto* player3D = player->Get3D(false)) {
                handle.SetObjectToFollow(player3D);
                attachedToPlayer = true;
            }
        }
        else if (thrownRef) {
            handle.SetPosition(thrownRef->GetPosition());
        }

        handle.Play();

        g_state.loopSound = handle;
        g_state.loopSoundPlaying = true;
        g_state.loopSoundStart = std::chrono::steady_clock::now();

        SKSE::log::debug(
            "[SaberThrow] started throw loop sound on {}: id=0x{:08X}, plugin='{}'",
            attachedToPlayer ? "player" : "fallback position",
            kLoopSoundLocalID,
            kLoopSoundPlugin);
    }

    inline ThrowPoisonState g_throwPoison{};
    inline std::mutex       g_throwPoisonLock{};

    inline std::atomic_bool     g_saberRunning{ false };
    inline std::atomic_uint32_t g_saberSession{ 1 };
    inline std::atomic_bool     g_forceReturn{ false };
    inline std::atomic_bool     g_updateQueued{ false };
    inline std::atomic_bool     g_schedulerActive{ false };

    inline void CacheSpearFixedPose();
    inline void ApplySpearPose(RE::TESObjectREFR* ref);
    inline void MoveAndApplySpearPose(RE::TESObjectREFR* ref, const RE::NiPoint3& newPos);

    inline constexpr float kAutoReturnSec = 10.0f;

    inline constexpr float kMinDownSpeed = 0.05f;

    inline constexpr float kEndSpeedFrac = 0.5f;

    inline constexpr float kCurveStartFrac = 0.50f;

    inline constexpr float kNPCLiftEndFrac = 0.20f;

    inline constexpr float kNPCMinLiftZ = 0.25f;

    inline constexpr float kNPCIgnoreGeomFrac = 0.25f;

    inline constexpr float kDownRampSec = 0.50f;

    inline constexpr float kGroundSweepRadius = 19.0f;

    inline constexpr float kGeomStopLift = 4.0f;

}

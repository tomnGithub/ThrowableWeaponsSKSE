#include "papyrus.h"
#include "SaberThrow/SaberThrow_SKSE.hpp"
#include "SaberThrow/SaberThrowAPI.h"
#include "SaberThrow/Settings.h"

#include "RE/B/BGSPerk.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESDataHandler.h"

namespace Papyrus
{
    namespace
    {
        static RE::BGSPerk* LookupConfiguredThrowWeaponPerk(
            std::uint32_t     a_perkID,
            const std::string& a_pluginName)
        {
            if (a_perkID == 0 || a_pluginName.empty()) {
                return nullptr;
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return nullptr;
            }

            return dataHandler->LookupForm<RE::BGSPerk>(a_perkID, a_pluginName);
        }

        static bool PlayerHasAnyPerkFromList(
            RE::PlayerCharacter*                                      a_player,
            const std::vector<::SaberThrow::Settings::PerkFormSpec>& a_perks,
            const char*                                               a_listName)
        {
            for (std::size_t i = 0; i < a_perks.size(); ++i) {
                const auto& perkSpec = a_perks[i];

                if (perkSpec.formID == 0 || perkSpec.pluginName.empty()) {
                    LOG_DEBUG(
                        "  >SaberThrowPlayerHasThrowWeaponPerk: {} perk entry {} is blank/disabled.",
                        a_listName,
                        i + 1);
                    continue;
                }

                auto* perk = LookupConfiguredThrowWeaponPerk(
                    perkSpec.formID,
                    perkSpec.pluginName);
                if (!perk) {
                    LOG_DEBUG(
                        "  >SaberThrowPlayerHasThrowWeaponPerk: failed to resolve {} perk entry {}: 0x{:08X} / '{}'.",
                        a_listName,
                        i + 1,
                        perkSpec.formID,
                        perkSpec.pluginName);
                    continue;
                }

                if (a_player->HasPerk(perk)) {
                    LOG_DEBUG(
                        "  >SaberThrowPlayerHasThrowWeaponPerk: player has {} perk entry {}: 0x{:08X} / '{}'.",
                        a_listName,
                        i + 1,
                        perkSpec.formID,
                        perkSpec.pluginName);
                    return true;
                }
            }

            return false;
        }

        static bool PlayerHasAnyConfiguredThrowWeaponPerk()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                LOG_DEBUG("  >SaberThrowPlayerHasThrowWeaponPerk: no player."sv);
                return false;
            }

            const auto settings = ::SaberThrow::Settings::Get();

            if (PlayerHasAnyPerkFromList(
                    player,
                    settings.weaponPerks1H,
                    "1H") ||
                PlayerHasAnyPerkFromList(
                    player,
                    settings.weaponPerks2H,
                    "2H")) {
                return true;
            }

            LOG_DEBUG(
                "  >SaberThrowPlayerHasThrowWeaponPerk: player has none of the configured 1H or 2H perks."sv);
            return false;
        }

        static bool PlayerHasConfiguredThrowShieldPerk()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                LOG_DEBUG("  >SaberThrowPlayerHasThrowShieldPerk: no player."sv);
                return false;
            }

            const auto settings = ::SaberThrow::Settings::Get();

            const std::uint32_t perkID = settings.shieldPerkID;
            const std::string& pluginName = settings.shieldPerkPlugin;

            if (perkID == 0 || pluginName.empty()) {
                LOG_DEBUG("  >SaberThrowPlayerHasThrowShieldPerk: shield perk slot is blank/disabled."sv);
                return false;
            }

            auto* perk = LookupConfiguredThrowWeaponPerk(perkID, pluginName);
            if (!perk) {
                LOG_DEBUG(
                    "  >SaberThrowPlayerHasThrowShieldPerk: failed to resolve perk: 0x{:08X} / '{}'.",
                    perkID,
                    pluginName);
                return false;
            }

            const bool hasPerk = player->HasPerk(perk);
            LOG_DEBUG(
                hasPerk
                ? "  >SaberThrowPlayerHasThrowShieldPerk: player has perk: 0x{:08X} / '{}'."
                : "  >SaberThrowPlayerHasThrowShieldPerk: player does not have perk: 0x{:08X} / '{}'.",
                perkID,
                pluginName);

            return hasPerk;
        }
    }

    static void SaberThrow(STATIC_ARGS,
        RE::BSFixedString  a_modName,
        RE::TESObjectREFR* a_ref,
        float              a_travel,
        float              a_step,
        float              a_spin,
        bool               a_noReturn)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrow"sv);
        LOG_DEBUG(a_noReturn ? "  > noReturn=true"sv : "  > noReturn=false"sv);

        const char* modName = a_modName.c_str();

        if (!modName || modName[0] == '\0') {
            LOG_DEBUG("  >SaberThrow: empty mod name, skipping."sv);
            return;
        }

        if (!a_ref) {
            LOG_DEBUG("  >SaberThrow: null ref, skipping."sv);
            return;
        }

        ::SaberThrow::Throw(modName, a_ref, a_travel, a_step, a_spin, a_noReturn);
    }

    static bool SaberThrowCacheEquippedWeaponPoison(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowCacheEquippedWeaponPoison"sv);

        const bool cached = ::SaberThrow::CacheThrowPoison();
        LOG_DEBUG(cached
            ? "  >SaberThrowCacheEquippedWeaponPoison: cached and consumed one poison stack."sv
            : "  >SaberThrowCacheEquippedWeaponPoison: no poison cached."sv);

        return cached;
    }

    static void SaberThrowStop(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowStop"sv);
        ::SaberThrow::StopSaberThrow();
    }

    static void SaberThrowReturn(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowReturn"sv);
        ::SaberThrow::RequestSaberReturnEarly();
    }

    static void SaberThrowReloadMCMSettings(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowReloadMCMSettings"sv);

        ::SaberThrow::Settings::LoadMCMSettings();
        ::SaberThrow::RecoverThrowInvFromTemp(
            "Papyrus SaberThrowReloadMCMSettings actual temp-container recovery");

        LOG_DEBUG("  >Reloaded SaberThrow MCM settings and checked throw temp container."sv);
    }

    static void SaberThrowCleanupThrownObjects(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowCleanupThrownObjects"sv);

        ::SaberThrow::CleanupThrowInv(
            "Papyrus SaberThrowCleanupThrownObjects");
    }

    static bool SaberThrowPlayerHasThrowWeaponPerk(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowPlayerHasThrowWeaponPerk"sv);

        return PlayerHasAnyConfiguredThrowWeaponPerk();
    }

    static bool SaberThrowPlayerHasThrowShieldPerk(STATIC_ARGS)
    {
        LOG_DEBUG("===[PAPYRUS]==="sv);
        LOG_DEBUG("Called SaberThrowPlayerHasThrowShieldPerk"sv);

        return PlayerHasConfiguredThrowShieldPerk();
    }

    static bool SaberThrowSendPlayerShoutAnimation(STATIC_ARGS, RE::BSFixedString a_eventName)
    {
        const char* eventName = a_eventName.c_str();
        return ::SaberThrow::QueueShoutAnim(eventName);
    }

    static bool ThrowEquippedWeapon(STATIC_ARGS,
        RE::BSFixedString  a_animationEvent,
        RE::BSFixedString  a_triggerEvent,
        RE::BSFixedString  a_modName,
        RE::TESObjectREFR* a_ref,
        float              a_travel,
        float              a_step,
        float              a_spin,
        bool               a_noReturn)
    {
        const char* animationEvent = a_animationEvent.c_str();
        const char* triggerEvent = a_triggerEvent.c_str();
        const char* modName = a_modName.c_str();

        return ::SaberThrow::QueueAnimThrow(
            animationEvent,
            triggerEvent,
            modName,
            a_ref,
            a_travel,
            a_step,
            a_spin,
            a_noReturn);
    }

    static void Bind(VM& a_vm)
    {
        logger::info("  >Binding SaberThrow..."sv);
        BIND(SaberThrow);

        logger::info("  >Binding SaberThrowCacheEquippedWeaponPoison..."sv);
        BIND(SaberThrowCacheEquippedWeaponPoison);

        logger::info("  >Binding SaberThrowStop..."sv);
        BIND(SaberThrowStop);

        logger::info("  >Binding SaberThrowReturn..."sv);
        BIND(SaberThrowReturn);

        logger::info("  >Binding SaberThrowReloadMCMSettings..."sv);
        BIND(SaberThrowReloadMCMSettings);

        logger::info("  >Binding SaberThrowCleanupThrownObjects..."sv);
        BIND(SaberThrowCleanupThrownObjects);

        logger::info("  >Binding SaberThrowPlayerHasThrowWeaponPerk..."sv);
        BIND(SaberThrowPlayerHasThrowWeaponPerk);

        logger::info("  >Binding SaberThrowPlayerHasThrowShieldPerk..."sv);
        BIND(SaberThrowPlayerHasThrowShieldPerk);

        logger::info("  >Binding SaberThrowSendPlayerShoutAnimation..."sv);
        BIND(SaberThrowSendPlayerShoutAnimation);

        logger::info("  >Binding ThrowEquippedWeapon..."sv);
        BIND(ThrowEquippedWeapon);

    }

    bool RegisterFunctions(VM* a_vm)
    {
        if (!a_vm) {
            logger::error("Cannot bind SaberThrow Papyrus functions: VM is null."sv);
            return false;
        }

        logger::info("Binding papyrus functions in utility script {}..."sv, script);
        Bind(*a_vm);
        logger::info("Finished binding functions."sv);
        return true;
    }
}



namespace
{
    class SaberThrowAPI_V1 final : public SaberThrowAPI::IVSaberThrow1
    {
    public:
        [[nodiscard]] std::uint32_t GetInterfaceVersion() const noexcept override
        {
            return static_cast<std::uint32_t>(SaberThrowAPI::InterfaceVersion::V1);
        }

        [[nodiscard]] bool StartNPCNoReturnRightHandWeaponThrow(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCNoReturnRightHandWeaponThrow(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool StartNPCNoReturnRightHandWeaponThrowMainThread(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCNoReturnRightHandWeaponThrowMainThread(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool IsNPCNoReturnRightHandWeaponThrowPendingMainThread(
            RE::Actor* sourceActor) const noexcept override
        {
            return ::SaberThrow::IsNPCNoReturnRightHandWeaponThrowPendingMainThread(sourceActor);
        }
    };

    class SaberThrowAPI_V2 final : public SaberThrowAPI::IVSaberThrow2
    {
    public:
        [[nodiscard]] std::uint32_t GetInterfaceVersion() const noexcept override
        {
            return static_cast<std::uint32_t>(SaberThrowAPI::InterfaceVersion::V2);
        }

        [[nodiscard]] bool StartNPCNoReturnRightHandWeaponThrow(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCNoReturnRightHandWeaponThrow(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool StartNPCNoReturnRightHandWeaponThrowMainThread(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCNoReturnRightHandWeaponThrowMainThread(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool IsNPCNoReturnRightHandWeaponThrowPendingMainThread(
            RE::Actor* sourceActor) const noexcept override
        {
            return ::SaberThrow::IsNPCNoReturnRightHandWeaponThrowPendingMainThread(sourceActor);
        }

        [[nodiscard]] bool StartNPCTelekineticRightHandWeaponThrow(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCTelekineticRightHandWeaponThrow(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool StartNPCTelekineticRightHandWeaponThrowMainThread(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept override
        {
            return ::SaberThrow::StartNPCTelekineticRightHandWeaponThrowMainThread(
                sourceActor, totalTravel, stepDist, spinRateRadTick);
        }

        [[nodiscard]] bool IsNPCTelekineticRightHandWeaponThrowPendingMainThread(
            RE::Actor* sourceActor) const noexcept override
        {
            return ::SaberThrow::IsNPCTelekineticRightHandWeaponThrowPendingMainThread(sourceActor);
        }
    };

    SaberThrowAPI_V1 g_saberThrowAPI_V1{};
    SaberThrowAPI_V2 g_saberThrowAPI_V2{};
}

extern "C" DLLEXPORT void* RequestPluginAPI(SaberThrowAPI::InterfaceVersion interfaceVersion)
{
    switch (interfaceVersion) {
    case SaberThrowAPI::InterfaceVersion::V1:
        return &g_saberThrowAPI_V1;
    case SaberThrowAPI::InterfaceVersion::V2:
        return &g_saberThrowAPI_V2;
    default:
        return nullptr;
    }
}

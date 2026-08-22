#pragma once

#include "SaberThrow/SaberThrowNoReturn.hpp"

namespace SaberThrow
{
    inline RE::TESObjectWEAP* GetPlayerWeapon(RE::PlayerCharacter* player, bool& outLeftHand)
    {
        outLeftHand = false;
        if (!player) {
            return nullptr;
        }

        if (auto* rightObj = player->GetEquippedObject(false)) {
            if (auto* rightWeapon = rightObj->As<RE::TESObjectWEAP>()) {
                outLeftHand = false;
                return rightWeapon;
            }
        }

        if (auto* leftObj = player->GetEquippedObject(true)) {
            if (auto* leftWeapon = leftObj->As<RE::TESObjectWEAP>()) {
                outLeftHand = true;
                return leftWeapon;
            }
        }

        return nullptr;
    }

    inline RE::EnchantmentItem* GetWeaponEnchantCharge(
        RE::TESObjectWEAP* weapon,
        RE::ExtraDataList* extraList,
        std::uint16_t& outMaxCharge)
    {
        outMaxCharge = 0;

        if (!weapon || !extraList) {
            return nullptr;
        }

        if (auto* extraEnchantment = extraList->GetByType<RE::ExtraEnchantment>();
            extraEnchantment && extraEnchantment->enchantment) {
            outMaxCharge = extraEnchantment->charge;
            return extraEnchantment->enchantment;
        }

        if (weapon->formEnchanting) {
            outMaxCharge = weapon->amountofEnchantment;
            return weapon->formEnchanting;
        }

        return nullptr;
    }

    inline float GetEnchantCost(RE::EnchantmentItem* enchantment, RE::Actor* caster)
    {
        if (!enchantment) {
            return 0.0f;
        }

        const float adjustedCost = enchantment->CalculateMagickaCost(caster);
        if (std::isfinite(adjustedCost) && adjustedCost > 0.0f) {
            return adjustedCost;
        }

        return 0.0f;
    }

    inline EquippedWeaponChargeExtra FindWeaponCharge(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* expectedWeapon = nullptr,
        bool restrictHand = false,
        bool expectedLeftHand = false)
    {
        EquippedWeaponChargeExtra result{};

        if (!player) {
            return result;
        }

        bool leftHand = false;
        auto* weapon = expectedWeapon;
        if (weapon) {
            leftHand = expectedLeftHand;
        }
        else {
            weapon = GetPlayerWeapon(player, leftHand);
        }

        if (!weapon) {
            return result;
        }

        auto* inventoryChanges = player->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return result;
        }

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || entry->object != weapon || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (!extraList) {
                    continue;
                }

                const bool wornRight = extraList->HasType<RE::ExtraWorn>();
                const bool wornLeft = extraList->HasType<RE::ExtraWornLeft>();

                if (restrictHand) {
                    if (expectedLeftHand ? !wornLeft : !wornRight) {
                        continue;
                    }
                    leftHand = expectedLeftHand;
                }
                else {
                    if (!wornRight && !wornLeft) {
                        continue;
                    }
                    leftHand = wornLeft && !wornRight;
                }

                std::uint16_t maxCharge = 0;
                auto* enchantment = GetWeaponEnchantCharge(weapon, extraList, maxCharge);
                if (!enchantment || maxCharge == 0) {
                    continue;
                }

                auto* chargeExtra = extraList->GetByType<RE::ExtraCharge>();
                const float currentCharge = chargeExtra ?
                    chargeExtra->charge :
                    static_cast<float>(maxCharge);

                if (!std::isfinite(currentCharge) || currentCharge <= 0.0f) {
                    continue;
                }

                const float costPerUse = GetEnchantCost(enchantment, player);
                if (costPerUse <= 0.0f) {
                    continue;
                }

                result.weapon = weapon;
                result.enchantment = enchantment;
                result.inventoryChanges = inventoryChanges;
                result.extraList = extraList;
                result.chargeExtra = chargeExtra;
                result.currentCharge = currentCharge;
                result.costPerUse = costPerUse;
                result.maxCharge = maxCharge;
                result.wasLeftHand = leftHand;
                return result;
            }
        }

        return result;
    }

    inline bool ConsumeWeaponEnchant(
        RE::PlayerCharacter* player,
        const char* reason,
        RE::TESObjectWEAP* expectedWeapon = nullptr,
        bool restrictHand = false,
        bool expectedLeftHand = false)
    {
        auto equippedCharge = FindWeaponCharge(
            player,
            expectedWeapon,
            restrictHand,
            expectedLeftHand);
        if (!equippedCharge.IsValid()) {
            SKSE::log::debug(
                "[SaberThrow] Enchantment charge use skipped for {}: no finite-charge equipped weapon enchantment found; expectedWeapon=0x{:08X}, restrictHand={}, hand={}",
                reason ? reason : "native equipped throw",
                expectedWeapon ? expectedWeapon->GetFormID() : 0,
                restrictHand,
                expectedLeftHand ? "left" : "right");
            return false;
        }

        auto* chargeExtra = equippedCharge.chargeExtra;
        const bool createdCharge = (chargeExtra == nullptr);
        if (!chargeExtra) {
            chargeExtra = new RE::ExtraCharge();
            chargeExtra->charge = equippedCharge.currentCharge;
            equippedCharge.extraList->Add(chargeExtra);
            equippedCharge.chargeExtra = chargeExtra;
        }

        const float previousCharge = chargeExtra->charge;
        const float cost = std::min(previousCharge, equippedCharge.costPerUse);
        chargeExtra->charge = std::max(0.0f, previousCharge - cost);
        equippedCharge.inventoryChanges->changed = true;

        SKSE::log::debug(
            "[SaberThrow] Consumed one equipped weapon enchantment use for {}: weapon=0x{:08X}, enchantment=0x{:08X}, oldCharge={}, newCharge={}, costPerUse={}, maxCharge={}, hand={}, createdExtraCharge={}",
            reason ? reason : "native equipped throw",
            equippedCharge.weapon ? equippedCharge.weapon->GetFormID() : 0,
            equippedCharge.enchantment ? equippedCharge.enchantment->GetFormID() : 0,
            previousCharge,
            chargeExtra->charge,
            equippedCharge.costPerUse,
            equippedCharge.maxCharge,
            equippedCharge.wasLeftHand ? "left" : "right",
            createdCharge);

        return true;
    }

    inline EquippedWeaponPoisonExtra FindWeaponPoison(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* expectedWeapon = nullptr,
        bool restrictHand = false,
        bool expectedLeftHand = false)
    {
        EquippedWeaponPoisonExtra result{};

        if (!player) {
            return result;
        }

        bool leftHand = false;
        auto* weapon = expectedWeapon;
        if (weapon) {
            leftHand = expectedLeftHand;
        }
        else {
            weapon = GetPlayerWeapon(player, leftHand);
        }

        if (!weapon) {
            return result;
        }

        auto* inventoryChanges = player->GetInventoryChanges();
        if (!inventoryChanges || !inventoryChanges->entryList) {
            return result;
        }

        for (auto* entry : *inventoryChanges->entryList) {
            if (!entry || entry->object != weapon || !entry->extraLists) {
                continue;
            }

            for (auto* extraList : *entry->extraLists) {
                if (!extraList) {
                    continue;
                }

                const bool wornRight = extraList->HasType<RE::ExtraWorn>();
                const bool wornLeft = extraList->HasType<RE::ExtraWornLeft>();

                if (restrictHand) {
                    if (expectedLeftHand ? !wornLeft : !wornRight) {
                        continue;
                    }
                    leftHand = expectedLeftHand;
                }
                else {
                    if (!wornRight && !wornLeft) {
                        continue;
                    }
                    leftHand = wornLeft && !wornRight;
                }

                auto* poisonExtra = extraList->GetByType<RE::ExtraPoison>();
                if (!poisonExtra || !poisonExtra->poison || poisonExtra->count == 0) {
                    continue;
                }

                if (!poisonExtra->poison->IsPoison()) {
                    continue;
                }

                result.weapon = weapon;
                result.poison = poisonExtra->poison;
                result.inventoryChanges = inventoryChanges;
                result.extraList = extraList;
                result.poisonExtra = poisonExtra;
                result.wasLeftHand = leftHand;
                return result;
            }
        }

        return result;
    }

    inline bool ConsumeWeaponPoison(
        RE::InventoryChanges* inventoryChanges,
        RE::ExtraDataList* extraList,
        RE::ExtraPoison* poisonExtra)
    {
        if (!inventoryChanges || !extraList || !poisonExtra || !poisonExtra->poison || poisonExtra->count == 0) {
            return false;
        }

        bool consumed = false;
        if (poisonExtra->count > 1) {
            --poisonExtra->count;
            consumed = true;
        }
        else {
            consumed = extraList->Remove(poisonExtra);
        }

        if (consumed) {
            inventoryChanges->changed = true;
        }

        return consumed;
    }

    inline ThrowPoisonState CaptureThrowPoison(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* expectedWeapon = nullptr,
        bool restrictHand = false,
        bool expectedLeftHand = false)
    {
        ThrowPoisonState captured{};

        auto equippedPoison = FindWeaponPoison(
            player,
            expectedWeapon,
            restrictHand,
            expectedLeftHand);
        if (!equippedPoison.IsValid()) {
            SKSE::log::debug("[SaberThrow] Poison cache requested, but no valid poison was found on the equipped weapon.");
            return captured;
        }

        auto* poison = equippedPoison.poison;
        auto* weapon = equippedPoison.weapon;
        const std::uint32_t previousCount = equippedPoison.poisonExtra->count;

        captured.poison = poison;
        captured.sourceWeapon = weapon;
        captured.wasLeftHand = equippedPoison.wasLeftHand;

        if (!ConsumeWeaponPoison(equippedPoison.inventoryChanges, equippedPoison.extraList, equippedPoison.poisonExtra)) {
            SKSE::log::debug(
                "[SaberThrow] Poison cache requested, but failed to consume poison extra data from equipped weapon 0x{:08X}.",
                weapon ? weapon->GetFormID() : 0);
            return ThrowPoisonState{};
        }

        SKSE::log::debug(
            "[SaberThrow] Cached and consumed one equipped weapon poison stack: poison=0x{:08X}, weapon=0x{:08X}, oldCount={}, hand={}",
            poison ? poison->GetFormID() : 0,
            weapon ? weapon->GetFormID() : 0,
            previousCount,
            equippedPoison.wasLeftHand ? "left" : "right");

        return captured;
    }

    inline ThrowPoisonState TakeThrowPoison()
    {
        std::scoped_lock lock(g_throwPoisonLock);
        ThrowPoisonState pending = g_throwPoison;
        g_throwPoison = ThrowPoisonState{};
        return pending;
    }

    inline bool CacheThrowPoison()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        ThrowPoisonState captured = CaptureThrowPoison(player);

        const bool cachedPoison = captured.HasPoison();
        {
            std::scoped_lock lock(g_throwPoisonLock);
            g_throwPoison = cachedPoison ? captured : ThrowPoisonState{};
        }

        if (cachedPoison) {
            SKSE::log::debug("[SaberThrow] Equipped weapon poison cached for the next throw.");
        }
        else {
            SKSE::log::debug("[SaberThrow] Equipped weapon poison cache requested, but no poison was cached.");
        }

        return cachedPoison;
    }

    inline bool HasPendingPoison()
    {
        std::scoped_lock lock(g_throwPoisonLock);
        return g_throwPoison.HasPoison();
    }

    inline bool AutoCacheThrowPoison(
        RE::TESObjectREFR* thrownRef,
        const char* reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        RE::TESObjectWEAP* exactThrownWeapon = nullptr;
        bool exactLeftHand = false;

        if (thrownRef) {
            const char* itemLabel = "equipped item";
            bool leftHand = false;
            auto* item = GetThrownRefInvItem(thrownRef, leftHand, itemLabel);
            exactThrownWeapon = item ? item->As<RE::TESObjectWEAP>() : nullptr;
            exactLeftHand = leftHand;
        }

        ConsumeWeaponEnchant(
            player,
            reason,
            exactThrownWeapon,
            exactThrownWeapon != nullptr,
            exactLeftHand);

        if (HasPendingPoison()) {
            SKSE::log::debug(
                "[SaberThrow] Auto poison cache skipped for {}: pending poison already exists.",
                reason ? reason : "native equipped throw");
            return true;
        }

        ThrowPoisonState captured = CaptureThrowPoison(
            player,
            exactThrownWeapon,
            exactThrownWeapon != nullptr,
            exactLeftHand);

        const bool cached = captured.HasPoison();
        {
            std::scoped_lock lock(g_throwPoisonLock);
            g_throwPoison = cached ? captured : ThrowPoisonState{};
        }

        SKSE::log::debug(
            "[SaberThrow] Auto poison cache for {}: cached={}, exactWeapon=0x{:08X}, hand={}",
            reason ? reason : "native equipped throw",
            cached,
            exactThrownWeapon ? exactThrownWeapon->GetFormID() : 0,
            exactLeftHand ? "left" : "right");
        return cached;
    }

    inline bool PlayerHasWeaponEquip(RE::PlayerCharacter* player, RE::TESObjectWEAP* weapon)
    {
        if (!player || !weapon) {
            return false;
        }

        return player->GetEquippedObject(false) == weapon || player->GetEquippedObject(true) == weapon;
    }

    inline void UnequipPlayerWeaponThrow(RE::PlayerCharacter* player)
    {
        g_state.weaponToReequip = nullptr;
        g_state.weaponUnequipped = false;
        g_state.weaponWasLeft = false;

        if (!player) {
            return;
        }

        bool wasLeftHand = false;
        auto* weapon = GetPlayerWeapon(player, wasLeftHand);
        if (!weapon) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        constexpr bool kQueueEquip = false;
        constexpr bool kForceEquip = true;
        constexpr bool kPlaySounds = false;
        constexpr bool kApplyNow = true;

        const bool unequipped = equipManager->UnequipObject(player, weapon, nullptr, 1, nullptr, kQueueEquip, kForceEquip, kPlaySounds, kApplyNow);

        g_state.weaponToReequip = weapon;
        g_state.weaponUnequipped = unequipped;
        g_state.weaponWasLeft = wasLeftHand;

    }

    inline void RestorePlayerWeapon(const char* reason)
    {
        auto* weapon = g_state.weaponToReequip;
        const bool shouldRestore = g_state.weaponUnequipped && weapon;
        const bool wasLeftHand = g_state.weaponWasLeft;

        g_state.weaponToReequip = nullptr;
        g_state.weaponUnequipped = false;
        g_state.weaponWasLeft = false;

        if (!shouldRestore) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        if (PlayerHasWeaponEquip(player, weapon)) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        constexpr bool kQueueEquip = false;
        constexpr bool kForceEquip = false;
        constexpr bool kPlaySounds = false;
        constexpr bool kApplyNow = true;

        auto* equipSlot = GetHandEquipSlot(wasLeftHand);

        equipManager->EquipObject(player, weapon, nullptr, 1, equipSlot, kQueueEquip, kForceEquip, kPlaySounds, kApplyNow);

    }

    inline bool HasActorDmgdThisThrow(RE::Actor* actor)
    {
        if (!actor) {
            return true;
        }

        const RE::FormID formID = actor->GetFormID();
        return std::find(g_state.damagedActorIDs.begin(), g_state.damagedActorIDs.end(), formID) != g_state.damagedActorIDs.end();
    }

    inline bool RememberDamagedActor(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        if (HasActorDmgdThisThrow(actor)) {
            return false;
        }

        g_state.damagedActorIDs.push_back(actor->GetFormID());
        return true;
    }

    inline constexpr float kLegacySimHz = 60.0f;
    inline constexpr float kSimHz = 120.0f;
    inline constexpr float kFixedDt = 1.0f / kSimHz;
    inline constexpr float kTickScale =
        kLegacySimHz / kSimHz;

    inline constexpr float kNPCSimHz = kSimHz;
    inline constexpr float kNPCFixedDt = kFixedDt;
    inline constexpr float kNPCTickScale = kTickScale;

    inline constexpr float kMaxAccumSec = 0.10f;
    inline constexpr std::uint32_t kMaxTicks = 12;

    inline float GetThrowFixedDt()
    {
        return kFixedDt;
    }

    inline std::uint32_t GetThrowMaxTicks()
    {
        return kMaxTicks;
    }

    inline float GetThrowMinDownStep()
    {
        return kMinDownSpeed * kTickScale;
    }

    inline void UpdateThrow();

    inline constexpr const char* kRestoreEvent = "OnThrownWeaponReturnedToPlayer";
    inline constexpr const char* kNoReturnStopEvent = "OnThrownWeaponNoReturnImpact";
    inline constexpr const char* kReturnStopEvent = "OnThrownWeaponImpact";
    inline constexpr const char* kThrowStartEvent = "OnThrownWeaponStartFlying";

    inline constexpr const char* kWeaponThrowAnim = "WeaponThrow_Start";
    inline constexpr const char* kTelekEndAnim = "TelekineticThrow_End";

    inline void NotifyPlayerAnimGraph(const char* eventName)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !eventName || eventName[0] == '\0') {
            return;
        }

        player->NotifyAnimationGraph(RE::BSFixedString(eventName));
    }

    inline constexpr const char* kShoutStartAnim = "shoutStart";
    inline constexpr const char* kBlockStartAnim = "BlockStart";
    inline constexpr const char* kBashStartAnim = "BashStart";

    inline bool TrySendPlayerShoutAnim(const char* eventName = kShoutStartAnim)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        if (!eventName || eventName[0] == '\0') {
            eventName = kShoutStartAnim;
        }

        if (std::strcmp(eventName, kBashStartAnim) == 0) {
            player->NotifyAnimationGraph(RE::BSFixedString(kBlockStartAnim));
        }

        return player->NotifyAnimationGraph(RE::BSFixedString(eventName));
    }


}

#pragma once

#include "SaberThrow/SaberThrowDismemberment.hpp"

namespace SaberThrow
{
    inline constexpr float kWeaponDamageMult = 1.0f;
    inline constexpr float kMinWeaponDamage = 0.0f;
    inline constexpr float kMinShieldDamage = 1.0f;
    inline constexpr float kShieldArmorMult = 0.50f;
    inline constexpr float kShieldSkillMin = 0.10f;
    inline constexpr float kShieldSkillMax = 0.25f;
    inline constexpr bool  kSendHitStagger = true;

    inline RE::TESObjectWEAP* GetWeaponBase(RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef) {
            return nullptr;
        }

        auto* baseObj = thrownRef->GetObjectReference();
        if (!baseObj) {
            return nullptr;
        }

        return baseObj->As<RE::TESObjectWEAP>();
    }

    inline bool IsShieldArmor(RE::TESObjectARMO* armor)
    {
        return armor &&
            (armor->formFlags & RE::TESObjectARMO::RecordFlags::kShield) != 0;
    }

    inline RE::TESObjectARMO* GetShieldBase(RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef) {
            return nullptr;
        }

        auto* baseObj = thrownRef->GetObjectReference();
        if (!baseObj) {
            return nullptr;
        }

        auto* armor = baseObj->As<RE::TESObjectARMO>();
        return IsShieldArmor(armor) ? armor : nullptr;
    }

    inline bool IsThrownShield(RE::TESObjectREFR* thrownRef)
    {
        return GetShieldBase(thrownRef) != nullptr;
    }

    inline bool IsThrownTorch(RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef) {
            return false;
        }

        return IsThrowTorchBase(thrownRef->GetObjectReference());
    }

    inline constexpr float kShieldRollOffset = 3.14159265359f;

    inline void RotateVisualRaw(
        RE::TESObjectREFR* ref,
        float pitch,
        float roll,
        float yaw,
        bool forceRotation = false)
    {
        if (IsThrownShield(ref)) {
            roll = Wrap0To2Pi(roll + kShieldRollOffset);
        }

        RotateRaw(ref, pitch, roll, yaw, forceRotation);
    }

    inline bool EqualsNoCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const auto a = static_cast<unsigned char>(lhs[i]);
            const auto b = static_cast<unsigned char>(rhs[i]);
            if (std::tolower(a) != std::tolower(b)) {
                return false;
            }
        }

        return true;
    }

    inline bool ContainsNoCase(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty()) {
            return true;
        }

        if (haystack.size() < needle.size()) {
            return false;
        }

        for (std::size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
            bool matched = true;
            for (std::size_t j = 0; j < needle.size(); ++j) {
                const auto a = static_cast<unsigned char>(haystack[i + j]);
                const auto b = static_cast<unsigned char>(needle[j]);
                if (std::tolower(a) != std::tolower(b)) {
                    matched = false;
                    break;
                }
            }

            if (matched) {
                return true;
            }
        }

        return false;
    }

    inline bool StartsWithNoCase(std::string_view value, std::string_view prefix)
    {
        if (value.size() < prefix.size()) {
            return false;
        }

        for (std::size_t i = 0; i < prefix.size(); ++i) {
            const auto a = static_cast<unsigned char>(value[i]);
            const auto b = static_cast<unsigned char>(prefix[i]);
            if (std::tolower(a) != std::tolower(b)) {
                return false;
            }
        }

        return true;
    }

    inline bool ParseKeywordFloat(
        std::string_view editorID,
        std::string_view prefix,
        float& outValue)
    {
        if (!StartsWithNoCase(editorID, prefix) || editorID.size() == prefix.size()) {
            return false;
        }

        const std::string suffix{ editorID.substr(prefix.size()) };
        errno = 0;
        char* end = nullptr;
        const float parsed = std::strtof(suffix.c_str(), &end);

        if (end == suffix.c_str() ||
            !end ||
            *end != '\0' ||
            errno == ERANGE ||
            !std::isfinite(parsed)) {
            return false;
        }

        outValue = parsed;
        return true;
    }

    inline const char* GetKeywordID(const RE::BGSKeyword* keyword)
    {
        if (!keyword) {
            return nullptr;
        }

        const char* editorID = keyword->GetFormEditorID();
        return (editorID && editorID[0] != '\0') ? editorID : nullptr;
    }

    inline bool KeywordMatchesSpear(
        const RE::BGSKeyword* keyword,
        const std::vector<std::string>& spearKeywords)
    {
        const char* editorID = GetKeywordID(keyword);
        if (!editorID || spearKeywords.empty()) {
            return false;
        }

        for (const auto& configuredKeyword : spearKeywords) {
            if (!configuredKeyword.empty() &&
                EqualsNoCase(editorID, configuredKeyword)) {
                return true;
            }
        }

        return false;
    }

    inline bool WeaponHasSpearKeyword(
        RE::TESObjectWEAP* weapon,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!weapon || settings.spearKeywords.empty()) {
            return false;
        }

        const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(weapon);
        if (!keywordForm || !keywordForm->keywords || keywordForm->numKeywords == 0) {
            return false;
        }

        for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
            const auto* keyword = keywordForm->keywords[i];
            if (KeywordMatchesSpear(keyword, settings.spearKeywords)) {
                return true;
            }
        }

        return false;
    }


    inline bool WeaponIsBoomerang(RE::TESObjectWEAP* weapon)
    {
        if (!weapon) {
            return false;
        }

        const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(weapon);
        if (!keywordForm || !keywordForm->keywords || keywordForm->numKeywords == 0) {
            return false;
        }

        for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
            const char* editorID = GetKeywordID(keywordForm->keywords[i]);
            if (editorID && EqualsNoCase(editorID, "WeaponTypeBoomerang")) {
                return true;
            }
        }

        return false;
    }

    inline bool ThrownIsBoomerang(RE::TESObjectREFR* thrownRef)
    {
        return WeaponIsBoomerang(GetWeaponBase(thrownRef));
    }

    inline bool IsFTGMod(const std::string& modName)
    {
        return modName.find("ThrowableWeaponsSKSEFTG") != std::string::npos;
    }

    inline bool ForceBoomerangReturn(
        RE::TESObjectREFR* thrownRef,
        const std::string& modName,
        bool noReturnDynamic)
    {
        if (!noReturnDynamic || IsFTGMod(modName)) {
            return false;
        }

        return ThrownIsBoomerang(thrownRef);
    }


    inline bool WeaponKeywordContains(
        RE::TESObjectWEAP* weapon,
        std::string_view needle)
    {
        if (!weapon || needle.empty()) {
            return false;
        }

        const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(weapon);
        if (!keywordForm || !keywordForm->keywords || keywordForm->numKeywords == 0) {
            return false;
        }

        for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
            const char* editorID = GetKeywordID(keywordForm->keywords[i]);
            if (editorID && ContainsNoCase(editorID, needle)) {
                return true;
            }
        }

        return false;
    }

    inline bool WeaponIsWarhammer(RE::TESObjectWEAP* weapon)
    {
        return WeaponKeywordContains(weapon, "Warhammer");
    }

    inline bool WeaponIs2HAxe(RE::TESObjectWEAP* weapon)
    {
        return WeaponKeywordContains(weapon, "Battleaxe") ||
            WeaponKeywordContains(weapon, "Axe");
    }

    inline bool ThrownHasSpearKeyword(RE::TESObjectREFR* thrownRef)
    {
        return WeaponHasSpearKeyword(
            GetWeaponBase(thrownRef),
            ::SaberThrow::Settings::Get());
    }

    inline ThrowWeaponHandedness GetSpearHandFromKeyword(
        RE::TESObjectWEAP* weapon,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (weapon && !settings.spearKeywords.empty()) {
            const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(weapon);
            if (keywordForm && keywordForm->keywords && keywordForm->numKeywords > 0) {
                for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
                    const auto* keyword = keywordForm->keywords[i];
                    if (!KeywordMatchesSpear(keyword, settings.spearKeywords)) {
                        continue;
                    }

                    const char* editorID = GetKeywordID(keyword);
                    if (!editorID) {
                        continue;
                    }

                    if (ContainsNoCase(editorID, "2H") || ContainsNoCase(editorID, "Pike")) {
                        return ThrowWeaponHandedness::TwoHanded;
                    }

                    if (ContainsNoCase(editorID, "1H") || ContainsNoCase(editorID, "Shortspear")) {
                        return ThrowWeaponHandedness::OneHanded;
                    }
                }
            }
        }

        if (weapon && (weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe())) {
            return ThrowWeaponHandedness::TwoHanded;
        }

        if (weapon &&
            (weapon->IsOneHandedSword() ||
                weapon->IsOneHandedDagger() ||
                weapon->IsOneHandedAxe() ||
                weapon->IsOneHandedMace())) {
            return ThrowWeaponHandedness::OneHanded;
        }

        return ThrowWeaponHandedness::OneHanded;
    }

    inline ThrowWeaponHandedness GetWeaponThrowHand(
        RE::TESObjectWEAP* weapon,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!weapon) {
            return ThrowWeaponHandedness::Unknown;
        }

        if (WeaponHasSpearKeyword(weapon, settings)) {
            return GetSpearHandFromKeyword(weapon, settings);
        }

        if (weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe()) {
            return ThrowWeaponHandedness::TwoHanded;
        }

        if (weapon->IsOneHandedSword() ||
            weapon->IsOneHandedDagger() ||
            weapon->IsOneHandedAxe() ||
            weapon->IsOneHandedMace()) {
            return ThrowWeaponHandedness::OneHanded;
        }

        return ThrowWeaponHandedness::OneHanded;
    }

    enum class ThrowWeaponMultiplierBucket : std::uint8_t
    {
        None,
        Sword1H,
        Sword2H,
        Dagger1H,
        Axe1H,
        Axe2H,
        Mace1H,
        Mace2H,
        Spear1H,
        Spear2H
    };

    struct ThrowWeaponMultipliers
    {
        float damage{ 1.0f };
        float speed{ 1.0f };
        float distance{ 1.0f };
    };

    inline ThrowWeaponMultiplierBucket GetWeaponMultBucket(
        RE::TESObjectWEAP* weapon,
        bool forceSpearMode,
        const ::SaberThrow::Settings::Values& settings)
    {
        if (!weapon) {
            return ThrowWeaponMultiplierBucket::None;
        }

        if (forceSpearMode || WeaponHasSpearKeyword(weapon, settings)) {
            return GetSpearHandFromKeyword(weapon, settings) == ThrowWeaponHandedness::TwoHanded ?
                ThrowWeaponMultiplierBucket::Spear2H :
                ThrowWeaponMultiplierBucket::Spear1H;
        }

        if (weapon->IsOneHandedDagger()) {
            return ThrowWeaponMultiplierBucket::Dagger1H;
        }

        if (weapon->IsOneHandedSword()) {
            return ThrowWeaponMultiplierBucket::Sword1H;
        }

        if (weapon->IsTwoHandedSword()) {
            return ThrowWeaponMultiplierBucket::Sword2H;
        }

        if (weapon->IsOneHandedAxe()) {
            return ThrowWeaponMultiplierBucket::Axe1H;
        }

        if (weapon->IsOneHandedMace()) {
            return ThrowWeaponMultiplierBucket::Mace1H;
        }

        if (weapon->IsTwoHandedAxe()) {
            if (WeaponIsWarhammer(weapon)) {
                return ThrowWeaponMultiplierBucket::Mace2H;
            }

            if (WeaponIs2HAxe(weapon)) {
                return ThrowWeaponMultiplierBucket::Axe2H;
            }

            return ThrowWeaponMultiplierBucket::Axe2H;
        }

        return ThrowWeaponMultiplierBucket::None;
    }


    enum class ThrowImpactSoundSlot : std::size_t
    {
        Geometry = 0,
        Sword1H,
        Sword2H,
        Dagger1H,
        Axe1H,
        Axe2H,
        Mace1H,
        Mace2H,
        Spear1H,
        Spear2H,
        Shield,
        Torch
    };

    inline std::size_t GetHitSoundIndex(ThrowImpactSoundSlot slot)
    {
        return static_cast<std::size_t>(slot);
    }

    inline ThrowImpactSoundSlot GetThrowHitSoundSlot(
        RE::TESObjectWEAP* weapon,
        bool forceSpearMode,
        const ::SaberThrow::Settings::Values& settings)
    {
        switch (GetWeaponMultBucket(weapon, forceSpearMode, settings)) {
        case ThrowWeaponMultiplierBucket::Sword1H:
            return ThrowImpactSoundSlot::Sword1H;
        case ThrowWeaponMultiplierBucket::Sword2H:
            return ThrowImpactSoundSlot::Sword2H;
        case ThrowWeaponMultiplierBucket::Dagger1H:
            return ThrowImpactSoundSlot::Dagger1H;
        case ThrowWeaponMultiplierBucket::Axe1H:
            return ThrowImpactSoundSlot::Axe1H;
        case ThrowWeaponMultiplierBucket::Axe2H:
            return ThrowImpactSoundSlot::Axe2H;
        case ThrowWeaponMultiplierBucket::Mace1H:
            return ThrowImpactSoundSlot::Mace1H;
        case ThrowWeaponMultiplierBucket::Mace2H:
            return ThrowImpactSoundSlot::Mace2H;
        case ThrowWeaponMultiplierBucket::Spear1H:
            return ThrowImpactSoundSlot::Spear1H;
        case ThrowWeaponMultiplierBucket::Spear2H:
            return ThrowImpactSoundSlot::Spear2H;
        case ThrowWeaponMultiplierBucket::None:
        default:
            return ThrowImpactSoundSlot::Sword1H;
        }
    }

    inline bool PlayThrowHitSound(
        ThrowImpactSoundSlot slot,
        const RE::NiPoint3& hitPos,
        const char* label,
        RE::NiAVObject* followObject = nullptr,
        bool player3DFallback = true)
    {
        const auto settings = ::SaberThrow::Settings::Get();
        const std::size_t index = GetHitSoundIndex(slot);

        if (index >= settings.impactSoundIDs.size() ||
            index >= settings.impactSoundPlugins.size()) {
            return false;
        }

        const RE::FormID soundID = settings.impactSoundIDs[index];
        if (soundID == 0) {
            return false;
        }

        return PlaySoundByFormAtPos(
            soundID,
            settings.impactSoundPlugins[index],
            hitPos,
            label,
            followObject,
            player3DFallback);
    }

    inline bool PlayThrownActorHitSound(
        RE::TESObjectREFR* thrownRef,
        RE::Actor* hitActor,
        const RE::NiPoint3& hitPos)
    {
        if (!thrownRef) {
            return false;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        auto* baseObject = thrownRef->GetObjectReference();
        auto* weapon = baseObject ? baseObject->As<RE::TESObjectWEAP>() : nullptr;
        auto* shield = weapon || !baseObject ? nullptr : baseObject->As<RE::TESObjectARMO>();
        auto* torch = (!weapon && !shield && IsThrownTorch(thrownRef)) ?
            baseObject :
            nullptr;

        ThrowImpactSoundSlot slot = ThrowImpactSoundSlot::Sword1H;
        const char* label = "actor impact";

        if (shield) {
            slot = ThrowImpactSoundSlot::Shield;
            label = "shield actor impact";
        }
        else if (torch) {
            slot = ThrowImpactSoundSlot::Torch;
            label = "torch actor impact";
        }
        else {
            slot = GetThrowHitSoundSlot(weapon, g_state.noReturnSpearMode, settings);
            label = "weapon actor impact";
        }

        RE::NiAVObject* followObject = hitActor ? hitActor->Get3D(false) : nullptr;
        const RE::NiPoint3 soundPos = hitActor ? hitActor->GetPosition() : hitPos;

        return PlayThrowHitSound(
            slot,
            soundPos,
            label,
            followObject,
            false);
    }

    inline constexpr std::int32_t kGeomSoundLevel = 100;

    inline bool CreateGeomHitEvent(
        RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef || !IsThrowFromPlayer()) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!player || !vm) {
            return false;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            return false;
        }

        const RE::VMHandle handle = handlePolicy->GetHandleForObject(
            RE::FormType::Reference,
            thrownRef);
        if (handle == handlePolicy->EmptyHandle()) {
            return false;
        }

        auto args = std::unique_ptr<RE::BSScript::IFunctionArguments>(
            RE::MakeFunctionArguments(
                static_cast<RE::Actor*>(player),
                static_cast<std::int32_t>(kGeomSoundLevel)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        const bool dispatched = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("ObjectReference"),
            RE::BSFixedString("CreateDetectionEvent"),
            args.get(),
            callback);

        SKSE::log::debug(
            "[SaberThrow] player geometry-hit detection event {} at thrown ref 0x{:08X}, soundLevel={}",
            dispatched ? "dispatched" : "failed to dispatch",
            thrownRef->GetFormID(),
            kGeomSoundLevel);

        return dispatched;
    }

    inline bool PlayThrownGeomHitSound(
        const RE::NiPoint3& hitPos,
        RE::TESObjectREFR* thrownRef = nullptr)
    {
        (void)CreateGeomHitEvent(thrownRef);

        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::NiAVObject* followObject = player ? player->Get3D(false) : nullptr;
        const RE::NiPoint3 soundPos = player ? player->GetPosition() : hitPos;

        return PlayThrowHitSound(
            ThrowImpactSoundSlot::Geometry,
            soundPos,
            "geometry impact",
            followObject,
            true);
    }

    inline float ClampWeaponThrowMult(float value)
    {
        if (!std::isfinite(value)) {
            return 1.0f;
        }

        return std::clamp(value, 0.0f, 100.0f);
    }

    struct ThrowWeaponKeywordMultiplierOverrides
    {
        bool hasDamage{ false };
        float damage{ 1.0f };
        bool hasSpeed{ false };
        float speed{ 1.0f };
        bool hasDistance{ false };
        float distance{ 1.0f };
    };

    inline ThrowWeaponKeywordMultiplierOverrides GetKeywordMults(
        RE::TESObjectWEAP* weapon)
    {
        ThrowWeaponKeywordMultiplierOverrides overrides{};
        if (!weapon) {
            return overrides;
        }

        constexpr std::string_view kDamagePrefix = "ThrowableWeaponsDamageMult";
        constexpr std::string_view kSpeedPrefix = "ThrowableWeaponsSpeedMult";
        constexpr std::string_view kDistancePrefix = "ThrowableWeaponsDistanceMult";

        const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(weapon);
        if (!keywordForm || !keywordForm->keywords || keywordForm->numKeywords == 0) {
            return overrides;
        }

        for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
            const char* editorIDRaw = GetKeywordID(keywordForm->keywords[i]);
            if (!editorIDRaw) {
                continue;
            }

            const std::string_view editorID{ editorIDRaw };
            float parsed = 1.0f;

            if (!overrides.hasDamage &&
                ParseKeywordFloat(editorID, kDamagePrefix, parsed)) {
                overrides.hasDamage = true;
                overrides.damage = ClampWeaponThrowMult(parsed);
            }
            else if (!overrides.hasSpeed &&
                ParseKeywordFloat(editorID, kSpeedPrefix, parsed)) {
                overrides.hasSpeed = true;
                overrides.speed = ClampWeaponThrowMult(parsed);
            }
            else if (!overrides.hasDistance &&
                ParseKeywordFloat(editorID, kDistancePrefix, parsed)) {
                overrides.hasDistance = true;
                overrides.distance = ClampWeaponThrowMult(parsed);
            }

            if (overrides.hasDamage && overrides.hasSpeed && overrides.hasDistance) {
                break;
            }
        }

        return overrides;
    }

    inline ThrowWeaponMultipliers ApplyKeywordMults(
        RE::TESObjectWEAP* weapon,
        ThrowWeaponMultipliers multipliers)
    {
        const auto overrides = GetKeywordMults(weapon);
        if (overrides.hasDamage) {
            multipliers.damage = overrides.damage;
        }
        if (overrides.hasSpeed) {
            multipliers.speed = overrides.speed;
        }
        if (overrides.hasDistance) {
            multipliers.distance = overrides.distance;
        }
        return multipliers;
    }

    inline ThrowWeaponMultipliers GetThrowShieldMults(
        const ::SaberThrow::Settings::Values& settings)
    {
        return {
            ClampWeaponThrowMult(settings.damageShield),
            ClampWeaponThrowMult(settings.speedShield),
            ClampWeaponThrowMult(settings.distanceShield)
        };
    }

    inline ThrowWeaponMultipliers GetThrowTorchMults(
        const ::SaberThrow::Settings::Values& settings)
    {
        return {
            ClampWeaponThrowMult(settings.damageTorch),
            ClampWeaponThrowMult(settings.speedTorch),
            ClampWeaponThrowMult(settings.distanceTorch)
        };
    }


    inline ThrowWeaponMultipliers GetWeaponThrowMults(
        RE::TESObjectWEAP* weapon,
        bool forceSpearMode = false)
    {
        const auto settings = ::SaberThrow::Settings::Get();
        const auto bucket = GetWeaponMultBucket(weapon, forceSpearMode, settings);

        ThrowWeaponMultipliers multipliers{};

        switch (bucket) {
        case ThrowWeaponMultiplierBucket::Sword1H:
            multipliers = { ClampWeaponThrowMult(settings.damageSword1H), ClampWeaponThrowMult(settings.speedSword1H), ClampWeaponThrowMult(settings.distanceSword1H) };
            break;
        case ThrowWeaponMultiplierBucket::Sword2H:
            multipliers = { ClampWeaponThrowMult(settings.damageSword2H), ClampWeaponThrowMult(settings.speedSword2H), ClampWeaponThrowMult(settings.distanceSword2H) };
            break;
        case ThrowWeaponMultiplierBucket::Dagger1H:
            multipliers = { ClampWeaponThrowMult(settings.damageDagger1H), ClampWeaponThrowMult(settings.speedDagger1H), ClampWeaponThrowMult(settings.distanceDagger1H) };
            break;
        case ThrowWeaponMultiplierBucket::Axe1H:
            multipliers = { ClampWeaponThrowMult(settings.damageAxe1H), ClampWeaponThrowMult(settings.speedAxe1H), ClampWeaponThrowMult(settings.distanceAxe1H) };
            break;
        case ThrowWeaponMultiplierBucket::Axe2H:
            multipliers = { ClampWeaponThrowMult(settings.damageAxe2H), ClampWeaponThrowMult(settings.speedAxe2H), ClampWeaponThrowMult(settings.distanceAxe2H) };
            break;
        case ThrowWeaponMultiplierBucket::Mace1H:
            multipliers = { ClampWeaponThrowMult(settings.damageMace1H), ClampWeaponThrowMult(settings.speedMace1H), ClampWeaponThrowMult(settings.distanceMace1H) };
            break;
        case ThrowWeaponMultiplierBucket::Mace2H:
            multipliers = { ClampWeaponThrowMult(settings.damageMace2H), ClampWeaponThrowMult(settings.speedMace2H), ClampWeaponThrowMult(settings.distanceMace2H) };
            break;
        case ThrowWeaponMultiplierBucket::Spear1H:
            multipliers = { ClampWeaponThrowMult(settings.damageSpear1H), ClampWeaponThrowMult(settings.speedSpear1H), ClampWeaponThrowMult(settings.distanceSpear1H) };
            break;
        case ThrowWeaponMultiplierBucket::Spear2H:
            multipliers = { ClampWeaponThrowMult(settings.damageSpear2H), ClampWeaponThrowMult(settings.speedSpear2H), ClampWeaponThrowMult(settings.distanceSpear2H) };
            break;
        case ThrowWeaponMultiplierBucket::None:
        default:
            break;
        }

        return ApplyKeywordMults(weapon, multipliers);
    }

    inline ThrowWeaponMultipliers ApplyNPCThrowMults(
        ThrowWeaponMultipliers multipliers,
        const ::SaberThrow::Settings::Values& settings)
    {
        multipliers.damage = ClampWeaponThrowMult(
            multipliers.damage * settings.npcDamageMult);
        multipliers.speed = ClampWeaponThrowMult(
            multipliers.speed * settings.npcSpeedMult);
        multipliers.distance = ClampWeaponThrowMult(
            multipliers.distance * settings.npcRangeMult);
        return multipliers;
    }

    inline float ApplyNPCThrowDmgMult(float damage)
    {
        if (damage <= 0.0f || !IsThrowFromNPC()) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const float multiplier = ClampWeaponThrowMult(
            settings.npcDamageMult);

        return damage * multiplier;
    }

    inline float GetThrowStamMult(
        RE::TESObjectWEAP* weapon,
        bool forceSpearMode,
        const ::SaberThrow::Settings::Values& settings)
    {
        const auto bucket = GetWeaponMultBucket(weapon, forceSpearMode, settings);

        switch (bucket) {
        case ThrowWeaponMultiplierBucket::Sword1H:
            return ClampWeaponThrowMult(settings.staminaSword1H);
        case ThrowWeaponMultiplierBucket::Sword2H:
            return ClampWeaponThrowMult(settings.staminaSword2H);
        case ThrowWeaponMultiplierBucket::Dagger1H:
            return ClampWeaponThrowMult(settings.staminaDagger1H);
        case ThrowWeaponMultiplierBucket::Axe1H:
            return ClampWeaponThrowMult(settings.staminaAxe1H);
        case ThrowWeaponMultiplierBucket::Axe2H:
            return ClampWeaponThrowMult(settings.staminaAxe2H);
        case ThrowWeaponMultiplierBucket::Mace1H:
            return ClampWeaponThrowMult(settings.staminaMace1H);
        case ThrowWeaponMultiplierBucket::Mace2H:
            return ClampWeaponThrowMult(settings.staminaMace2H);
        case ThrowWeaponMultiplierBucket::Spear1H:
            return ClampWeaponThrowMult(settings.staminaSpear1H);
        case ThrowWeaponMultiplierBucket::Spear2H:
            return ClampWeaponThrowMult(settings.staminaSpear2H);
        case ThrowWeaponMultiplierBucket::None:
        default:
            return 1.0f;
        }
    }

    inline float GetThrowShieldStamMult(
        const ::SaberThrow::Settings::Values& settings)
    {
        return ClampWeaponThrowMult(settings.staminaShield);
    }

    inline float GetThrowTorchStamMult(
        const ::SaberThrow::Settings::Values& settings)
    {
        return ClampWeaponThrowMult(settings.staminaTorch);
    }

    inline float GetWeaponThrowStamCost(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        const float baseCost = std::max(settings.staminaCost, 0.0f);
        if (!player || baseCost <= 0.0f) {
            return baseCost;
        }

        RE::TESObjectWEAP* weapon = nullptr;
        const bool preferLeftHand = PreferLHThrowWeapon(settings);
        if (preferLeftHand) {
            weapon = GetPlayerLHEquipWeapon(player);
            if (!weapon) {
                weapon = GetPlayerRHEquipWeapon(player);
            }
        }
        else {
            weapon = GetPlayerRHEquipWeapon(player);
            if (!weapon) {
                weapon = GetPlayerLHEquipWeapon(player);
            }
        }

        if (!weapon) {
            return baseCost;
        }

        const float staminaMultiplier = GetThrowStamMult(
            weapon,
            false,
            settings);

        return baseCost * staminaMultiplier;
    }

    inline float GetShieldThrowStamCost(
        RE::PlayerCharacter* player,
        const ::SaberThrow::Settings::Values& settings)
    {
        const float baseCost = std::max(settings.staminaCost, 0.0f);
        if (!player || baseCost <= 0.0f) {
            return baseCost;
        }

        if (GetPlayerShield(player)) {
            return baseCost * GetThrowShieldStamMult(settings);
        }

        if (GetPlayerTorch(player)) {
            return baseCost * GetThrowTorchStamMult(settings);
        }

        return baseCost;
    }

    inline float ApplyWeaponDmgMult(
        RE::TESObjectWEAP* weapon,
        float damage,
        bool forceSpearMode = false)
    {
        if (damage <= 0.0f) {
            return damage;
        }

        const auto multipliers = GetWeaponThrowMults(weapon, forceSpearMode);
        return std::max(kMinWeaponDamage, damage * multipliers.damage);
    }

    inline float ApplyThrowPerkDmg(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* weapon,
        float damage)
    {
        if (!player || !weapon || damage <= 0.0f) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const bool noReturnThrow = g_state.noReturnDynamic;
        const auto& damagePerks = noReturnThrow ?
            settings.noReturnPerks :
            settings.telekPerks;

        if (damagePerks.empty() || !PlayerHasAnyPerk(player, damagePerks)) {
            return damage;
        }

        const float increasePercent = std::clamp(
            noReturnThrow ?
                settings.noReturnPerkPct :
                settings.telekPerkPct,
            0.0f,
            1000.0f);
        const float damageMultiplier = 1.0f + (increasePercent * 0.01f);

        return std::max(kMinWeaponDamage, damage * damageMultiplier);
    }

    inline float ApplyThrowShieldDmgMult(
        RE::TESObjectARMO* shield,
        float damage)
    {
        if (!shield || damage <= 0.0f) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const auto multipliers = GetThrowShieldMults(settings);
        return std::max(kMinShieldDamage, damage * multipliers.damage);
    }

    inline float ApplyThrowTorchDmgMult(
        RE::TESBoundObject* torch,
        float damage)
    {
        if (!torch || damage <= 0.0f || !IsThrowTorchBase(torch)) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const auto multipliers = GetThrowTorchMults(settings);
        return std::max(kMinWeaponDamage, damage * multipliers.damage);
    }


    inline float GetWeaponDmgFallback(RE::TESObjectREFR* thrownRef)
    {
        auto* weapon = GetWeaponBase(thrownRef);
        const float baseDamage = weapon ? static_cast<float>(weapon->GetAttackDamage()) : kMinWeaponDamage;
        return std::max(kMinWeaponDamage, baseDamage * kWeaponDamageMult);
    }

    inline float ApplyShieldBashPerkDmg(
        RE::Actor* attacker,
        RE::Actor* target,
        float damage)
    {
        if (!attacker || damage <= 0.0f) {
            return damage;
        }

        float modifiedDamage = damage;

        if (target) {
            RE::BGSEntryPoint::HandleEntryPoint(
                RE::BGSEntryPoint::ENTRY_POINTS::kModBashingDamage,
                attacker,
                static_cast<RE::TESForm*>(target),
                &modifiedDamage);
        }
        else {
            RE::BGSEntryPoint::HandleEntryPoint(
                RE::BGSEntryPoint::ENTRY_POINTS::kModBashingDamage,
                attacker,
                &modifiedDamage);
        }

        if (!std::isfinite(modifiedDamage)) {
            return damage;
        }

        return std::max(0.0f, modifiedDamage);
    }

    inline float CalcShieldBashDmg(
        RE::PlayerCharacter* player,
        RE::Actor* target,
        RE::TESObjectARMO* shield,
        RE::BGSAttackData* bashAttackData)
    {
        if (!shield) {
            return kMinShieldDamage;
        }

        const float armorRating = std::max(0.0f, shield->GetArmorRating());
        const float blockSkill = player ?
            std::clamp(player->GetActorValue(RE::ActorValue::kBlock), 0.0f, 100.0f) :
            0.0f;

        const float skill01 = blockSkill / 100.0f;
        const float skillMultiplier =
            kShieldSkillMin +
            ((kShieldSkillMax - kShieldSkillMin) * skill01);

        float damage = armorRating * kShieldArmorMult * skillMultiplier;

        damage = ApplyShieldBashPerkDmg(player, target, damage);

        return std::max(kMinShieldDamage, damage);
    }

    inline float GetShieldBashDmg(RE::TESObjectREFR* thrownRef)
    {
        auto* shield = GetShieldBase(thrownRef);
        if (!shield) {
            return kMinWeaponDamage;
        }

        const float armorRating = std::max(0.0f, shield->GetArmorRating());
        const float bashDamage = armorRating *
            kShieldArmorMult *
            kShieldSkillMin;

        return std::max(kMinShieldDamage, bashDamage);
    }


}

#pragma once

#include "SaberThrow/SaberThrowWeaponProfile.hpp"

namespace SaberThrow
{
    inline float GetThrowDmgFallback(RE::TESObjectREFR* thrownRef)
    {
        return IsThrownShield(thrownRef) ?
            GetShieldBashDmg(thrownRef) :
            GetWeaponDmgFallback(thrownRef);
    }

    inline RE::TESObjectWEAP* GetWeaponImpactXPWeapon(RE::TESObjectREFR* thrownRef)
    {
        return g_state.weaponToReequip ? g_state.weaponToReequip : GetWeaponBase(thrownRef);
    }

    inline RE::TESObjectARMO* GetShieldImpactXPShield(RE::TESObjectREFR* thrownRef)
    {
        return GetShieldBase(thrownRef);
    }

    inline bool GetWeaponImpactSkill(RE::TESObjectWEAP* weapon, RE::ActorValue& outSkill)
    {
        if (!weapon) {
            return false;
        }

        if (weapon->IsOneHandedSword() ||
            weapon->IsOneHandedDagger() ||
            weapon->IsOneHandedAxe() ||
            weapon->IsOneHandedMace()) {
            outSkill = RE::ActorValue::kOneHanded;
            return true;
        }

        if (weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe()) {
            outSkill = RE::ActorValue::kTwoHanded;
            return true;
        }

        return false;
    }

    inline float GetWeaponImpactXPAmount(RE::TESObjectWEAP* weapon)
    {
        if (!weapon) {
            return 0.0f;
        }

        return std::max(0.0f, static_cast<float>(weapon->GetAttackDamage()));
    }

    inline float GetShieldImpactXPAmount(RE::TESObjectARMO* shield)
    {
        if (!shield) {
            return 0.0f;
        }

        const float armorRating = std::max(0.0f, shield->GetArmorRating());
        const float baseSkillUse = armorRating *
            kShieldArmorMult *
            kShieldSkillMin;
        return std::max(kMinShieldDamage, baseSkillUse);
    }

    inline void GrantWeaponImpactXP(
        RE::PlayerCharacter* player,
        RE::Actor* target,
        RE::TESObjectREFR* thrownRef,
        float healthDamage)
    {
        if (!player || !target || target == player || healthDamage <= 0.0f) {
            return;
        }

        if (!g_state.noReturnDynamic) {
            return;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const bool weaponXPOn = settings.weaponXPOn;
        const bool archeryXPOn = settings.archeryXPOn;
        const float weaponXPMult = std::clamp(settings.weaponXPMult, 0.0f, 100.0f);
        const float archeryXPMult = std::clamp(settings.archeryXPMult, 0.0f, 100.0f);

        if ((!weaponXPOn || weaponXPMult <= 0.0f) &&
            (!archeryXPOn || archeryXPMult <= 0.0f)) {
            return;
        }

        auto* xpWeapon = GetWeaponImpactXPWeapon(thrownRef);
        auto* xpShield = xpWeapon ? nullptr : GetShieldImpactXPShield(thrownRef);

        const bool isShieldBash = xpShield != nullptr;
        const float baseSkillXP = isShieldBash ?
            GetShieldImpactXPAmount(xpShield) :
            GetWeaponImpactXPAmount(xpWeapon);
        if (baseSkillXP <= 0.0f) {
            return;
        }

        RE::ActorValue weaponSkill{};
        const bool grantWeaponXP =
            weaponXPOn &&
            weaponXPMult > 0.0f &&
            (isShieldBash ?
                (weaponSkill = RE::ActorValue::kBlock, true) :
                GetWeaponImpactSkill(xpWeapon, weaponSkill));

        const bool grantArcheryXP = !isShieldBash && archeryXPOn && archeryXPMult > 0.0f;
        const float weaponXPGain = baseSkillXP * weaponXPMult;
        const float archeryXPGain = baseSkillXP * archeryXPMult;

        if ((!grantWeaponXP || weaponXPGain <= 0.0f) &&
            (!grantArcheryXP || archeryXPGain <= 0.0f)) {
            return;
        }

        if (auto* taskInterface = SKSE::GetTaskInterface()) {
            taskInterface->AddTask([
                grantWeaponXP,
                weaponSkill,
                weaponXPGain,
                grantArcheryXP,
                archeryXPGain]() {
                    if (auto* pc = RE::PlayerCharacter::GetSingleton()) {
                        if (grantWeaponXP && weaponXPGain > 0.0f) {
                            pc->AddSkillExperience(weaponSkill, weaponXPGain);
                        }
                        if (grantArcheryXP && archeryXPGain > 0.0f) {
                            pc->AddSkillExperience(RE::ActorValue::kArchery, archeryXPGain);
                        }
                    }
                });
        }
        else {
            if (grantWeaponXP && weaponXPGain > 0.0f) {
                player->AddSkillExperience(weaponSkill, weaponXPGain);
            }
            if (grantArcheryXP && archeryXPGain > 0.0f) {
                player->AddSkillExperience(RE::ActorValue::kArchery, archeryXPGain);
            }
        }
    }

    inline bool MakeThrownWeaponHit(
        RE::Actor* attacker,
        RE::Actor* target,
        RE::TESObjectREFR* thrownRef,
        const RE::NiPoint3& hitPos,
        const RE::NiPoint3& hitDir,
        RE::HitData& outHitData)
    {
        auto* weapon = GetWeaponBase(thrownRef);
        if (!attacker || !target || !weapon) {
            return false;
        }

        RE::InventoryEntryData weaponEntry{ weapon, 1 };

        const bool isLeftHand = g_state.hasThrowHand && g_state.throwHandLeft;
        outHitData.Populate(attacker, target, &weaponEntry, isLeftHand);
        outHitData.hitPosition = hitPos;
        outHitData.hitDirection = hitDir;
        if (thrownRef) {
            outHitData.sourceRef = thrownRef->CreateRefHandle();
        }

        return true;
    }

    inline float GetPhysDmgTakenMult(const RE::HitData& hitData)
    {
        const float rawPhysicalDamage = hitData.physicalDamage;
        if (!std::isfinite(rawPhysicalDamage) || rawPhysicalDamage <= 0.0f) {
            return 1.0f;
        }

        const float resistedPhysical = std::max(0.0f, hitData.resistedPhysicalDamage);
        const float resistedTyped = std::max(0.0f, hitData.resistedTypedDamage);
        const float physicalTaken = std::clamp(
            rawPhysicalDamage - resistedPhysical - resistedTyped,
            0.0f,
            rawPhysicalDamage);

        return std::clamp(physicalTaken / rawPhysicalDamage, 0.0f, 1.0f);
    }

    inline float ApplyPhysDmgTakenMult(
        float rawDamage,
        float nativePhysMult)
    {
        if (!std::isfinite(rawDamage) || rawDamage <= 0.0f) {
            return rawDamage;
        }

        if (!std::isfinite(nativePhysMult)) {
            return rawDamage;
        }

        const float clampedMultiplier = std::clamp(nativePhysMult, 0.0f, 1.0f);
        return std::max(0.0f, rawDamage * clampedMultiplier);
    }

    inline void SetPhysDmgFields(
        RE::HitData& hitData,
        float rawPhysicalDamage,
        float finalPhysical)
    {
        rawPhysicalDamage = std::max(0.0f, rawPhysicalDamage);
        finalPhysical = std::clamp(finalPhysical, 0.0f, rawPhysicalDamage);

        hitData.physicalDamage = rawPhysicalDamage;
        hitData.resistedPhysicalDamage = std::max(0.0f, rawPhysicalDamage - finalPhysical);
        hitData.resistedTypedDamage = 0.0f;
        hitData.totalDamage = finalPhysical;
    }

    inline RE::BGSAttackData* FindShieldBashAttack(RE::PlayerCharacter* player)
    {
        auto* playerBase = player ? player->GetActorBase() : nullptr;
        if (!playerBase || !playerBase->attackDataMap) {
            return nullptr;
        }

        auto* attackDataMap = playerBase->attackDataMap.get();
        if (!attackDataMap) {
            return nullptr;
        }

        for (auto& entry : attackDataMap->attackDataMap) {
            auto& attackData = entry.second;
            if (attackData && attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack)) {
                return attackData.get();
            }
        }

        return nullptr;
    }

    inline bool MakeShieldBashHit(
        RE::Actor* target,
        RE::TESObjectREFR* thrownRef,
        const RE::NiPoint3& hitPos,
        const RE::NiPoint3& hitDir,
        RE::HitData& outHitData,
        RE::BGSAttackData* bashAttackData = nullptr)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* shield = GetShieldBase(thrownRef);
        if (!player || !target || !shield) {
            return false;
        }

        if (!bashAttackData) {
            bashAttackData = FindShieldBashAttack(player);
        }

        RE::InventoryEntryData shieldEntry{ shield, 1 };

        outHitData.Populate(player, target, &shieldEntry, true);

        const float nativePhysMult =
            GetPhysDmgTakenMult(outHitData);

        outHitData.hitPosition = hitPos;
        outHitData.hitDirection = hitDir;
        outHitData.flags.set(RE::HitData::Flag::kBash, RE::HitData::Flag::kMeleeAttack);
        outHitData.skill = RE::ActorValue::kBlock;

        if (bashAttackData) {
            outHitData.attackData = RE::NiPointer<RE::BGSAttackData>{ bashAttackData };
            outHitData.attackDataSpell = bashAttackData->data.attackSpell;

            if (bashAttackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack)) {
                outHitData.flags.set(RE::HitData::Flag::kPowerAttack);
            }
        }

        if (thrownRef) {
            outHitData.sourceRef = thrownRef->CreateRefHandle();
        }

        const float fallbackDamage = CalcShieldBashDmg(
            player,
            target,
            shield,
            bashAttackData);
        SetPhysDmgFields(
            outHitData,
            fallbackDamage,
            ApplyPhysDmgTakenMult(
                fallbackDamage,
                nativePhysMult));

        return true;
    }

    inline constexpr bool kApplyShieldSpells = false;

    inline RE::SpellItem* GetTorchBashSpell()
    {
        if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
            if (auto* spell = dataHandler->LookupForm<RE::SpellItem>(
                kTorchBashSpellID,
                kSkyrimPluginName)) {
                return spell;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kTorchBashSpellID)) {
            return rawForm->As<RE::SpellItem>();
        }

        return nullptr;
    }

    inline bool ApplyThrownBashSpell(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESBoundObject* sourceObject,
        RE::SpellItem* spell,
        const RE::NiPoint3& hitPos)
    {
        if (!target || !caster || !sourceObject || !spell) {
            return false;
        }

        auto* magicTarget = target->AsMagicTarget();
        bool anyApplied = false;

        for (auto* effect : spell->effects) {
            if (!effect || !effect->baseEffect) {
                continue;
            }

            RE::MagicTarget::AddTargetData data{};
            data.caster = caster;
            data.magicItem = spell;
            data.effect = effect;
            data.source = sourceObject;
            data.explosionPoint = hitPos;
            data.magnitude = effect->effectItem.magnitude;
            data.power = 1.0f;
            data.castingSource = RE::MagicSystem::CastingSource::kLeftHand;
            data.areaTarget = false;
            data.dualCasted = false;

            anyApplied = magicTarget->AddTarget(data) || anyApplied;
        }

        return anyApplied;
    }

    inline bool ApplyShieldBashSpell(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESObjectARMO* shield,
        RE::SpellItem* spell,
        const RE::NiPoint3& hitPos)
    {
        return ApplyThrownBashSpell(
            target,
            caster,
            shield,
            spell,
            hitPos);
    }

    inline bool ApplyTorchBashFireSpell(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESBoundObject* torch,
        const RE::NiPoint3& hitPos)
    {
        auto* spell = GetTorchBashSpell();
        if (!spell) {
            static bool warnedTorchSpell = false;
            if (!warnedTorchSpell) {
                warnedTorchSpell = true;
                SKSE::log::warn(
                    "[SaberThrow] Could not resolve Skyrim torch bash fire spell 0x{:06X}; skipping thrown torch bash fire application.",
                    kTorchBashSpellID);
            }
            return false;
        }

        return ApplyThrownBashSpell(
            target,
            caster,
            torch,
            spell,
            hitPos);
    }

    inline bool ApplyShieldAttackSpell(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESObjectARMO* shield,
        RE::BGSAttackData* bashAttackData,
        const RE::NiPoint3& hitPos)
    {
        return bashAttackData ?
            ApplyShieldBashSpell(
                target,
                caster,
                shield,
                bashAttackData->data.attackSpell,
                hitPos) :
            false;
    }

    inline bool ApplyShieldBashPerkSpell(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESObjectARMO* shield,
        const RE::NiPoint3& hitPos)
    {
        if (!target || !caster || !shield) {
            return false;
        }

        RE::SpellItem* bashingSpell = nullptr;

        RE::BGSEntryPoint::HandleEntryPoint(
            RE::BGSEntryPoint::ENTRY_POINTS::kApplyBashingSpell,
            caster,
            static_cast<RE::TESForm*>(target),
            &bashingSpell);

        return ApplyShieldBashSpell(
            target,
            caster,
            shield,
            bashingSpell,
            hitPos);
    }

    inline void SendThrownHitStaggerAnim(RE::Actor* actor)
    {
        if (!actor) {
            return;
        }

        if constexpr (!kSendHitStagger) {
            return;
        }

        constexpr float stagger = 0.33f;

        actor->SetGraphVariableFloat(RE::BSFixedString("staggerMagnitude"), stagger);
        actor->NotifyAnimationGraph(RE::BSFixedString("staggerStart"));
    }

    inline void TryShieldBashStagger(RE::Actor* actor)
    {
        SendThrownHitStaggerAnim(actor);
    }

    inline float GetDmgFromHitOrFallback(const RE::HitData& hitData, RE::TESObjectREFR* thrownRef)
    {
        float damage = hitData.totalDamage;
        if (damage <= 0.0f) {
            damage = hitData.physicalDamage - hitData.resistedPhysicalDamage - hitData.resistedTypedDamage;
        }
        if (damage <= 0.0f) {
            damage = GetThrowDmgFallback(thrownRef);
        }

        return std::max(kMinWeaponDamage, damage * kWeaponDamageMult);
    }

    inline float ApplyShieldBashSkillDmg(RE::PlayerCharacter* player, float damage)
    {
        if (!player || damage <= 0.0f) {
            return damage;
        }

        const float blockSkill = std::clamp(
            player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock),
            0.0f,
            100.0f);

        const float skill01 = blockSkill / 100.0f;
        const float skillMultiplier =
            kShieldSkillMin +
            ((kShieldSkillMax - kShieldSkillMin) * skill01);

        return std::max(kMinShieldDamage, damage * skillMultiplier);
    }

    inline float ApplyThrowSkillDmg(RE::PlayerCharacter* player, float damage)
    {
        if (!player || damage <= 0.0f) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();

        const bool useArcheryScaling = g_state.noReturnDynamic;

        const float damageAt0Skill = std::clamp(
            useArcheryScaling ? settings.archeryDamage0 : settings.altDamage0,
            0.0f,
            10.0f);

        const float damageAt100Skill = std::clamp(
            useArcheryScaling ? settings.archeryDamage100 : settings.altDamage100,
            0.0f,
            10.0f);

        const RE::ActorValue skillActorValue = useArcheryScaling
            ? RE::ActorValue::kArchery
            : RE::ActorValue::kAlteration;

        const float skill = std::clamp(
            player->AsActorValueOwner()->GetActorValue(skillActorValue),
            0.0f,
            100.0f);

        const float skill01 = skill / 100.0f;
        const float skillMultiplier =
            damageAt0Skill + ((damageAt100Skill - damageAt0Skill) * skill01);

        return std::max(kMinWeaponDamage, damage * skillMultiplier);
    }

    inline bool IsThrownWeaponHitHead(RE::Actor* target, const RE::NiPoint3& hitPos)
    {
        if (!target || !IsFinitePoint(hitPos)) {
            return false;
        }

        if (!target->Get3D(false) && !target->Get3D(true)) {
            return false;
        }

        auto* neckNode = target->GetNodeByName("NPC Neck [Neck]");
        if (!neckNode) {
            return false;
        }

        const RE::NiPoint3 neckPos = neckNode->world.translate;
        if (!IsFinitePoint(neckPos)) {
            return false;
        }

        return hitPos.z >= neckPos.z;
    }

    inline float ApplyHeadshotDmgMult(
        RE::Actor* target,
        const RE::NiPoint3& hitPos,
        float damage)
    {
        if (!target || damage <= 0.0f) {
            return damage;
        }

        const auto settings = ::SaberThrow::Settings::Get();
        const float multiplier = std::clamp(settings.headshotMult, 0.0f, 100.0f);

        if (std::fabs(multiplier - 1.0f) <= 0.000001f) {
            return damage;
        }

        if (!IsThrownWeaponHitHead(target, hitPos)) {
            return damage;
        }

        return std::max(kMinWeaponDamage, damage * multiplier);
    }

    inline constexpr bool kIgnoreDifficulty = false;

    inline float ApplyCalcdHealthDmg(RE::Actor* actor, RE::Actor* attacker, float damage)
    {
        if (!actor || damage <= 0.0f) {
            return 0.0f;
        }

        const float healthBefore = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
        const bool didDamage = actor->DoDamage(damage, attacker, kIgnoreDifficulty);
        const float healthAfter = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
        (void)didDamage;

        return std::max(0.0f, healthBefore - healthAfter);
    }

    inline bool HitHasSneakFlag(const RE::HitData& hitData)
    {
        return hitData.flags.any(RE::HitData::Flag::kSneakAttack);
    }

    inline bool IsSneakHit(const RE::HitData& hitData, float healthDamage)
    {
        (void)healthDamage;

        return HitHasSneakFlag(hitData) ||
            hitData.sneakAttackBonus > 1.0f ||
            hitData.bonusHealthDamageMult > 1.0f;
    }

    inline const char* GetGameString(const char* settingName, const char* fallback)
    {
        auto* gameSettings = RE::GameSettingCollection::GetSingleton();
        if (!gameSettings || !settingName || settingName[0] == '\0') {
            return fallback;
        }

        auto* setting = gameSettings->GetSetting(settingName);
        if (!setting) {
            return fallback;
        }

        const char* value = setting->GetString();
        return (value && value[0] != '\0') ? value : fallback;
    }

    inline const char* GetSneakMainSpacing(const char* messageStart)
    {
        if (!messageStart || messageStart[0] == '\0') {
            return "";
        }

        const unsigned char lastChar = static_cast<unsigned char>(messageStart[std::strlen(messageStart) - 1]);
        return std::isspace(lastChar) ? "" : " ";
    }

    inline float GetSneakNativeMult(const RE::HitData& hitData)
    {
        if (hitData.sneakAttackBonus > 1.0f) {
            return hitData.sneakAttackBonus;
        }

        if (hitData.bonusHealthDamageMult > 1.0f) {
            return hitData.bonusHealthDamageMult;
        }

        if (HitHasSneakFlag(hitData)) {
            return 2.0f;
        }

        return 1.0f;
    }

    inline float GetSneakBonusMult()
    {
        const auto settings = ::SaberThrow::Settings::Get();
        return std::clamp(settings.sneakBonusMult, 0.0f, 100.0f);
    }

    inline float NormalizeSneakBonus(float configMult)
    {
        constexpr float kOneThird = 1.0f / 3.0f;
        if (std::abs(configMult - 0.33f) <= 0.005f ||
            std::abs(configMult - 0.333f) <= 0.005f ||
            std::abs(configMult - kOneThird) <= 0.005f) {
            return kOneThird;
        }

        return configMult;
    }

    inline float RoundSneakMultToStep(float value, float step)
    {
        if (value <= 0.0f || step <= 0.0f || !std::isfinite(value) || !std::isfinite(step)) {
            return value;
        }

        return std::round(value / step) * step;
    }

    inline float GetSneakDisplayMult(const RE::HitData& hitData)
    {
        const float nativeMult = GetSneakNativeMult(hitData);
        if (nativeMult <= 1.0f) {
            return nativeMult;
        }

        const float configMult = NormalizeSneakBonus(
            GetSneakBonusMult());

        if (configMult <= 0.0f) {
            return 1.0f;
        }

        if (configMult >= 1.0f) {
            return 1.0f + ((nativeMult - 1.0f) * configMult);
        }

        const float prettyStep = configMult;

        if (nativeMult <= 2.0f) {
            const float lowValue = 1.0f + ((nativeMult - 1.0f) * configMult);
            return std::max(1.0f, RoundSneakMultToStep(lowValue, prettyStep));
        }

        constexpr float kAnchorNativeLow = 3.0f;
        constexpr float kAnchorNativeHigh = 30.0f;

        const float lowAnchor = std::max(
            2.0f,
            RoundSneakMultToStep(kAnchorNativeLow * configMult, prettyStep));

        const float highAnchor = std::max(
            lowAnchor,
            RoundSneakMultToStep(kAnchorNativeHigh * configMult, prettyStep));

        if (nativeMult <= kAnchorNativeHigh) {
            const float t = std::clamp(
                (nativeMult - kAnchorNativeLow) / (kAnchorNativeHigh - kAnchorNativeLow),
                0.0f,
                1.0f);

            const float remapped = lowAnchor + ((highAnchor - lowAnchor) * t);
            return std::max(1.0f, RoundSneakMultToStep(remapped, prettyStep));
        }

        const float scaledHighNative = RoundSneakMultToStep(
            nativeMult * configMult,
            prettyStep);

        return std::max(highAnchor, scaledHighNative);
    }

    inline std::string FormatSneakMult(float multiplier)
    {
        if (!std::isfinite(multiplier)) {
            multiplier = 1.0f;
        }

        multiplier = std::max(1.0f, multiplier);

        const float roundedInteger = std::round(multiplier);
        if (std::fabs(multiplier - roundedInteger) <= 0.005f) {
            char wholeText[32]{};
            std::snprintf(wholeText, sizeof(wholeText), "%.1f", roundedInteger);
            return wholeText;
        }

        const float thirds = multiplier * 3.0f;
        const float roundedThirds = std::round(thirds);
        if (std::fabs(thirds - roundedThirds) <= 0.015f) {
            const int whole = static_cast<int>(std::floor(roundedThirds / 3.0f));
            const int remainder = static_cast<int>(roundedThirds) - (whole * 3);
            if (remainder == 0) {
                return std::to_string(whole) + ".0";
            }
            if (remainder == 1) {
                return std::to_string(whole) + ".33";
            }
            return std::to_string(whole) + ".66";
        }

        char text[32]{};
        std::snprintf(text, sizeof(text), "%.2f", multiplier);

        std::string value{ text };
        while (!value.empty() && value.back() == '0') {
            value.pop_back();
        }
        if (!value.empty() && value.back() == '.') {
            value.pop_back();
        }

        return value.empty() ? std::string{ "1.0" } : value;
    }

    inline float ApplySneakBonusMult(
        const RE::HitData& hitData,
        float damage)
    {
        if (damage <= 0.0f || !IsSneakHit(hitData, damage)) {
            return damage;
        }

        const float nativeMult = GetSneakNativeMult(hitData);
        if (nativeMult <= 1.0f) {
            return damage;
        }

        const float sneakMult = GetSneakDisplayMult(hitData);
        const float damageScale = sneakMult / nativeMult;
        return std::max(kMinWeaponDamage, damage * damageScale);
    }

    inline RE::BGSSoundDescriptorForm* GetUISneakSoundForm()
    {
        constexpr RE::FormID kSneakSoundID = 0x000C06CE;
        constexpr const char* kSkyrimMasterName = "Skyrim.esm";

        if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
            if (auto* soundForm = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(
                kSneakSoundID,
                kSkyrimMasterName)) {
                return soundForm;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kSneakSoundID)) {
            return rawForm->As<RE::BGSSoundDescriptorForm>();
        }

        return nullptr;
    }

    inline bool PlayUISneakSoundRecord(const RE::NiPoint3& position)
    {
        auto* soundForm = GetUISneakSoundForm();
        if (!soundForm) {
            SKSE::log::warn("[SaberThrow] UISneakAttack sound lookup failed: Skyrim.esm 0x000C06CE.");
            return false;
        }

        auto* audioManager = RE::BSAudioManager::GetSingleton();
        if (!audioManager) {
            SKSE::log::warn("[SaberThrow] UISneakAttack sound failed: no BSAudioManager.");
            return false;
        }

        RE::BSSoundHandle handle{};
        handle.soundID = static_cast<std::uint32_t>(-1);
        handle.assumeSuccess = false;
        *reinterpret_cast<std::uint32_t*>(&handle.state) = 0;

        (void)audioManager->GetSoundHandle(
            handle,
            static_cast<RE::BSISoundDescriptor*>(soundForm),
            16);

        handle.SetPosition(position);

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* player3D = player->Get3D(false)) {
                handle.SetObjectToFollow(player3D);
            }
        }

        handle.Play();
        return true;
    }

    inline void ShowDebugNotifyNoSound(const char* message)
    {
        if (!message || message[0] == '\0') {
            return;
        }

        using DebugNotification_t = void (*)(const char*, const char*, bool);
        static REL::Relocation<DebugNotification_t> debugNotification{
            RELOCATION_ID(52050, 52933)
        };

        debugNotification(message, nullptr, false);
    }

    inline void ShowLowStamNotify()
    {
        constexpr float kStaminaCooldown = 2.0f;
        static std::chrono::steady_clock::time_point lastNotify{};

        const auto now = std::chrono::steady_clock::now();
        if (lastNotify != std::chrono::steady_clock::time_point{}) {
            const float elapsedSeconds = std::chrono::duration<float>(now - lastNotify).count();
            if (elapsedSeconds < kStaminaCooldown) {
                return;
            }
        }

        lastNotify = now;
        ShowDebugNotifyNoSound("You lack the stamina required for this action");
    }

    inline void NotifyMissingThrowPerk()
    {
        constexpr float kPerkCooldown = 2.0f;
        static std::chrono::steady_clock::time_point lastNotify{};

        const auto now = std::chrono::steady_clock::now();
        if (lastNotify != std::chrono::steady_clock::time_point{}) {
            const float elapsedSeconds = std::chrono::duration<float>(now - lastNotify).count();
            if (elapsedSeconds < kPerkCooldown) {
                return;
            }
        }

        lastNotify = now;
        ShowDebugNotifyNoSound("You lack the skill to throw this weapon");
    }

    inline void ShowSneakNotify(
        const RE::HitData& hitData,
        float healthDamage,
        const RE::NiPoint3& hitPos)
    {
        if (!IsSneakHit(hitData, healthDamage)) {
            SKSE::log::debug(
                "[SaberThrow] Sneak attack notification skipped: no HitData sneak indicator. actualHealthDamage={}, sneakAttackBonus={}, bonusHealthDamageMult={}",
                healthDamage,
                hitData.sneakAttackBonus,
                hitData.bonusHealthDamageMult);
            return;
        }

        const char* messageStart = GetGameString(
            "sSuccessfulSneakAttackMain",
            "Sneak attack for");

        const char* messageEnd = GetGameString(
            "sSuccessfulSneakAttackEnd",
            "X damage!");

        const float sneakMult = GetSneakDisplayMult(hitData);
        const std::string sneakMultText =
            FormatSneakMult(sneakMult);

        char message[256]{};
        std::snprintf(
            message,
            sizeof(message),
            "%s%s%s%s",
            messageStart,
            GetSneakMainSpacing(messageStart),
            sneakMultText.c_str(),
            messageEnd);

        SKSE::log::info(
            "[SaberThrow] Showing sneak attack notification: '{}', actualHealthDamage={}, nativeSneakMult={}, configuredSneakBonusMult={}, effectiveSneakMult={}, effectiveSneakMultText='{}', sneakAttackBonus={}, bonusHealthDamageMult={}",
            message,
            healthDamage,
            GetSneakNativeMult(hitData),
            GetSneakBonusMult(),
            sneakMult,
            sneakMultText,
            hitData.sneakAttackBonus,
            hitData.bonusHealthDamageMult);

        ShowDebugNotifyNoSound(message);
        PlayUISneakSoundRecord(hitPos);
    }

    inline bool SendAssaultAlarm(RE::Actor* victim)
    {
        if (!victim) {
            return false;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            SKSE::log::debug(
                "[SaberThrow] SendAssaultAlarm skipped for 0x{:08X}: Papyrus VM unavailable.",
                victim->GetFormID());
            return false;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            SKSE::log::debug(
                "[SaberThrow] SendAssaultAlarm skipped for 0x{:08X}: VM handle policy unavailable.",
                victim->GetFormID());
            return false;
        }

        const RE::VMHandle handle = handlePolicy->GetHandleForObject(RE::FormType::ActorCharacter, victim);
        if (handle == handlePolicy->EmptyHandle()) {
            SKSE::log::debug(
                "[SaberThrow] SendAssaultAlarm skipped for 0x{:08X}: could not resolve Actor VM handle.",
                victim->GetFormID());
            return false;
        }

        auto args = std::unique_ptr<RE::BSScript::IFunctionArguments>(RE::MakeFunctionArguments());
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        const bool dispatched = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("Actor"),
            RE::BSFixedString("SendAssaultAlarm"),
            args.get(),
            callback);

        SKSE::log::debug(
            "[SaberThrow] SendAssaultAlarm {} for hit actor 0x{:08X}.",
            dispatched ? "dispatched" : "failed to dispatch",
            victim->GetFormID());

        return dispatched;
    }

    inline void SendTESHitEvent(
        RE::Actor* target,
        RE::Actor* attacker,
        RE::TESForm* sourceForm,
        const RE::HitData* hitData = nullptr,
        float healthDamage = 0.0f)
    {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder || !target || !attacker) {
            return;
        }

        RE::TESHitEvent::Flag flags = RE::TESHitEvent::Flag::kNone;
        if (hitData && IsSneakHit(*hitData, healthDamage)) {
            flags = RE::TESHitEvent::Flag::kSneakAttack;
        }

        const RE::FormID sourceFormID = sourceForm ? sourceForm->GetFormID() : 0;
        RE::TESHitEvent event(target, attacker, sourceFormID, 0, flags);
        holder->SendEvent(&event);

    }

    inline RE::TESForm* GetWorldHitSource(RE::TESObjectREFR* thrownRef)
    {
        if (!thrownRef) {
            return nullptr;
        }

        if (auto* weapon = GetWeaponBase(thrownRef)) {
            return weapon;
        }
        if (auto* shield = GetShieldBase(thrownRef)) {
            return shield;
        }
        if (IsThrownTorch(thrownRef)) {
            return thrownRef->GetObjectReference();
        }

        return thrownRef->GetObjectReference();
    }

    inline void SendPlayerWorldHitEvent(
        RE::TESObjectREFR* target,
        RE::TESObjectREFR* thrownRef)
    {
        if (!target || !thrownRef || target->As<RE::Actor>() || !IsThrowFromPlayer()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!player || !holder) {
            return;
        }

        auto* sourceForm = GetWorldHitSource(thrownRef);
        const RE::FormID sourceFormID = sourceForm ? sourceForm->GetFormID() : 0;

        RE::TESHitEvent event(
            target,
            player,
            sourceFormID,
            0,
            RE::TESHitEvent::Flag::kNone);
        holder->SendEvent(&event);
    }

    inline RE::EnchantmentItem* GetThrowDummyEnchant()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (dataHandler) {
            if (auto* enchantment = dataHandler->LookupForm<RE::EnchantmentItem>(
                kDummyEnchantLocal,
                kDummyEnchantPlugin)) {
                return enchantment;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kDummyEnchantFullID)) {
            if (auto* enchantment = rawForm->As<RE::EnchantmentItem>()) {
                return enchantment;
            }
        }

        if (auto* rawForm = RE::TESForm::LookupByID(kDummyEnchantLocal)) {
            return rawForm->As<RE::EnchantmentItem>();
        }

        return nullptr;
    }

    inline bool ApplyThrowDummyEnchant(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESBoundObject* sourceForm,
        const RE::NiPoint3& hitPos,
        RE::MagicSystem::CastingSource castingSource)
    {
        if (!target || !caster) {
            return false;
        }

        auto* enchantment = GetThrowDummyEnchant();
        if (!enchantment) {
            static bool warnedDummy = false;
            if (!warnedDummy) {
                warnedDummy = true;
                SKSE::log::warn(
                    "[SaberThrow] Could not resolve dummy throw enchant 0x{:06X}/'{}'; skipping pre-enchant throw marker application.",
                    kDummyEnchantLocal,
                    kDummyEnchantPlugin);
            }
            return false;
        }

        auto* magicTarget = target->AsMagicTarget();
        bool anyApplied = false;
        std::uint32_t attempted = 0;

        for (auto* effect : enchantment->effects) {
            if (!effect || !effect->baseEffect) {
                continue;
            }

            ++attempted;

            RE::MagicTarget::AddTargetData data{};
            data.caster = caster;
            data.magicItem = enchantment;
            data.effect = effect;
            data.source = sourceForm;
            data.explosionPoint = hitPos;
            data.magnitude = effect->effectItem.magnitude;
            data.power = 1.0f;
            data.castingSource = castingSource;
            data.areaTarget = false;
            data.dualCasted = false;

            const bool applied = magicTarget->AddTarget(data);
            anyApplied = anyApplied || applied;
        }

        SKSE::log::debug(
            "[SaberThrow] Applied dummy throw enchant 0x{:08X}: source=0x{:08X}, target=0x{:08X}, attemptedEffects={}, anyApplied={}",
            enchantment->GetFormID(),
            sourceForm ? sourceForm->GetFormID() : 0,
            target->GetFormID(),
            attempted,
            anyApplied);

        return anyApplied;
    }

    inline bool ApplyWeaponBaseEnchant(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESObjectWEAP* weapon,
        const RE::NiPoint3& hitPos,
        RE::EnchantmentItem* instanceEnchantment = nullptr)
    {
        if (!target || !caster || !weapon) {
            return false;
        }

        auto* enchantment = instanceEnchantment ? instanceEnchantment : weapon->formEnchanting;
        if (!enchantment) {
            return false;
        }
        auto* magicTarget = target->AsMagicTarget();
        bool anyApplied = false;
        std::uint32_t attempted = 0;

        for (auto* effect : enchantment->effects) {
            if (!effect || !effect->baseEffect) {
                continue;
            }

            ++attempted;

            RE::MagicTarget::AddTargetData data{};
            data.caster = caster;
            data.magicItem = enchantment;
            data.effect = effect;
            data.source = weapon;
            data.explosionPoint = hitPos;
            data.magnitude = effect->effectItem.magnitude;
            data.power = 1.0f;
            data.castingSource = RE::MagicSystem::CastingSource::kRightHand;
            data.areaTarget = false;
            data.dualCasted = false;

            const bool applied = magicTarget->AddTarget(data);
            anyApplied = anyApplied || applied;

        }

        return anyApplied;
    }

    inline bool ApplyCachedThrowPoison(
        RE::Actor* target,
        RE::Actor* caster,
        RE::TESObjectWEAP* fallbackWeapon,
        const RE::NiPoint3& hitPos)
    {
        if (!target || !caster || !g_state.throwPoison.HasPoison()) {
            return false;
        }

        auto poisonState = g_state.throwPoison;

        g_state.throwPoison = ThrowPoisonState{};

        auto* poison = poisonState.poison;
        if (!poison || !poison->IsPoison()) {
            return false;
        }

        auto* magicTarget = target->AsMagicTarget();
        auto* sourceWeapon = poisonState.sourceWeapon ? poisonState.sourceWeapon : fallbackWeapon;

        bool anyApplied = false;
        std::uint32_t attempted = 0;

        for (auto* effect : poison->effects) {
            if (!effect || !effect->baseEffect) {
                continue;
            }

            ++attempted;

            RE::MagicTarget::AddTargetData data{};
            data.caster = caster;
            data.magicItem = poison;
            data.effect = effect;
            data.source = sourceWeapon;
            data.explosionPoint = hitPos;
            data.magnitude = effect->effectItem.magnitude;
            data.power = 1.0f;
            data.castingSource = poisonState.wasLeftHand ?
                RE::MagicSystem::CastingSource::kLeftHand :
                RE::MagicSystem::CastingSource::kRightHand;
            data.areaTarget = false;
            data.dualCasted = false;

            const bool applied = magicTarget->AddTarget(data);
            anyApplied = anyApplied || applied;
        }

        SKSE::log::debug(
            "[SaberThrow] Applied cached throw poison 0x{:08X}: attemptedEffects={}, anyApplied={}",
            poison->GetFormID(),
            attempted,
            anyApplied);

        return anyApplied;
    }

    inline bool RollStaggerChance(float chance)
    {
        if (chance > 1.0f) {
            chance *= 0.01f;
        }

        chance = std::clamp(chance, 0.0f, 1.0f);
        if (chance <= 0.0f) {
            return false;
        }
        if (chance >= 1.0f) {
            return true;
        }

        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        return distribution(rng) < chance;
    }

    inline RE::BGSPerk* GetPerk(std::uint32_t perkID, const std::string& pluginName)
    {
        if (perkID == 0 || pluginName.empty()) {
            return nullptr;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        return dataHandler->LookupForm<RE::BGSPerk>(perkID, pluginName);
    }

    inline bool PlayerHasAnyPerk(
        RE::PlayerCharacter* player,
        const std::vector<::SaberThrow::Settings::PerkFormSpec>& perkSpecs)
    {
        if (!player) {
            return false;
        }

        for (const auto& spec : perkSpecs) {
            if (auto* perk = GetPerk(spec.formID, spec.pluginName);
                perk && player->HasPerk(perk)) {
                return true;
            }
        }

        return false;
    }

    inline bool ShouldStaggerShieldBash()
    {
        const auto settings = ::SaberThrow::Settings::Get();

        return RollStaggerChance(settings.shieldStaggerChance);
    }

    inline bool ShouldStaggerWeaponHit(
        RE::PlayerCharacter* player,
        RE::TESObjectWEAP* weapon,
        bool useNoReturn)
    {
        const auto settings = ::SaberThrow::Settings::Get();

        if (useNoReturn) {
            if (settings.noReturnStaggerReq) {
                const ThrowWeaponHandedness handedness =
                    GetWeaponThrowHand(weapon, settings);
                const auto& staggerPerks = handedness == ThrowWeaponHandedness::TwoHanded ?
                    settings.noReturnStagger2H :
                    settings.noReturnStagger1H;

                if (!PlayerHasAnyPerk(player, staggerPerks)) {
                    return false;
                }
            }

            return RollStaggerChance(settings.noReturnStaggerChance);
        }

        if (settings.staggerNeedsPerk) {
            const ThrowWeaponHandedness handedness =
                GetWeaponThrowHand(weapon, settings);
            const auto& staggerPerks = handedness == ThrowWeaponHandedness::TwoHanded ?
                settings.staggerPerks2H :
                settings.staggerPerks1H;

            if (!PlayerHasAnyPerk(player, staggerPerks)) {
                return false;
            }
        }

        return RollStaggerChance(settings.staggerChance);
    }

    inline void TryWeaponHitStagger(RE::Actor* actor, RE::TESObjectWEAP* weapon, const RE::HitData* hitData = nullptr)
    {
        if (!actor) {
            return;
        }

        if constexpr (!kSendHitStagger) {
            return;
        }

        (void)weapon;
        (void)hitData;

        SendThrownHitStaggerAnim(actor);
    }

    inline void OnSaberHitActor(
        RE::Actor* actor,
        RE::TESObjectREFR* hitRef,
        RE::TESObjectREFR* thrownRef,
        const RE::NiPoint3& hitPos,
        const RE::NiPoint3& hitDir)
    {
        if (!actor) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* sourceActor = GetThrowSourceActor();
        if (!sourceActor || actor == sourceActor) {
            return;
        }

        if (IgnoreActorNPCThrow(actor)) {
            return;
        }

        const bool throwFromPlayer = player && sourceActor == player;

        if (ActorDead(actor)) {
            return;
        }

        if (g_state.noReturnDynamic) {
            StopLoopSound("actor impact");
        }

        auto* weapon = GetWeaponBase(thrownRef);
        auto* shield = weapon ? nullptr : GetShieldBase(thrownRef);
        auto* torch = (!weapon && !shield && IsThrownTorch(thrownRef)) ?
            thrownRef->GetObjectReference() :
            nullptr;
        const bool shieldBash = shield != nullptr;
        const bool torchBash = torch != nullptr;

        PlayThrownActorHitSound(thrownRef, actor, hitPos);

        RE::BGSAttackData* bashData = (shieldBash && player) ?
            FindShieldBashAttack(player) :
            nullptr;

        RE::HitData hitData{};
        const bool builtHitData = shieldBash ?
            MakeShieldBashHit(actor, thrownRef, hitPos, hitDir, hitData, bashData) :
            MakeThrownWeaponHit(sourceActor, actor, thrownRef, hitPos, hitDir, hitData);

        const float shieldPhysMult =
            (builtHitData && shieldBash) ?
            GetPhysDmgTakenMult(hitData) :
            1.0f;

        float baseDamage = shieldBash ?
            (player ? CalcShieldBashDmg(player, actor, shield, bashData) : GetThrowDmgFallback(thrownRef)) :
            (builtHitData ?
                GetDmgFromHitOrFallback(hitData, thrownRef) :
                GetThrowDmgFallback(thrownRef));

        if (throwFromPlayer && !torchBash) {
            baseDamage *= g_state.throwTemperMult;
        }

        if (throwFromPlayer && builtHitData && weapon && !shieldBash && !torchBash) {
            baseDamage = ApplySneakBonusMult(hitData, baseDamage);
        }

        float damage = shieldBash ?
            baseDamage :
            (throwFromPlayer ? ApplyThrowSkillDmg(player, baseDamage) : baseDamage);

        if (shieldBash) {
            damage = ApplyThrowShieldDmgMult(shield, damage);
        }
        else if (torchBash) {
            damage = ApplyThrowTorchDmgMult(torch, damage);
        }
        else {
            damage = ApplyWeaponDmgMult(weapon, damage, g_state.noReturnSpearMode);
            damage = ApplyHeadshotDmgMult(actor, hitPos, damage);
            if (throwFromPlayer) {
                damage = ApplyThrowPerkDmg(player, weapon, damage);
            }
        }

        damage = ApplyNPCThrowDmgMult(damage);

        const float rawShieldDamage = damage;
        if (shieldBash) {
            damage = ApplyPhysDmgTakenMult(
                damage,
                shieldPhysMult);
        }

        if (builtHitData) {
            if (shieldBash) {
                SetPhysDmgFields(
                    hitData,
                    rawShieldDamage,
                    damage);
                hitData.skill = RE::ActorValue::kBlock;
                hitData.flags.set(RE::HitData::Flag::kBash, RE::HitData::Flag::kMeleeAttack);
            }
            else {
                hitData.totalDamage = damage;
            }
        }

        const float healthDamage = ApplyCalcdHealthDmg(actor, sourceActor, damage);

        if (throwFromPlayer && builtHitData && !shieldBash) {
            ShowSneakNotify(hitData, healthDamage, hitPos);
        }
        auto* thrownSourceForm = shieldBash ?
            static_cast<RE::TESForm*>(shield) :
            (weapon ?
                static_cast<RE::TESForm*>(weapon) :
                static_cast<RE::TESForm*>(torch));
        auto* sourceObject = shieldBash ?
            static_cast<RE::TESBoundObject*>(shield) :
            (weapon ?
                static_cast<RE::TESBoundObject*>(weapon) :
                static_cast<RE::TESBoundObject*>(torch));

        if (throwFromPlayer) {
            GrantWeaponImpactXP(player, actor, thrownRef, healthDamage);
        }
        SendTESHitEvent(actor, sourceActor, thrownSourceForm, builtHitData ? &hitData : nullptr, healthDamage);
        if (throwFromPlayer) {
            SendAssaultAlarm(actor);
        }
        ApplyThrowDummyEnchant(
            actor,
            sourceActor,
            sourceObject,
            hitPos,
            (shieldBash || torchBash) ?
            RE::MagicSystem::CastingSource::kLeftHand :
            RE::MagicSystem::CastingSource::kRightHand);

        if (shieldBash) {
            if constexpr (kApplyShieldSpells) {
                if (throwFromPlayer) {
                    ApplyShieldAttackSpell(
                        actor,
                        player,
                        shield,
                        bashData,
                        hitPos);
                    ApplyShieldBashPerkSpell(
                        actor,
                        player,
                        shield,
                        hitPos);
                }
            }
            if (ShouldStaggerShieldBash()) {
                TryShieldBashStagger(actor);
            }
        }
        else {
            if (torchBash) {
                ApplyTorchBashFireSpell(actor, sourceActor, torch, hitPos);
            }

            ApplyWeaponBaseEnchant(
                actor,
                sourceActor,
                weapon,
                hitPos,
                throwFromPlayer ? g_state.throwInstanceEnchantment : nullptr);
            ApplyCachedThrowPoison(actor, sourceActor, weapon, hitPos);

            const bool noReturnStagger = g_state.noReturnDynamic;
            if (throwFromPlayer && ShouldStaggerWeaponHit(player, weapon, noReturnStagger)) {
                TryWeaponHitStagger(actor, weapon, builtHitData ? &hitData : nullptr);
            }
        }

        if (throwFromPlayer && !shieldBash) {
            const bool allowNeck = !g_state.noReturnDynamic;
            DismemberKilledActor(
                actor,
                player,
                weapon,
                hitPos,
                builtHitData ? &hitData : nullptr,
                allowNeck);
        }
    }

}

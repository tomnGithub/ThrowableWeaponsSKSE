#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SaberThrow::Settings
{
    enum class ThrowOrientation : std::uint32_t
    {
        Vertical = 0,
        Horizontal = 1,
        SpearLike = 2
    };

    struct PerkFormSpec
    {
        std::uint32_t formID{ 0 };
        std::string pluginName{};
    };

    struct Values
    {
        float zOffset{ 0.0f };

        float aimOffset{ 0.0f };
        float sneakAimOffset{ 0.0f };
        float thirdAimOffset{ 0.0f };
        float thirdSneakAimOffset{ 0.0f };
        float noReturnLockAim{ 0.0f };
        float telekLockAim{ 0.0f };

        float throwSpeed{ 2.8f };
        float throwDist{ 1500.0f };
        float noReturnSpeed{ 2.8f };
        float noReturnDist{ 1800.0f };

        float actorSweepRadiusMult{ 1.0f };
        float groundSweepRadiusMult{ 0.0f };

        float weaponSpinMult{ 1.0f };
        float telekSpinMult{ 1.0f };
        ThrowOrientation weaponOrient{ ThrowOrientation::Vertical };
        ThrowOrientation telekOrient{ ThrowOrientation::Horizontal };

        bool weaponHotkeyOn{ false };
        std::uint32_t weaponHotkey{ 0 };
        bool weaponHotkeyUseModifier{ false };
        std::uint32_t weaponHotkeyModifier{ 0 };
        bool returnOnHit{ false };
        bool shieldHotkeyOn{ false };
        std::uint32_t shieldHotkey{ 0 };
        bool shieldHotkeyUseModifier{ false };
        std::uint32_t shieldHotkeyModifier{ 0 };

        float npcDamageMult{ 1.0f };
        float npcSpeedMult{ 1.0f };
        float npcRangeMult{ 1.0f };
        float npcRotMult{ 1.0f };

        std::string npcNoReturnAnim{ "TWS_ThrowWeaponStandingRooted" };
        std::string npcReturnAnim{ "TWS_TelekineticThrowStandingRooted" };

        float staminaCost{ 60.0f };

        float altDamage0{ 1.0f };
        float altDamage100{ 1.0f };

        float archeryDamage0{ 1.0f };
        float archeryDamage100{ 1.0f };

        std::vector<PerkFormSpec> telekPerks{};
        float telekPerkPct{ 25.0f };
        std::vector<PerkFormSpec> noReturnPerks{};
        float noReturnPerkPct{ 25.0f };

        float headshotMult{ 1.0f };

        float sneakBonusMult{ 1.0f };

        float damageSword1H{ 1.0f };
        float speedSword1H{ 1.0f };
        float distanceSword1H{ 1.0f };
        float staminaSword1H{ 1.0f };
        float damageSword2H{ 1.0f };
        float speedSword2H{ 1.0f };
        float distanceSword2H{ 1.0f };
        float staminaSword2H{ 1.0f };

        float damageDagger1H{ 1.0f };
        float speedDagger1H{ 1.0f };
        float distanceDagger1H{ 1.0f };
        float staminaDagger1H{ 0.3f };

        float damageAxe1H{ 1.0f };
        float speedAxe1H{ 1.0f };
        float distanceAxe1H{ 1.0f };
        float staminaAxe1H{ 0.6f };
        float damageAxe2H{ 1.0f };
        float speedAxe2H{ 1.0f };
        float distanceAxe2H{ 1.0f };
        float staminaAxe2H{ 1.0f };

        float damageMace1H{ 1.0f };
        float speedMace1H{ 1.0f };
        float distanceMace1H{ 1.0f };
        float staminaMace1H{ 1.0f };
        float damageMace2H{ 1.0f };
        float speedMace2H{ 1.0f };
        float distanceMace2H{ 1.0f };
        float staminaMace2H{ 1.0f };

        float damageSpear1H{ 1.0f };
        float speedSpear1H{ 1.0f };
        float distanceSpear1H{ 1.0f };
        float staminaSpear1H{ 0.6f };
        float damageSpear2H{ 1.0f };
        float speedSpear2H{ 1.0f };
        float distanceSpear2H{ 1.0f };
        float staminaSpear2H{ 0.8f };

        float damageShield{ 1.0f };
        float speedShield{ 1.0f };
        float distanceShield{ 1.0f };
        float staminaShield{ 1.0f };

        float damageTorch{ 1.0f };
        float speedTorch{ 1.0f };
        float distanceTorch{ 1.0f };
        float staminaTorch{ 1.0f };

        bool weaponXPOn{ true };
        bool archeryXPOn{ true };
        float weaponXPMult{ 1.0f };
        float archeryXPMult{ 1.0f };

        bool equipNextStack{ true };

        bool preferLeftHand{ false };

        bool autoEquipPickup{ true };
        bool recastBoundOnImpact{ false };

        std::string weaponAnimRight{ "TWS_ThrowWeaponStanding" };
        std::string telekAnim{ "TWS_TelekineticThrowStanding" };
        std::string weaponAnimLeft{ "TWS_LeftHandThrowStanding" };
        std::string shieldAnim{ "TWS_LeftHandThrowStanding" };
        std::string shieldBashAnim{ "TWS_LeftHandThrowStanding" };
        std::string throwTriggerEvent{ "ThrowWeaponRelease" };

        bool weaponNeedsPerk{ false };

        std::vector<PerkFormSpec> weaponPerks1H{
            { 0x058F61, "Skyrim.esm" },
            { 0x03AF81, "Skyrim.esm" }
        };

        std::vector<PerkFormSpec> weaponPerks2H{
            { 0x058F61, "Skyrim.esm" },
            { 0x052D52, "Skyrim.esm" }
        };

        bool shieldNeedsPerk{ false };
        std::uint32_t shieldPerkID{ 0x058F66 };
        std::string shieldPerkPlugin{ "Skyrim.esm" };

        std::vector<std::string> spearKeywords{
            "OCF_WeapTypePole2H_Thrust",
            "OCF_WeapTypePole1H_Thrust",
            "WeapTypePike",
            "ShWeapTypeShortspear"
        };

        float staggerChance{ 0.0f };
        bool staggerNeedsPerk{ true };
        std::vector<PerkFormSpec> staggerPerks1H{
            { 0x058F62, "Skyrim.esm" }
        };
        std::vector<PerkFormSpec> staggerPerks2H{
            { 0x058F62, "Skyrim.esm" }
        };

        float shieldStaggerChance{ 0.33f };

        float noReturnStaggerChance{ 0.0f };
        bool noReturnStaggerReq{ true };
        std::vector<PerkFormSpec> noReturnStagger1H{
            { 0x058F62, "Skyrim.esm" }
        };
        std::vector<PerkFormSpec> noReturnStagger2H{
            { 0x058F62, "Skyrim.esm" }
        };

        bool teleportFX{ true };

        std::uint32_t teleportIMODID{ 0 };
        std::string teleportIMODPlugin{ "" };
        float teleportIMODStrength{ 1.0f };

        std::uint32_t teleportSoundID{ 0 };
        std::string teleportSoundPlugin{ "" };
        float teleportSoundVolume{ 1.0f };

        std::array<std::uint32_t, 12> impactSoundIDs{};
        std::array<std::string, 12> impactSoundPlugins{};

        float minBounceDist = 30.0f;

        bool continueThrow{ false };
        bool dismemberOn{ false };

        float dismemberMaxDist{ 384.0f };
    };

    struct GamepadHotkeys
    {
        bool weaponHotkeyOn{ false };
        std::uint32_t weaponHotkey{ 0 };
        bool weaponHotkeyUseModifier{ true };
        std::uint32_t weaponHotkeyModifier{ 0 };
        bool shieldHotkeyOn{ false };
        std::uint32_t shieldHotkey{ 0 };
        bool shieldHotkeyUseModifier{ true };
        std::uint32_t shieldHotkeyModifier{ 0 };
    };

    void LoadMCMSettings();

    Values Get();
    GamepadHotkeys GetGamepadHotkeys();
    float GetPickupRadiusMultiplier();
}

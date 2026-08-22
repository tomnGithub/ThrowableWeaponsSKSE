#include "Settings.h"

#include <SimpleIni.h>
#include "SKSE/SKSE.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace SaberThrow::Settings
{
    namespace
    {
        constexpr const char* kMainINI =
            "Data\\SKSE\\Plugins\\ThrowableWeaponsSKSE.ini";

        constexpr const char* kNPCINI =
            "Data\\SKSE\\Plugins\\ThrowableWeaponsSKSE_NPCs.ini";

        constexpr const char* kAnimINI =
            "Data\\SKSE\\Plugins\\ThrowableWeaponsSKSE_AnimationEvents.ini";

        constexpr const char* kMCMINI =
            "Data\\MCM\\Settings\\madSaberThrow.ini";

        constexpr const char* kMCMSection = "Main";
        constexpr const char* kNPCSection = "General";
        constexpr const char* kAnimSection = "AnimationEvents";

        constexpr const char* kNPCDamageKey = "fNPCThrowDamageMultiplier";
        constexpr const char* kNPCSpeedKey = "fNPCThrowSpeedMultiplier";
        constexpr const char* kNPCRangeKey = "fNPCThrowRangeMultiplier";
        constexpr const char* kNPCRotKey = "fNPCThrowRotationMultiplier";
        constexpr const char* kNPCNoReturnAnimKey =
            "sNPCNoReturnThrowAnimationEvent";
        constexpr const char* kNPCReturnAnimKey =
            "sNPCReturningThrowAnimationEvent";

        constexpr const char* kZOffsetKey = "fmadSaberThrowZOffset";
        constexpr const char* kAimOffsetKey = "fAimOffsetHeight";
        constexpr const char* kSneakAimOffsetKey = "fAimOffsetHeightSneak";
        constexpr const char* kThirdAimOffsetKey = "fAimOffsetHeight3rdPerson";
        constexpr const char* kThirdSneakAimOffsetKey = "fAimOffsetHeightSneak3rdPerson";
        constexpr const char* kNoReturnLockAimKey =
            "fAimOffsetHeightLockedOnNoReturn";
        constexpr const char* kTelekLockAimKey =
            "fAimOffsetHeightLockedOnTelekineticShield";
        constexpr const char* kThrowSpeedKey = "fmadSaberThrowSpeed";
        constexpr const char* kThrowDistKey = "fmadSaberThrowDistance";
        constexpr const char* kNoReturnSpeedKey = "fmadSaberThrowNoReturnSpeed";
        constexpr const char* kNoReturnDistKey = "fmadSaberThrowNoReturnDistance";
        constexpr const char* kWeaponSpinMultKey = "fThrowWeaponSpinMultiplier";
        constexpr const char* kTelekSpinMultKey = "fTelekineticThrowSpinMultiplier";
        constexpr const char* kWeaponOrientKey = "iThrowWeaponDefaultOrientation";
        constexpr const char* kTelekOrientKey = "iTelekineticThrowDefaultOrientation";
        constexpr const char* kWeaponHotkeyEnabledKey = "iThrowWeaponHotkeyEnabled";
        constexpr const char* kWeaponHotkeyKey = "iThrowWeaponHotkey";
        constexpr const char* kHotkeyReturnOnHitKey = "iThrowWeaponHotkeyReturnOnHit";
        constexpr const char* kShieldHotkeyEnabledKey = "iThrowShieldHotkeyEnabled";
        constexpr const char* kShieldHotkeyKey = "iThrowShieldHotkey";
        constexpr const char* kStaminaCostKey = "fmadSaberThrowStaminaCost";
        constexpr const char* kContinueThrowKey = "imadSaberThrowContiueThrow";
        constexpr const char* kDismemberEnabledKey = "imadSaberThrowDismember";
        constexpr const char* kDismemberDistKey = "fMaxDismemberDistance";

        constexpr const char* kAltDamage0Key = "fThrowDamageAt0Alteration";
        constexpr const char* kAltDamage100Key = "fThrowDamageAt100Alteration";

        constexpr const char* kArcheryDamage0Key = "fThrowDamageAt0Archery";
        constexpr const char* kArcheryDamage100Key = "fThrowDamageAt100Archery";
        constexpr const char* kTelekDamagePerkKey = "sTelekineticThrowDamagePerk";
        constexpr const char* kTelekPerkDamagePctKey =
            "fTelekineticThrowPerkDamageIncreasePercent";
        constexpr const char* kNoReturnDamagePerkKey = "sNoReturnThrowDamagePerk";
        constexpr const char* kNoReturnPerkPctKey =
            "fNoReturnThrowPerkDamageIncreasePercent";
        constexpr const char* kHeadshotMultKey = "fSaberThrowHeadshotMultiplier";
        constexpr const char* kSneakBonusMultKey = "fSneakAttackBonusMultiplier";
        constexpr const char* kDamageSword1HKey = "fDamageSword1H";
        constexpr const char* kSpeedSword1HKey = "fSpeedSword1H";
        constexpr const char* kDistanceSword1HKey = "fDistanceSword1H";
        constexpr const char* kStaminaSword1HKey = "fStaminaSword1H";
        constexpr const char* kDamageSword2HKey = "fDamageSword2H";
        constexpr const char* kSpeedSword2HKey = "fSpeedSword2H";
        constexpr const char* kDistanceSword2HKey = "fDistanceSword2H";
        constexpr const char* kStaminaSword2HKey = "fStaminaSword2H";
        constexpr const char* kDamageDagger1HKey = "fDamageDagger1H";
        constexpr const char* kSpeedDagger1HKey = "fSpeedDagger1H";
        constexpr const char* kDistanceDagger1HKey = "fDistanceDagger1H";
        constexpr const char* kStaminaDagger1HKey = "fStaminaDagger1H";
        constexpr const char* kDamageAxe1HKey = "fDamageAxe1H";
        constexpr const char* kSpeedAxe1HKey = "fSpeedAxe1H";
        constexpr const char* kDistanceAxe1HKey = "fDistanceAxe1H";
        constexpr const char* kStaminaAxe1HKey = "fStaminaAxe1H";
        constexpr const char* kDamageAxe2HKey = "fDamageAxe2H";
        constexpr const char* kSpeedAxe2HKey = "fSpeedAxe2H";
        constexpr const char* kDistanceAxe2HKey = "fDistanceAxe2H";
        constexpr const char* kStaminaAxe2HKey = "fStaminaAxe2H";
        constexpr const char* kDamageMace1HKey = "fDamageMace1H";
        constexpr const char* kSpeedMace1HKey = "fSpeedMace1H";
        constexpr const char* kDistanceMace1HKey = "fDistanceMace1H";
        constexpr const char* kStaminaMace1HKey = "fStaminaMace1H";
        constexpr const char* kDamageMace2HKey = "fDamageMace2H";
        constexpr const char* kSpeedMace2HKey = "fSpeedMace2H";
        constexpr const char* kDistanceMace2HKey = "fDistanceMace2H";
        constexpr const char* kStaminaMace2HKey = "fStaminaMace2H";
        constexpr const char* kDamageSpear1HKey = "fDamageSpear1H";
        constexpr const char* kSpeedSpear1HKey = "fSpeedSpear1H";
        constexpr const char* kDistanceSpear1HKey = "fDistanceSpear1H";
        constexpr const char* kStaminaSpear1HKey = "fStaminaSpear1H";
        constexpr const char* kDamageSpear2HKey = "fDamageSpear2H";
        constexpr const char* kSpeedSpear2HKey = "fSpeedSpear2H";
        constexpr const char* kDistanceSpear2HKey = "fDistanceSpear2H";
        constexpr const char* kStaminaSpear2HKey = "fStaminaSpear2H";
        constexpr const char* kDamageShieldKey = "fDamageShield";
        constexpr const char* kSpeedShieldKey = "fSpeedShield";
        constexpr const char* kDistanceShieldKey = "fDistanceShield";
        constexpr const char* kStaminaShieldKey = "fStaminaShield";
        constexpr const char* kDamageTorchKey = "fDamageTorch";
        constexpr const char* kSpeedTorchKey = "fSpeedTorch";
        constexpr const char* kDistanceTorchKey = "fDistanceTorch";
        constexpr const char* kStaminaTorchKey = "fStaminaTorch";

        constexpr const char* kWeaponXPKey = "iWeaponXPEnabled";
        constexpr const char* kArcheryXPKey = "iArcheryXPEnabled";
        constexpr const char* kWeaponXPMultKey = "fWeaponXPMult";
        constexpr const char* kArcheryXPMultKey = "fArcheryXPMult";
        constexpr const char* kEquipNextStackKey = "iEquipNextItemInStack";
        constexpr const char* kPreferLeftHandKey = "imadSaberThrowLeftHand";
        constexpr const char* kAutoEquipPickupKey = "imadSaberThrowAutoEquip";
        constexpr const char* kWeaponAnimRightKey = "smadSaberThrowAnimationEventRight";
        constexpr const char* kTelekAnimKey = "smadSaberThrowAnimationEventTelekinetic";
        constexpr const char* kWeaponAnimLeftKey = "smadSaberThrowAnimationEventLeft";
        constexpr const char* kShieldAnimKey = "smadSaberThrowShieldAnimationEvent";
        constexpr const char* kShieldBashAnimKey = "smadSaberThrowShieldBashAnimationEvent";
        constexpr const char* kThrowTriggerEventKey = "smadSaberThrowTriggerEvent";

        constexpr const char* kWeaponNeedsPerkKey = "iThrowWeaponRequiresPerk";

        constexpr const char* kWeaponPerk1HKey = "sThrowWeaponPerk1H";
        constexpr const char* kWeaponPerk2HKey = "sThrowWeaponPerk2H";
        constexpr const char* kShieldPerkIDKey = "sThrowShieldPerkID";
        constexpr const char* kShieldPerkPluginKey = "sThrowShieldPerkName";
        constexpr const char* kShieldPerkAliasKey = "sThrowShieldPPerkID";

        constexpr const char* kSpearKeywordsKey = "sSpearKeywords";

        constexpr const char* kStaggerChanceKey = "fStaggerChance";
        constexpr const char* kShieldStaggerChanceKey = "fStaggerChanceShield";
        constexpr const char* kStaggerNeedsPerkKey = "iStaggerRequiresPerk";
        constexpr const char* kStaggerPerk1HKey = "sStaggerPerk1H";
        constexpr const char* kStaggerPerk2HKey = "sStaggerPerk2H";
        constexpr const char* kLegacyStaggerPerkIDKey = "fStaggerPerkID";
        constexpr const char* kLegacyStaggerPluginKey = "fStaggerPluginName";

        constexpr const char* kNoReturnStaggerKey = "fStaggerChanceNoReturn";
        constexpr const char* kNoReturnStaggerReqKey = "iStaggerRequiresPerkNoReturn";
        constexpr const char* kNoReturnStagger1HKey = "sStaggerPerkNoReturn1H";
        constexpr const char* kNoReturnStagger2HKey = "sStaggerPerkNoReturn2H";

        constexpr const char* kTeleportFXKey = "iThrowTeleportFXEnabled";
        constexpr const char* kTeleportIMODIDKey = "sThrowTeleportIMODID";
        constexpr const char* kTeleportIMODPluginKey = "sThrowTeleportIMODPluginName";
        constexpr const char* kIMODStrengthKey = "fThrowTeleportIMODStrength";
        constexpr const char* kTeleportSoundIDKey = "sThrowTeleportSoundID";
        constexpr const char* kTeleportSoundPluginKey = "sThrowTeleportSoundPluginName";
        constexpr const char* kTeleportSoundVolumeKey = "fThrowTeleportSoundVolume";

        constexpr std::array<const char*, 12> kImpactIDKeys{
            "sThrowImpactSoundGeometryID",
            "sThrowImpactSoundSword1HID",
            "sThrowImpactSoundSword2HID",
            "sThrowImpactSoundDagger1HID",
            "sThrowImpactSoundAxe1HID",
            "sThrowImpactSoundAxe2HID",
            "sThrowImpactSoundMace1HID",
            "sThrowImpactSoundMace2HID",
            "sThrowImpactSoundSpear1HID",
            "sThrowImpactSoundSpear2HID",
            "sThrowImpactSoundShieldID",
            "sThrowImpactSoundTorchID"
        };

        constexpr std::array<const char*, 12> kImpactPluginKeys{
            "sThrowImpactSoundGeometryPluginName",
            "sThrowImpactSoundSword1HPluginName",
            "sThrowImpactSoundSword2HPluginName",
            "sThrowImpactSoundDagger1HPluginName",
            "sThrowImpactSoundAxe1HPluginName",
            "sThrowImpactSoundAxe2HPluginName",
            "sThrowImpactSoundMace1HPluginName",
            "sThrowImpactSoundMace2HPluginName",
            "sThrowImpactSoundSpear1HPluginName",
            "sThrowImpactSoundSpear2HPluginName",
            "sThrowImpactSoundShieldPluginName",
            "sThrowImpactSoundTorchPluginName"
        };

        constexpr const char* kMinBounceDistKey = "fMinimumDistanceBounce";

        constexpr float kDefZOffset = 0.0f;
        constexpr float kDefAimOffset = 0.0f;
        constexpr float kDefSneakAimOffset = 0.0f;
        constexpr float kDefThirdAimOffset = 0.0f;
        constexpr float kDefThirdSneakAimOffset = 0.0f;
        constexpr float kDefNoReturnLockAim = 0.0f;
        constexpr float kDefTelekLockAim = 0.0f;
        constexpr float kDefThrowSpeed = 2.3f;
        constexpr float kDefThrowDist = 1500.0f;
        constexpr float kDefNoReturnSpeed = 3.0f;
        constexpr float kDefNoReturnDist = 1800.0f;
        constexpr float kDefWeaponSpinMult = 1.0f;
        constexpr float kDefTelekSpinMult = 1.0f;
        constexpr std::uint32_t kDefWeaponOrient =
            static_cast<std::uint32_t>(ThrowOrientation::Vertical);
        constexpr std::uint32_t kDefTelekOrient =
            static_cast<std::uint32_t>(ThrowOrientation::Horizontal);
        constexpr bool kDefWeaponHotkeyOn = false;
        constexpr std::uint32_t kDefWeaponHotkey = 0;
        constexpr bool kDefReturnOnHit = false;
        constexpr bool kDefShieldHotkeyOn = false;
        constexpr std::uint32_t kDefShieldHotkey = 0;
        constexpr float kDefNPCDamage = 1.0f;
        constexpr float kDefNPCSpeed = 1.0f;
        constexpr float kDefNPCRange = 1.0f;
        constexpr float kDefNPCRot = 1.0f;
        constexpr const char* kDefNPCNoReturnAnim =
            "TWS_ThrowWeaponStandingRooted";
        constexpr const char* kDefNPCReturnAnim =
            "TWS_TelekineticThrowStandingRooted";

        constexpr float kDefStaminaCost = 60.0f;
        constexpr bool kDefContinueThrow = false;
        constexpr bool kDefDismemberOn = false;
        constexpr float kDefDismemberMaxDist = 384.0f;

        constexpr float kDefAltDamage0 = 1.0f;
        constexpr float kDefAltDamage100 = 1.0f;
        constexpr float kDefArcheryDamage0 = 1.0f;
        constexpr float kDefArcheryDamage100 = 1.0f;
        const std::vector<PerkFormSpec> kDefTelekPerks{};
        constexpr float kDefTelekPerkPct = 25.0f;
        const std::vector<PerkFormSpec> kDefNoReturnPerks{};
        constexpr float kDefNoReturnPerkPct = 25.0f;
        constexpr float kDefHeadshotMult = 1.0f;
        constexpr float kDefSneakBonusMult = 1.0f;
        constexpr float kDefDamageSword1H = 1.0f;
        constexpr float kDefSpeedSword1H = 1.0f;
        constexpr float kDefDistanceSword1H = 1.0f;
        constexpr float kDefStaminaSword1H = 1.0f;
        constexpr float kDefDamageSword2H = 1.0f;
        constexpr float kDefSpeedSword2H = 1.0f;
        constexpr float kDefDistanceSword2H = 1.0f;
        constexpr float kDefStaminaSword2H = 1.0f;
        constexpr float kDefDamageDagger1H = 1.0f;
        constexpr float kDefSpeedDagger1H = 1.0f;
        constexpr float kDefDistanceDagger1H = 1.0f;
        constexpr float kDefStaminaDagger1H = 1.0f;
        constexpr float kDefDamageAxe1H = 1.0f;
        constexpr float kDefSpeedAxe1H = 1.0f;
        constexpr float kDefDistanceAxe1H = 1.0f;
        constexpr float kDefStaminaAxe1H = 1.0f;
        constexpr float kDefDamageAxe2H = 1.0f;
        constexpr float kDefSpeedAxe2H = 1.0f;
        constexpr float kDefDistanceAxe2H = 1.0f;
        constexpr float kDefStaminaAxe2H = 1.0f;
        constexpr float kDefDamageMace1H = 1.0f;
        constexpr float kDefSpeedMace1H = 1.0f;
        constexpr float kDefDistanceMace1H = 1.0f;
        constexpr float kDefStaminaMace1H = 1.0f;
        constexpr float kDefDamageMace2H = 1.0f;
        constexpr float kDefSpeedMace2H = 1.0f;
        constexpr float kDefDistanceMace2H = 1.0f;
        constexpr float kDefStaminaMace2H = 1.0f;
        constexpr float kDefDamageSpear1H = 1.0f;
        constexpr float kDefSpeedSpear1H = 1.0f;
        constexpr float kDefDistanceSpear1H = 1.0f;
        constexpr float kDefStaminaSpear1H = 1.0f;
        constexpr float kDefDamageSpear2H = 1.0f;
        constexpr float kDefSpeedSpear2H = 1.0f;
        constexpr float kDefDistanceSpear2H = 1.0f;
        constexpr float kDefStaminaSpear2H = 1.0f;
        constexpr float kDefDamageShield = 1.0f;
        constexpr float kDefSpeedShield = 1.0f;
        constexpr float kDefDistanceShield = 1.0f;
        constexpr float kDefStaminaShield = 1.0f;
        constexpr float kDefDamageTorch = 1.0f;
        constexpr float kDefSpeedTorch = 1.0f;
        constexpr float kDefDistanceTorch = 1.0f;
        constexpr float kDefStaminaTorch = 1.0f;

        constexpr bool kDefWeaponXPOn = true;
        constexpr bool kDefArcheryXPOn = true;
        constexpr float kDefWeaponXPMult = 1.0f;
        constexpr float kDefArcheryXPMult = 1.0f;
        constexpr bool kDefEquipNextStack = true;
        constexpr bool kDefPreferLeftHand = false;
        constexpr bool kDefAutoEquipPickup = true;
        constexpr const char* kDefWeaponAnimRight = "TWS_ThrowWeaponStanding";
        constexpr const char* kDefTelekAnim = "TWS_TelekineticThrowStanding";
        constexpr const char* kDefWeaponAnimLeft = "TWS_LeftHandThrowStanding";
        constexpr const char* kDefShieldAnim = "TWS_LeftHandThrowStanding";
        constexpr const char* kDefShieldBashAnim = "TWS_LeftHandThrowStanding";
        constexpr const char* kDefTriggerEvent = "ThrowWeaponRelease";

        constexpr bool kDefWeaponNeedsPerk = false;

        const std::vector<PerkFormSpec> kDefWeaponPerks1H{
            { 0x058F61, "Skyrim.esm" },
            { 0x03AF81, "Skyrim.esm" }
        };

        const std::vector<PerkFormSpec> kDefWeaponPerks2H{
            { 0x058F61, "Skyrim.esm" },
            { 0x052D52, "Skyrim.esm" }
        };

        constexpr std::uint32_t kDefShieldPerkID = 0x058F66;
        constexpr const char* kDefShieldPerkPlugin = "Skyrim.esm";

        const std::vector<std::string> kDefSpearKeywords{
            "OCF_WeapTypePole2H_Thrust",
            "OCF_WeapTypePole1H_Thrust",
            "WeapTypePike",
            "ShWeapTypeShortspear"
        };

        constexpr float kDefStaggerChance = 0.5f;
        constexpr float kDefShieldStaggerChance = 0.33f;
        constexpr bool kDefStaggerNeedsPerk = true;
        const std::vector<PerkFormSpec> kDefStaggerPerks1H{
            { 0x058F62, "Skyrim.esm" }
        };
        const std::vector<PerkFormSpec> kDefStaggerPerks2H{
            { 0x058F62, "Skyrim.esm" }
        };

        constexpr float kDefNoReturnStagger = 0.5f;
        constexpr bool kDefNoReturnStaggerReq = true;
        const std::vector<PerkFormSpec> kDefNoReturnStagger1H{
            { 0x058F62, "Skyrim.esm" }
        };
        const std::vector<PerkFormSpec> kDefNoReturnStagger2H{
            { 0x058F62, "Skyrim.esm" }
        };

        constexpr bool kDefTeleportFX = true;
        constexpr std::uint32_t kDefTeleportIMODID = 0;
        constexpr const char* kDefTeleportIMODPlugin = "";
        constexpr float kDefIMODStrength = 1.0f;
        constexpr std::uint32_t kDefTeleportSoundID = 0;
        constexpr const char* kDefTeleportSoundPlugin = "";
        constexpr float kDefTeleportSoundVolume = 1.0f;

        constexpr float kDefMinBounceDist = 30.0f;

        std::atomic<float> g_zOffset{ kDefZOffset };
        std::atomic<float> g_aimOffset{ kDefAimOffset };
        std::atomic<float> g_sneakAimOffset{ kDefSneakAimOffset };
        std::atomic<float> g_thirdAimOffset{ kDefThirdAimOffset };
        std::atomic<float> g_thirdSneakAimOffset{ kDefThirdSneakAimOffset };
        std::atomic<float> g_noReturnLockAim{
            kDefNoReturnLockAim
        };
        std::atomic<float> g_telekLockAim{
            kDefTelekLockAim
        };
        std::atomic<float> g_throwSpeed{ kDefThrowSpeed };
        std::atomic<float> g_throwDist{ kDefThrowDist };
        std::atomic<float> g_noReturnSpeed{ kDefNoReturnSpeed };
        std::atomic<float> g_noReturnDist{ kDefNoReturnDist };
        std::atomic<float> g_weaponSpinMult{ kDefWeaponSpinMult };
        std::atomic<float> g_telekSpinMult{ kDefTelekSpinMult };
        std::atomic<std::uint32_t> g_weaponOrient{ kDefWeaponOrient };
        std::atomic<std::uint32_t> g_telekOrient{ kDefTelekOrient };
        std::atomic<bool> g_weaponHotkeyOn{ kDefWeaponHotkeyOn };
        std::atomic<std::uint32_t> g_weaponHotkey{ kDefWeaponHotkey };
        std::atomic<bool> g_returnOnHit{ kDefReturnOnHit };
        std::atomic<bool> g_shieldHotkeyOn{ kDefShieldHotkeyOn };
        std::atomic<std::uint32_t> g_shieldHotkey{ kDefShieldHotkey };
        std::atomic<float> g_npcDamageMult{ kDefNPCDamage };
        std::atomic<float> g_npcSpeedMult{ kDefNPCSpeed };
        std::atomic<float> g_npcRangeMult{ kDefNPCRange };
        std::atomic<float> g_npcRotMult{ kDefNPCRot };
        std::atomic<float> g_staminaCost{ kDefStaminaCost };
        std::atomic<bool> g_continueThrow{ kDefContinueThrow };
        std::atomic<bool> g_dismemberOn{ kDefDismemberOn };
        std::atomic<float> g_dismemberMaxDist{ kDefDismemberMaxDist };
        std::atomic<float> g_altDamage0{ kDefAltDamage0 };
        std::atomic<float> g_altDamage100{ kDefAltDamage100 };
        std::atomic<float> g_archeryDamage0{ kDefArcheryDamage0 };
        std::atomic<float> g_archeryDamage100{ kDefArcheryDamage100 };
        std::atomic<float> g_telekPerkPct{
            kDefTelekPerkPct
        };
        std::atomic<float> g_noReturnPerkPct{
            kDefNoReturnPerkPct
        };
        std::atomic<float> g_headshotMult{ kDefHeadshotMult };
        std::atomic<float> g_sneakBonusMult{ kDefSneakBonusMult };
        std::atomic<float> g_damageSword1H{ kDefDamageSword1H };
        std::atomic<float> g_speedSword1H{ kDefSpeedSword1H };
        std::atomic<float> g_distanceSword1H{ kDefDistanceSword1H };
        std::atomic<float> g_staminaSword1H{ kDefStaminaSword1H };
        std::atomic<float> g_damageSword2H{ kDefDamageSword2H };
        std::atomic<float> g_speedSword2H{ kDefSpeedSword2H };
        std::atomic<float> g_distanceSword2H{ kDefDistanceSword2H };
        std::atomic<float> g_staminaSword2H{ kDefStaminaSword2H };
        std::atomic<float> g_damageDagger1H{ kDefDamageDagger1H };
        std::atomic<float> g_speedDagger1H{ kDefSpeedDagger1H };
        std::atomic<float> g_distanceDagger1H{ kDefDistanceDagger1H };
        std::atomic<float> g_staminaDagger1H{ kDefStaminaDagger1H };
        std::atomic<float> g_damageAxe1H{ kDefDamageAxe1H };
        std::atomic<float> g_speedAxe1H{ kDefSpeedAxe1H };
        std::atomic<float> g_distanceAxe1H{ kDefDistanceAxe1H };
        std::atomic<float> g_staminaAxe1H{ kDefStaminaAxe1H };
        std::atomic<float> g_damageAxe2H{ kDefDamageAxe2H };
        std::atomic<float> g_speedAxe2H{ kDefSpeedAxe2H };
        std::atomic<float> g_distanceAxe2H{ kDefDistanceAxe2H };
        std::atomic<float> g_staminaAxe2H{ kDefStaminaAxe2H };
        std::atomic<float> g_damageMace1H{ kDefDamageMace1H };
        std::atomic<float> g_speedMace1H{ kDefSpeedMace1H };
        std::atomic<float> g_distanceMace1H{ kDefDistanceMace1H };
        std::atomic<float> g_staminaMace1H{ kDefStaminaMace1H };
        std::atomic<float> g_damageMace2H{ kDefDamageMace2H };
        std::atomic<float> g_speedMace2H{ kDefSpeedMace2H };
        std::atomic<float> g_distanceMace2H{ kDefDistanceMace2H };
        std::atomic<float> g_staminaMace2H{ kDefStaminaMace2H };
        std::atomic<float> g_damageSpear1H{ kDefDamageSpear1H };
        std::atomic<float> g_speedSpear1H{ kDefSpeedSpear1H };
        std::atomic<float> g_distanceSpear1H{ kDefDistanceSpear1H };
        std::atomic<float> g_staminaSpear1H{ kDefStaminaSpear1H };
        std::atomic<float> g_damageSpear2H{ kDefDamageSpear2H };
        std::atomic<float> g_speedSpear2H{ kDefSpeedSpear2H };
        std::atomic<float> g_distanceSpear2H{ kDefDistanceSpear2H };
        std::atomic<float> g_staminaSpear2H{ kDefStaminaSpear2H };
        std::atomic<float> g_damageShield{ kDefDamageShield };
        std::atomic<float> g_speedShield{ kDefSpeedShield };
        std::atomic<float> g_distanceShield{ kDefDistanceShield };
        std::atomic<float> g_staminaShield{ kDefStaminaShield };
        std::atomic<float> g_damageTorch{ kDefDamageTorch };
        std::atomic<float> g_speedTorch{ kDefSpeedTorch };
        std::atomic<float> g_distanceTorch{ kDefDistanceTorch };
        std::atomic<float> g_staminaTorch{ kDefStaminaTorch };

        std::atomic<bool> g_weaponXPOn{ kDefWeaponXPOn };
        std::atomic<bool> g_archeryXPOn{ kDefArcheryXPOn };
        std::atomic<float> g_weaponXPMult{ kDefWeaponXPMult };
        std::atomic<float> g_archeryXPMult{ kDefArcheryXPMult };
        std::atomic<bool> g_equipNextStack{ kDefEquipNextStack };
        std::atomic<bool> g_preferLeftHand{ kDefPreferLeftHand };
        std::atomic<bool> g_autoEquipPickup{ kDefAutoEquipPickup };
        std::atomic<bool> g_weaponNeedsPerk{ kDefWeaponNeedsPerk };
        std::atomic<std::uint32_t> g_shieldPerkID{ kDefShieldPerkID };

        std::atomic<float> g_staggerChance{ kDefStaggerChance };
        std::atomic<float> g_shieldStaggerChance{ kDefShieldStaggerChance };
        std::atomic<bool> g_staggerNeedsPerk{ kDefStaggerNeedsPerk };

        std::atomic<float> g_noReturnStaggerChance{ kDefNoReturnStagger };
        std::atomic<bool> g_noReturnStaggerReq{ kDefNoReturnStaggerReq };

        std::atomic<bool> g_teleportFX{ kDefTeleportFX };
        std::atomic<std::uint32_t> g_teleportIMODID{ kDefTeleportIMODID };
        std::atomic<float> g_teleportIMODStrength{ kDefIMODStrength };
        std::atomic<std::uint32_t> g_teleportSoundID{ kDefTeleportSoundID };
        std::atomic<float> g_teleportSoundVolume{ kDefTeleportSoundVolume };

        std::atomic<float> g_minBounceDist{ kDefMinBounceDist };

        std::mutex g_stringLock;

        std::string g_npcNoReturnAnim{ kDefNPCNoReturnAnim };
        std::string g_npcReturnAnim{ kDefNPCReturnAnim };

        std::string g_weaponAnimRight{ kDefWeaponAnimRight };
        std::string g_telekAnim{ kDefTelekAnim };
        std::string g_weaponAnimLeft{ kDefWeaponAnimLeft };
        std::string g_shieldAnim{ kDefShieldAnim };
        std::string g_shieldBashAnim{ kDefShieldBashAnim };
        std::string g_throwTriggerEvent{ kDefTriggerEvent };

        std::vector<PerkFormSpec> g_weaponPerks1H{ kDefWeaponPerks1H };
        std::vector<PerkFormSpec> g_weaponPerks2H{ kDefWeaponPerks2H };
        std::vector<PerkFormSpec> g_telekPerks{
            kDefTelekPerks
        };
        std::vector<PerkFormSpec> g_noReturnPerks{
            kDefNoReturnPerks
        };
        std::string g_shieldPerkPlugin{ kDefShieldPerkPlugin };

        std::vector<std::string> g_spearKeywords{ kDefSpearKeywords };

        std::vector<PerkFormSpec> g_staggerPerks1H{ kDefStaggerPerks1H };
        std::vector<PerkFormSpec> g_staggerPerks2H{ kDefStaggerPerks2H };
        std::vector<PerkFormSpec> g_noReturnStagger1H{ kDefNoReturnStagger1H };
        std::vector<PerkFormSpec> g_noReturnStagger2H{ kDefNoReturnStagger2H };

        std::string g_teleportIMODPlugin{ kDefTeleportIMODPlugin };
        std::string g_teleportSoundPlugin{ kDefTeleportSoundPlugin };


        std::array<std::uint32_t, 12> g_impactSoundIDs{};
        std::array<std::string, 12> g_impactPlugins{};

        constexpr bool kDebugSettings = true;

        float ReadFiniteFloat(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            float defaultValue,
            float minValue,
            float maxValue)
        {
            const double value = ini.GetDoubleValue(
                section,
                key,
                static_cast<double>(defaultValue));

            if (!std::isfinite(value)) {
                return defaultValue;
            }

            return std::clamp(static_cast<float>(value), minValue, maxValue);
        }

        float ReadChanceFloat(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            float defaultValue)
        {
            float value = ReadFiniteFloat(
                ini,
                section,
                key,
                defaultValue,
                0.0f,
                100.0f);

            if (value > 1.0f) {
                value *= 0.01f;
            }

            return std::clamp(value, 0.0f, 1.0f);
        }

        bool ReadBool01(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            bool defaultValue)
        {
            const long value = ini.GetLongValue(
                section,
                key,
                defaultValue ? 1L : 0L);

            return value != 0;
        }

        std::uint32_t ReadUInt32(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            std::uint32_t defaultValue)
        {
            const char* raw = ini.GetValue(section, key, nullptr);
            if (!raw) {
                return defaultValue;
            }

            while (std::isspace(static_cast<unsigned char>(*raw))) {
                ++raw;
            }

            if (*raw == '\0') {
                return defaultValue;
            }

            errno = 0;
            char* end = nullptr;
            const unsigned long value = std::strtoul(raw, &end, 0);

            if (raw == end ||
                errno == ERANGE ||
                value > (std::numeric_limits<std::uint32_t>::max)()) {
                return defaultValue;
            }

            while (std::isspace(static_cast<unsigned char>(*end))) {
                ++end;
            }

            if (*end != '\0') {
                return defaultValue;
            }

            return static_cast<std::uint32_t>(value);
        }

        std::uint32_t ReadUInt32AllowBlank(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            std::uint32_t defaultValue)
        {
            const char* raw = ini.GetValue(section, key, nullptr);
            if (!raw) {
                return defaultValue;
            }

            while (std::isspace(static_cast<unsigned char>(*raw))) {
                ++raw;
            }

            if (*raw == '\0') {
                return 0;
            }

            errno = 0;
            char* end = nullptr;
            const unsigned long value = std::strtoul(raw, &end, 0);

            if (raw == end ||
                errno == ERANGE ||
                value > (std::numeric_limits<std::uint32_t>::max)()) {
                return defaultValue;
            }

            while (std::isspace(static_cast<unsigned char>(*end))) {
                ++end;
            }

            if (*end != '\0') {
                return defaultValue;
            }

            return static_cast<std::uint32_t>(value);
        }

        std::string ReadString(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            const char* defaultValue)
        {
            const char* value = ini.GetValue(section, key, defaultValue);

            if (!value || value[0] == '\0') {
                return defaultValue;
            }

            return value;
        }

        std::string ReadStringAllowBlank(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            const char* defaultValue)
        {
            const char* value = ini.GetValue(section, key, nullptr);

            if (!value) {
                return defaultValue;
            }

            return value;
        }

        std::string TrimCopy(std::string value)
        {
            auto notSpace = [](unsigned char ch) {
                return !std::isspace(ch);
                };

            value.erase(
                value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));

            value.erase(
                std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());

            return value;
        }

        std::vector<std::string> ParseCommaSeparatedStrings(const std::string& raw)
        {
            std::vector<std::string> values;

            std::string current;
            for (const char ch : raw) {
                if (ch == ',') {
                    current = TrimCopy(current);
                    if (!current.empty()) {
                        values.push_back(current);
                    }
                    current.clear();
                    continue;
                }

                current.push_back(ch);
            }

            current = TrimCopy(current);
            if (!current.empty()) {
                values.push_back(current);
            }

            return values;
        }

        bool TryParsePerkFormSpec(const std::string& raw, PerkFormSpec& outSpec)
        {
            const std::string value = TrimCopy(raw);
            const std::size_t separator = value.find('~');
            if (separator == std::string::npos || value.find('~', separator + 1) != std::string::npos) {
                return false;
            }

            const std::string idText = TrimCopy(value.substr(0, separator));
            const std::string pluginName = TrimCopy(value.substr(separator + 1));
            if (idText.empty() || pluginName.empty()) {
                return false;
            }

            errno = 0;
            char* end = nullptr;
            const unsigned long parsedID = std::strtoul(idText.c_str(), &end, 0);
            if (idText.c_str() == end ||
                errno == ERANGE ||
                parsedID == 0 ||
                parsedID > (std::numeric_limits<std::uint32_t>::max)()) {
                return false;
            }

            while (std::isspace(static_cast<unsigned char>(*end))) {
                ++end;
            }
            if (*end != '\0') {
                return false;
            }

            outSpec.formID = static_cast<std::uint32_t>(parsedID);
            outSpec.pluginName = pluginName;
            return true;
        }

        std::vector<PerkFormSpec> ParsePerkFormSpecs(
            const std::string& raw,
            const char* key)
        {
            std::vector<PerkFormSpec> specs;
            for (const auto& entry : ParseCommaSeparatedStrings(raw)) {
                PerkFormSpec spec{};
                if (TryParsePerkFormSpec(entry, spec)) {
                    specs.push_back(spec);
                }
                else {
                    SKSE::log::warn(
                        "[SaberThrow/Settings] ignored invalid {} entry '{}'; expected 0xFormID~PluginName.",
                        key ? key : "perk list",
                        entry);
                }
            }

            return specs;
        }


        bool LoadIniFile(const char* path, CSimpleIniA& ini, const char* label)
        {
            const bool exists = std::filesystem::exists(path);

            if constexpr (kDebugSettings) {
                SKSE::log::info(
                    "[SaberThrow/Settings] loading {} settings: path='{}' exists={}",
                    label,
                    path,
                    exists);
            }

            if (!exists) {
                return false;
            }

            ini.SetUnicode();
            ini.SetMultiKey(false);

            const SI_Error rc = ini.LoadFile(path);

            if constexpr (kDebugSettings) {
                SKSE::log::info(
                    "[SaberThrow/Settings] {} LoadFile result: rc={}",
                    label,
                    rc);
            }

            return rc >= 0;
        }

        const CSimpleIniA* PickIniWithKey(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key)
        {
            if (primaryIni && primaryIni->GetValue(section, key, nullptr) != nullptr) {
                return primaryIni;
            }

            if (fallbackIni && fallbackIni->GetValue(section, key, nullptr) != nullptr) {
                return fallbackIni;
            }

            return nullptr;
        }

        std::vector<std::string> ReadCommaSeparatedStringsFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            const std::vector<std::string>& defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);
            if (!ini) {
                return defaultValue;
            }

            const char* raw = ini->GetValue(section, key, nullptr);
            if (!raw) {
                return defaultValue;
            }

            return ParseCommaSeparatedStrings(raw);
        }

        std::vector<PerkFormSpec> ReadPerkFormSpecsFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            const std::vector<PerkFormSpec>& defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);
            if (!ini) {
                return defaultValue;
            }

            const char* raw = ini->GetValue(section, key, nullptr);
            if (!raw) {
                return defaultValue;
            }

            return ParsePerkFormSpecs(raw, key);
        }


        float ReadFiniteFloatFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            float defaultValue,
            float minValue,
            float maxValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadFiniteFloat(*ini, section, key, defaultValue, minValue, maxValue) :
                defaultValue;
        }

        float ReadChanceFloatFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            float defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadChanceFloat(*ini, section, key, defaultValue) :
                defaultValue;
        }

        bool ReadBool01Fallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            bool defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadBool01(*ini, section, key, defaultValue) :
                defaultValue;
        }

        std::uint32_t ReadUInt32Fallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            std::uint32_t defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadUInt32(*ini, section, key, defaultValue) :
                defaultValue;
        }

        std::uint32_t ReadUInt32AllowBlankFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            std::uint32_t defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadUInt32AllowBlank(*ini, section, key, defaultValue) :
                defaultValue;
        }

        std::string ReadStringFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            const char* defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadString(*ini, section, key, defaultValue) :
                defaultValue;
        }

        std::string ReadAnimationEventString(
            const CSimpleIniA* animIni,
            const char* key,
            const char* defaultValue,
            const CSimpleIniA* legacyPrimaryIni,
            const CSimpleIniA* legacyFallbackIni,
            const char* legacySection)
        {
            if (animIni &&
                animIni->GetValue(kAnimSection, key, nullptr) != nullptr) {
                return ReadString(
                    *animIni,
                    kAnimSection,
                    key,
                    defaultValue);
            }

            return ReadStringFallback(
                legacyPrimaryIni,
                legacyFallbackIni,
                legacySection,
                key,
                defaultValue);
        }

        std::string ReadStringAllowBlankFallback(
            const CSimpleIniA* primaryIni,
            const CSimpleIniA* fallbackIni,
            const char* section,
            const char* key,
            const char* defaultValue)
        {
            const CSimpleIniA* ini = PickIniWithKey(primaryIni, fallbackIni, section, key);

            return ini ?
                ReadStringAllowBlank(*ini, section, key, defaultValue) :
                defaultValue;
        }
    }

    void LoadMCMSettings()
    {
        float zOffset = kDefZOffset;
        float aimOffset = kDefAimOffset;
        float sneakAimOffset = kDefSneakAimOffset;
        float thirdAimOffset = kDefThirdAimOffset;
        float thirdSneakAimOffset = kDefThirdSneakAimOffset;
        float noReturnLockAim = kDefNoReturnLockAim;
        float telekLockAim =
            kDefTelekLockAim;
        float throwSpeed = kDefThrowSpeed;
        float throwDist = kDefThrowDist;
        float noReturnSpeed = kDefNoReturnSpeed;
        float noReturnDist = kDefNoReturnDist;
        float weaponSpinMult = kDefWeaponSpinMult;
        float telekSpinMult = kDefTelekSpinMult;
        std::uint32_t weaponOrient = kDefWeaponOrient;
        std::uint32_t telekOrient = kDefTelekOrient;
        bool weaponHotkeyOn = kDefWeaponHotkeyOn;
        std::uint32_t weaponHotkey = kDefWeaponHotkey;
        bool returnOnHit = kDefReturnOnHit;
        bool shieldHotkeyOn = kDefShieldHotkeyOn;
        std::uint32_t shieldHotkey = kDefShieldHotkey;
        float npcDamageMult = kDefNPCDamage;
        float npcSpeedMult = kDefNPCSpeed;
        float npcRangeMult = kDefNPCRange;
        float npcRotMult = kDefNPCRot;
        std::string npcNoReturnAnim =
            kDefNPCNoReturnAnim;
        std::string npcReturnAnim =
            kDefNPCReturnAnim;
        float staminaCost = kDefStaminaCost;
        bool continueThrow = kDefContinueThrow;
        bool dismemberOn = kDefDismemberOn;
        float dismemberMaxDist = kDefDismemberMaxDist;
        float altDamage0 = kDefAltDamage0;
        float altDamage100 = kDefAltDamage100;
        float archeryDamage0 = kDefArcheryDamage0;
        float archeryDamage100 = kDefArcheryDamage100;
        std::vector<PerkFormSpec> telekPerks =
            kDefTelekPerks;
        float telekPerkPct =
            kDefTelekPerkPct;
        std::vector<PerkFormSpec> noReturnPerks =
            kDefNoReturnPerks;
        float noReturnPerkPct =
            kDefNoReturnPerkPct;
        float headshotMult = kDefHeadshotMult;
        float sneakBonusMult = kDefSneakBonusMult;
        float damageSword1H = kDefDamageSword1H;
        float speedSword1H = kDefSpeedSword1H;
        float distanceSword1H = kDefDistanceSword1H;
        float staminaSword1H = kDefStaminaSword1H;
        float damageSword2H = kDefDamageSword2H;
        float speedSword2H = kDefSpeedSword2H;
        float distanceSword2H = kDefDistanceSword2H;
        float staminaSword2H = kDefStaminaSword2H;
        float damageDagger1H = kDefDamageDagger1H;
        float speedDagger1H = kDefSpeedDagger1H;
        float distanceDagger1H = kDefDistanceDagger1H;
        float staminaDagger1H = kDefStaminaDagger1H;
        float damageAxe1H = kDefDamageAxe1H;
        float speedAxe1H = kDefSpeedAxe1H;
        float distanceAxe1H = kDefDistanceAxe1H;
        float staminaAxe1H = kDefStaminaAxe1H;
        float damageAxe2H = kDefDamageAxe2H;
        float speedAxe2H = kDefSpeedAxe2H;
        float distanceAxe2H = kDefDistanceAxe2H;
        float staminaAxe2H = kDefStaminaAxe2H;
        float damageMace1H = kDefDamageMace1H;
        float speedMace1H = kDefSpeedMace1H;
        float distanceMace1H = kDefDistanceMace1H;
        float staminaMace1H = kDefStaminaMace1H;
        float damageMace2H = kDefDamageMace2H;
        float speedMace2H = kDefSpeedMace2H;
        float distanceMace2H = kDefDistanceMace2H;
        float staminaMace2H = kDefStaminaMace2H;
        float damageSpear1H = kDefDamageSpear1H;
        float speedSpear1H = kDefSpeedSpear1H;
        float distanceSpear1H = kDefDistanceSpear1H;
        float staminaSpear1H = kDefStaminaSpear1H;
        float damageSpear2H = kDefDamageSpear2H;
        float speedSpear2H = kDefSpeedSpear2H;
        float distanceSpear2H = kDefDistanceSpear2H;
        float staminaSpear2H = kDefStaminaSpear2H;
        float damageShield = kDefDamageShield;
        float speedShield = kDefSpeedShield;
        float distanceShield = kDefDistanceShield;
        float staminaShield = kDefStaminaShield;
        float damageTorch = kDefDamageTorch;
        float speedTorch = kDefSpeedTorch;
        float distanceTorch = kDefDistanceTorch;
        float staminaTorch = kDefStaminaTorch;

        bool weaponXPOn = kDefWeaponXPOn;
        bool archeryXPOn = kDefArcheryXPOn;
        float weaponXPMult = kDefWeaponXPMult;
        float archeryXPMult = kDefArcheryXPMult;
        bool equipNextStack = kDefEquipNextStack;
        bool preferLeftHand = kDefPreferLeftHand;
        bool autoEquipPickup = kDefAutoEquipPickup;
        std::string weaponAnimRight = kDefWeaponAnimRight;
        std::string telekAnim = kDefTelekAnim;
        std::string weaponAnimLeft = kDefWeaponAnimLeft;
        std::string shieldAnim = kDefShieldAnim;
        std::string shieldBashAnim = kDefShieldBashAnim;
        std::string throwTriggerEvent = kDefTriggerEvent;

        bool weaponNeedsPerk = kDefWeaponNeedsPerk;

        std::vector<PerkFormSpec> weaponPerks1H = kDefWeaponPerks1H;
        std::vector<PerkFormSpec> weaponPerks2H = kDefWeaponPerks2H;

        std::uint32_t shieldPerkID = kDefShieldPerkID;
        std::string shieldPerkPlugin = kDefShieldPerkPlugin;

        std::vector<std::string> spearKeywords = kDefSpearKeywords;

        float staggerChance = kDefStaggerChance;
        float shieldStaggerChance = kDefShieldStaggerChance;
        bool staggerNeedsPerk = kDefStaggerNeedsPerk;
        std::vector<PerkFormSpec> staggerPerks1H = kDefStaggerPerks1H;
        std::vector<PerkFormSpec> staggerPerks2H = kDefStaggerPerks2H;

        float noReturnStaggerChance = kDefNoReturnStagger;
        bool noReturnStaggerReq = kDefNoReturnStaggerReq;
        std::vector<PerkFormSpec> noReturnStagger1H = kDefNoReturnStagger1H;
        std::vector<PerkFormSpec> noReturnStagger2H = kDefNoReturnStagger2H;

        bool teleportFX = kDefTeleportFX;
        std::uint32_t teleportIMODID = kDefTeleportIMODID;
        std::string teleportIMODPlugin = kDefTeleportIMODPlugin;
        float teleportIMODStrength = kDefIMODStrength;
        std::uint32_t teleportSoundID = kDefTeleportSoundID;
        std::string teleportSoundPlugin = kDefTeleportSoundPlugin;
        float teleportSoundVolume = kDefTeleportSoundVolume;

        std::array<std::uint32_t, 12> impactSoundIDs{};
        std::array<std::string, 12> impactSoundPlugins{};

        float minBounceDist = kDefMinBounceDist;

        CSimpleIniA mainIni;
        CSimpleIniA npcIniFile;
        CSimpleIniA animIniFile;
        CSimpleIniA mcmIni;

        const CSimpleIniA* primaryIni = nullptr;
        const CSimpleIniA* fallbackIni = nullptr;
        const CSimpleIniA* npcIni = nullptr;
        const CSimpleIniA* animIni = nullptr;

        if (LoadIniFile(kMainINI, mainIni, "ThrowableWeaponsSKSE")) {
            primaryIni = &mainIni;
        }

        if (LoadIniFile(kNPCINI, npcIniFile, "ThrowableWeaponsSKSE_NPCs")) {
            npcIni = &npcIniFile;
        }

        if (LoadIniFile(
                kAnimINI,
                animIniFile,
                "ThrowableWeaponsSKSE_AnimationEvents")) {
            animIni = &animIniFile;
        }

        if (LoadIniFile(kMCMINI, mcmIni, "MCM")) {
            fallbackIni = &mcmIni;
        }

        zOffset = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kZOffsetKey,
            kDefZOffset,
            -10000.0f,
            10000.0f);

        aimOffset = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kAimOffsetKey,
            kDefAimOffset,
            -10000.0f,
            10000.0f);

        sneakAimOffset = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSneakAimOffsetKey,
            kDefSneakAimOffset,
            -10000.0f,
            10000.0f);

        thirdAimOffset = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kThirdAimOffsetKey,
            kDefThirdAimOffset,
            -10000.0f,
            10000.0f);

        thirdSneakAimOffset = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kThirdSneakAimOffsetKey,
            kDefThirdSneakAimOffset,
            -10000.0f,
            10000.0f);

        noReturnLockAim = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnLockAimKey,
            kDefNoReturnLockAim,
            -10000.0f,
            10000.0f);

        telekLockAim = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTelekLockAimKey,
            kDefTelekLockAim,
            -10000.0f,
            10000.0f);


        throwSpeed = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kThrowSpeedKey,
            kDefThrowSpeed,
            0.0f,
            100000.0f);

        throwDist = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kThrowDistKey,
            kDefThrowDist,
            0.0f,
            100000.0f);

        noReturnSpeed = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnSpeedKey,
            kDefNoReturnSpeed,
            0.0f,
            100000.0f);

        noReturnDist = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnDistKey,
            kDefNoReturnDist,
            0.0f,
            100000.0f);

        weaponSpinMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponSpinMultKey,
            kDefWeaponSpinMult,
            0.0f,
            100.0f);

        telekSpinMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTelekSpinMultKey,
            kDefTelekSpinMult,
            0.0f,
            100.0f);

        weaponOrient = std::min<std::uint32_t>(
            ReadUInt32Fallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kWeaponOrientKey,
                kDefWeaponOrient),
            static_cast<std::uint32_t>(ThrowOrientation::SpearLike));

        telekOrient = std::min<std::uint32_t>(
            ReadUInt32Fallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kTelekOrientKey,
                kDefTelekOrient),
            static_cast<std::uint32_t>(ThrowOrientation::SpearLike));

        weaponHotkeyOn = ReadBool01Fallback(
            primaryIni, fallbackIni, kMCMSection,
            kWeaponHotkeyEnabledKey, kDefWeaponHotkeyOn);
        weaponHotkey = std::min<std::uint32_t>(
            ReadUInt32Fallback(
                primaryIni, fallbackIni, kMCMSection,
                kWeaponHotkeyKey, kDefWeaponHotkey),
            0xFFu);
        returnOnHit = ReadBool01Fallback(
            primaryIni, fallbackIni, kMCMSection,
            kHotkeyReturnOnHitKey, kDefReturnOnHit);
        shieldHotkeyOn = ReadBool01Fallback(
            primaryIni, fallbackIni, kMCMSection,
            kShieldHotkeyEnabledKey, kDefShieldHotkeyOn);
        shieldHotkey = std::min<std::uint32_t>(
            ReadUInt32Fallback(
                primaryIni, fallbackIni, kMCMSection,
                kShieldHotkeyKey, kDefShieldHotkey),
            0xFFu);

        if (npcIni) {
            npcDamageMult = ReadFiniteFloat(
                *npcIni,
                kNPCSection,
                kNPCDamageKey,
                kDefNPCDamage,
                0.0f,
                100.0f);

            npcSpeedMult = ReadFiniteFloat(
                *npcIni,
                kNPCSection,
                kNPCSpeedKey,
                kDefNPCSpeed,
                0.0f,
                100.0f);

            npcRangeMult = ReadFiniteFloat(
                *npcIni,
                kNPCSection,
                kNPCRangeKey,
                kDefNPCRange,
                0.0f,
                100.0f);

            npcRotMult = ReadFiniteFloat(
                *npcIni,
                kNPCSection,
                kNPCRotKey,
                kDefNPCRot,
                0.0f,
                100.0f);

        }

        staminaCost = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaCostKey,
            kDefStaminaCost,
            0.0f,
            10000.0f);

        continueThrow = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kContinueThrowKey,
            kDefContinueThrow);

        dismemberOn = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDismemberEnabledKey,
            kDefDismemberOn);

        dismemberMaxDist = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDismemberDistKey,
            kDefDismemberMaxDist,
            0.0f,
            100000.0f);

        altDamage0 = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kAltDamage0Key,
            kDefAltDamage0,
            0.0f,
            10.0f);

        altDamage100 = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kAltDamage100Key,
            kDefAltDamage100,
            0.0f,
            10.0f);

        archeryDamage0 = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kArcheryDamage0Key,
            kDefArcheryDamage0,
            0.0f,
            10.0f);

        archeryDamage100 = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kArcheryDamage100Key,
            kDefArcheryDamage100,
            0.0f,
            10.0f);

        telekPerks = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTelekDamagePerkKey,
            kDefTelekPerks);

        telekPerkPct = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTelekPerkDamagePctKey,
            kDefTelekPerkPct,
            0.0f,
            1000.0f);

        noReturnPerks = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnDamagePerkKey,
            kDefNoReturnPerks);

        noReturnPerkPct = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnPerkPctKey,
            kDefNoReturnPerkPct,
            0.0f,
            1000.0f);

        headshotMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kHeadshotMultKey,
            kDefHeadshotMult,
            0.0f,
            100.0f);

        sneakBonusMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSneakBonusMultKey,
            kDefSneakBonusMult,
            0.0f,
            100.0f);

        damageSword1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageSword1HKey,
            kDefDamageSword1H,
            0.0f,
            100.0f);

        speedSword1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedSword1HKey,
            kDefSpeedSword1H,
            0.0f,
            100.0f);

        distanceSword1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceSword1HKey,
            kDefDistanceSword1H,
            0.0f,
            100.0f);

        staminaSword1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaSword1HKey,
            kDefStaminaSword1H,
            0.0f,
            100.0f);

        damageSword2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageSword2HKey,
            kDefDamageSword2H,
            0.0f,
            100.0f);

        speedSword2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedSword2HKey,
            kDefSpeedSword2H,
            0.0f,
            100.0f);

        distanceSword2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceSword2HKey,
            kDefDistanceSword2H,
            0.0f,
            100.0f);

        staminaSword2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaSword2HKey,
            kDefStaminaSword2H,
            0.0f,
            100.0f);

        damageDagger1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageDagger1HKey,
            kDefDamageDagger1H,
            0.0f,
            100.0f);

        speedDagger1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedDagger1HKey,
            kDefSpeedDagger1H,
            0.0f,
            100.0f);

        distanceDagger1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceDagger1HKey,
            kDefDistanceDagger1H,
            0.0f,
            100.0f);

        staminaDagger1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaDagger1HKey,
            kDefStaminaDagger1H,
            0.0f,
            100.0f);

        damageAxe1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageAxe1HKey,
            kDefDamageAxe1H,
            0.0f,
            100.0f);

        speedAxe1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedAxe1HKey,
            kDefSpeedAxe1H,
            0.0f,
            100.0f);

        distanceAxe1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceAxe1HKey,
            kDefDistanceAxe1H,
            0.0f,
            100.0f);

        staminaAxe1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaAxe1HKey,
            kDefStaminaAxe1H,
            0.0f,
            100.0f);

        damageAxe2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageAxe2HKey,
            kDefDamageAxe2H,
            0.0f,
            100.0f);

        speedAxe2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedAxe2HKey,
            kDefSpeedAxe2H,
            0.0f,
            100.0f);

        distanceAxe2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceAxe2HKey,
            kDefDistanceAxe2H,
            0.0f,
            100.0f);

        staminaAxe2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaAxe2HKey,
            kDefStaminaAxe2H,
            0.0f,
            100.0f);

        damageMace1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageMace1HKey,
            kDefDamageMace1H,
            0.0f,
            100.0f);

        speedMace1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedMace1HKey,
            kDefSpeedMace1H,
            0.0f,
            100.0f);

        distanceMace1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceMace1HKey,
            kDefDistanceMace1H,
            0.0f,
            100.0f);

        staminaMace1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaMace1HKey,
            kDefStaminaMace1H,
            0.0f,
            100.0f);

        damageMace2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageMace2HKey,
            kDefDamageMace2H,
            0.0f,
            100.0f);

        speedMace2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedMace2HKey,
            kDefSpeedMace2H,
            0.0f,
            100.0f);

        distanceMace2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceMace2HKey,
            kDefDistanceMace2H,
            0.0f,
            100.0f);

        staminaMace2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaMace2HKey,
            kDefStaminaMace2H,
            0.0f,
            100.0f);

        damageSpear1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageSpear1HKey,
            kDefDamageSpear1H,
            0.0f,
            100.0f);

        speedSpear1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedSpear1HKey,
            kDefSpeedSpear1H,
            0.0f,
            100.0f);

        distanceSpear1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceSpear1HKey,
            kDefDistanceSpear1H,
            0.0f,
            100.0f);

        staminaSpear1H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaSpear1HKey,
            kDefStaminaSpear1H,
            0.0f,
            100.0f);

        damageSpear2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageSpear2HKey,
            kDefDamageSpear2H,
            0.0f,
            100.0f);

        speedSpear2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedSpear2HKey,
            kDefSpeedSpear2H,
            0.0f,
            100.0f);

        distanceSpear2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceSpear2HKey,
            kDefDistanceSpear2H,
            0.0f,
            100.0f);

        staminaSpear2H = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaSpear2HKey,
            kDefStaminaSpear2H,
            0.0f,
            100.0f);

        damageShield = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageShieldKey,
            kDefDamageShield,
            0.0f,
            100.0f);

        speedShield = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedShieldKey,
            kDefSpeedShield,
            0.0f,
            100.0f);

        distanceShield = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceShieldKey,
            kDefDistanceShield,
            0.0f,
            100.0f);

        staminaShield = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaShieldKey,
            kDefStaminaShield,
            0.0f,
            100.0f);

        damageTorch = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDamageTorchKey,
            kDefDamageTorch,
            0.0f,
            100.0f);

        speedTorch = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpeedTorchKey,
            kDefSpeedTorch,
            0.0f,
            100.0f);

        distanceTorch = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kDistanceTorchKey,
            kDefDistanceTorch,
            0.0f,
            100.0f);

        staminaTorch = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaminaTorchKey,
            kDefStaminaTorch,
            0.0f,
            100.0f);


        weaponXPOn = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponXPKey,
            kDefWeaponXPOn);

        archeryXPOn = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kArcheryXPKey,
            kDefArcheryXPOn);

        weaponXPMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponXPMultKey,
            kDefWeaponXPMult,
            0.0f,
            100.0f);

        archeryXPMult = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kArcheryXPMultKey,
            kDefArcheryXPMult,
            0.0f,
            100.0f);

        equipNextStack = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kEquipNextStackKey,
            kDefEquipNextStack);

        preferLeftHand = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kPreferLeftHandKey,
            kDefPreferLeftHand);

        autoEquipPickup = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kAutoEquipPickupKey,
            kDefAutoEquipPickup);

        weaponAnimRight = ReadAnimationEventString(
            animIni,
            kWeaponAnimRightKey,
            kDefWeaponAnimRight,
            primaryIni,
            fallbackIni,
            kMCMSection);

        telekAnim = ReadAnimationEventString(
            animIni,
            kTelekAnimKey,
            kDefTelekAnim,
            primaryIni,
            fallbackIni,
            kMCMSection);

        weaponAnimLeft = ReadAnimationEventString(
            animIni,
            kWeaponAnimLeftKey,
            kDefWeaponAnimLeft,
            primaryIni,
            fallbackIni,
            kMCMSection);

        shieldAnim = ReadAnimationEventString(
            animIni,
            kShieldAnimKey,
            kDefShieldAnim,
            primaryIni,
            fallbackIni,
            kMCMSection);

        shieldBashAnim = ReadAnimationEventString(
            animIni,
            kShieldBashAnimKey,
            kDefShieldBashAnim,
            primaryIni,
            fallbackIni,
            kMCMSection);

        throwTriggerEvent = ReadAnimationEventString(
            animIni,
            kThrowTriggerEventKey,
            kDefTriggerEvent,
            primaryIni,
            fallbackIni,
            kMCMSection);

        npcNoReturnAnim = ReadAnimationEventString(
            animIni,
            kNPCNoReturnAnimKey,
            kDefNPCNoReturnAnim,
            npcIni,
            nullptr,
            kNPCSection);

        npcReturnAnim = ReadAnimationEventString(
            animIni,
            kNPCReturnAnimKey,
            kDefNPCReturnAnim,
            npcIni,
            nullptr,
            kNPCSection);

        weaponNeedsPerk = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponNeedsPerkKey,
            kDefWeaponNeedsPerk);

        weaponPerks1H = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponPerk1HKey,
            kDefWeaponPerks1H);

        weaponPerks2H = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kWeaponPerk2HKey,
            kDefWeaponPerks2H);

        shieldPerkID = ReadUInt32AllowBlankFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kShieldPerkIDKey,
            kDefShieldPerkID);

        if (const CSimpleIniA* shieldPerkIni = PickIniWithKey(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kShieldPerkPluginKey)) {
            shieldPerkPlugin = ReadStringAllowBlank(
                *shieldPerkIni,
                kMCMSection,
                kShieldPerkPluginKey,
                kDefShieldPerkPlugin);
        }
        else {
            shieldPerkPlugin = ReadStringAllowBlankFallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kShieldPerkAliasKey,
                kDefShieldPerkPlugin);
        }

        spearKeywords = ReadCommaSeparatedStringsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kSpearKeywordsKey,
            kDefSpearKeywords);

        staggerChance = ReadChanceFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaggerChanceKey,
            kDefStaggerChance);

        shieldStaggerChance = ReadChanceFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kShieldStaggerChanceKey,
            kDefShieldStaggerChance);

        staggerNeedsPerk = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaggerNeedsPerkKey,
            kDefStaggerNeedsPerk);

        const bool hasStaggerPerk1HList = PickIniWithKey(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaggerPerk1HKey) != nullptr;

        const bool hasStaggerPerk2HList = PickIniWithKey(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kStaggerPerk2HKey) != nullptr;

        if (hasStaggerPerk1HList || hasStaggerPerk2HList) {
            staggerPerks1H = ReadPerkFormSpecsFallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kStaggerPerk1HKey,
                kDefStaggerPerks1H);

            staggerPerks2H = ReadPerkFormSpecsFallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kStaggerPerk2HKey,
                kDefStaggerPerks2H);
        }
        else {
            const bool hasLegacyPerk = PickIniWithKey(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kLegacyStaggerPerkIDKey) != nullptr;

            const bool hasLegacyPlugin = PickIniWithKey(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kLegacyStaggerPluginKey) != nullptr;

            if (hasLegacyPerk || hasLegacyPlugin) {
                const std::uint32_t legacyPerkID = ReadUInt32Fallback(
                    primaryIni,
                    fallbackIni,
                    kMCMSection,
                    kLegacyStaggerPerkIDKey,
                    0x058F62);

                const std::string legacyPluginName = ReadStringFallback(
                    primaryIni,
                    fallbackIni,
                    kMCMSection,
                    kLegacyStaggerPluginKey,
                    "Skyrim.esm");

                std::vector<PerkFormSpec> legacyPerks;
                if (legacyPerkID != 0 && !TrimCopy(legacyPluginName).empty()) {
                    legacyPerks.push_back({ legacyPerkID, TrimCopy(legacyPluginName) });
                }

                staggerPerks1H = legacyPerks;
                staggerPerks2H = legacyPerks;

                SKSE::log::info(
                    "[SaberThrow/Settings] using legacy returning-stagger perk keys for both 1H and 2H lists.");
            }
        }

        noReturnStaggerChance = ReadChanceFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnStaggerKey,
            kDefNoReturnStagger);

        noReturnStaggerReq = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnStaggerReqKey,
            kDefNoReturnStaggerReq);

        noReturnStagger1H = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnStagger1HKey,
            kDefNoReturnStagger1H);

        noReturnStagger2H = ReadPerkFormSpecsFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kNoReturnStagger2HKey,
            kDefNoReturnStagger2H);

        teleportFX = ReadBool01Fallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportFXKey,
            kDefTeleportFX);

        teleportIMODID = ReadUInt32AllowBlankFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportIMODIDKey,
            kDefTeleportIMODID);

        teleportIMODPlugin = ReadStringAllowBlankFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportIMODPluginKey,
            kDefTeleportIMODPlugin);

        teleportIMODStrength = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kIMODStrengthKey,
            kDefIMODStrength,
            0.0f,
            10.0f);

        teleportSoundID = ReadUInt32AllowBlankFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportSoundIDKey,
            kDefTeleportSoundID);

        teleportSoundPlugin = ReadStringAllowBlankFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportSoundPluginKey,
            kDefTeleportSoundPlugin);

        teleportSoundVolume = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kTeleportSoundVolumeKey,
            kDefTeleportSoundVolume,
            0.0f,
            10.0f);


        for (std::size_t i = 0; i < impactSoundIDs.size(); ++i) {
            impactSoundIDs[i] = ReadUInt32AllowBlankFallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kImpactIDKeys[i],
                0);

            impactSoundPlugins[i] = ReadStringAllowBlankFallback(
                primaryIni,
                fallbackIni,
                kMCMSection,
                kImpactPluginKeys[i],
                "");
        }

        minBounceDist = ReadFiniteFloatFallback(
            primaryIni,
            fallbackIni,
            kMCMSection,
            kMinBounceDistKey,
            kDefMinBounceDist,
            0.0f,
            100000.0f);

        g_zOffset.store(zOffset, std::memory_order_release);
        g_aimOffset.store(aimOffset, std::memory_order_release);
        g_sneakAimOffset.store(sneakAimOffset, std::memory_order_release);
        g_thirdAimOffset.store(thirdAimOffset, std::memory_order_release);
        g_thirdSneakAimOffset.store(thirdSneakAimOffset, std::memory_order_release);
        g_noReturnLockAim.store(
            noReturnLockAim,
            std::memory_order_release);
        g_telekLockAim.store(
            telekLockAim,
            std::memory_order_release);
        g_throwSpeed.store(throwSpeed, std::memory_order_release);
        g_throwDist.store(throwDist, std::memory_order_release);
        g_noReturnSpeed.store(noReturnSpeed, std::memory_order_release);
        g_noReturnDist.store(noReturnDist, std::memory_order_release);
        g_weaponSpinMult.store(weaponSpinMult, std::memory_order_release);
        g_telekSpinMult.store(telekSpinMult, std::memory_order_release);
        g_weaponOrient.store(weaponOrient, std::memory_order_release);
        g_telekOrient.store(telekOrient, std::memory_order_release);
        g_weaponHotkeyOn.store(weaponHotkeyOn, std::memory_order_release);
        g_weaponHotkey.store(weaponHotkey, std::memory_order_release);
        g_returnOnHit.store(returnOnHit, std::memory_order_release);
        g_shieldHotkeyOn.store(shieldHotkeyOn, std::memory_order_release);
        g_shieldHotkey.store(shieldHotkey, std::memory_order_release);
        g_npcDamageMult.store(npcDamageMult, std::memory_order_release);
        g_npcSpeedMult.store(npcSpeedMult, std::memory_order_release);
        g_npcRangeMult.store(npcRangeMult, std::memory_order_release);
        g_npcRotMult.store(npcRotMult, std::memory_order_release);
        g_staminaCost.store(staminaCost, std::memory_order_release);
        g_continueThrow.store(continueThrow, std::memory_order_release);
        g_dismemberOn.store(dismemberOn, std::memory_order_release);
        g_dismemberMaxDist.store(dismemberMaxDist, std::memory_order_release);
        g_altDamage0.store(altDamage0, std::memory_order_release);
        g_altDamage100.store(altDamage100, std::memory_order_release);
        g_archeryDamage0.store(archeryDamage0, std::memory_order_release);
        g_archeryDamage100.store(archeryDamage100, std::memory_order_release);
        g_telekPerkPct.store(
            telekPerkPct,
            std::memory_order_release);
        g_noReturnPerkPct.store(
            noReturnPerkPct,
            std::memory_order_release);
        g_headshotMult.store(headshotMult, std::memory_order_release);
        g_sneakBonusMult.store(sneakBonusMult, std::memory_order_release);
        g_damageSword1H.store(damageSword1H, std::memory_order_release);
        g_speedSword1H.store(speedSword1H, std::memory_order_release);
        g_distanceSword1H.store(distanceSword1H, std::memory_order_release);
        g_staminaSword1H.store(staminaSword1H, std::memory_order_release);
        g_damageSword2H.store(damageSword2H, std::memory_order_release);
        g_speedSword2H.store(speedSword2H, std::memory_order_release);
        g_distanceSword2H.store(distanceSword2H, std::memory_order_release);
        g_staminaSword2H.store(staminaSword2H, std::memory_order_release);
        g_damageDagger1H.store(damageDagger1H, std::memory_order_release);
        g_speedDagger1H.store(speedDagger1H, std::memory_order_release);
        g_distanceDagger1H.store(distanceDagger1H, std::memory_order_release);
        g_staminaDagger1H.store(staminaDagger1H, std::memory_order_release);
        g_damageAxe1H.store(damageAxe1H, std::memory_order_release);
        g_speedAxe1H.store(speedAxe1H, std::memory_order_release);
        g_distanceAxe1H.store(distanceAxe1H, std::memory_order_release);
        g_staminaAxe1H.store(staminaAxe1H, std::memory_order_release);
        g_damageAxe2H.store(damageAxe2H, std::memory_order_release);
        g_speedAxe2H.store(speedAxe2H, std::memory_order_release);
        g_distanceAxe2H.store(distanceAxe2H, std::memory_order_release);
        g_staminaAxe2H.store(staminaAxe2H, std::memory_order_release);
        g_damageMace1H.store(damageMace1H, std::memory_order_release);
        g_speedMace1H.store(speedMace1H, std::memory_order_release);
        g_distanceMace1H.store(distanceMace1H, std::memory_order_release);
        g_staminaMace1H.store(staminaMace1H, std::memory_order_release);
        g_damageMace2H.store(damageMace2H, std::memory_order_release);
        g_speedMace2H.store(speedMace2H, std::memory_order_release);
        g_distanceMace2H.store(distanceMace2H, std::memory_order_release);
        g_staminaMace2H.store(staminaMace2H, std::memory_order_release);
        g_damageSpear1H.store(damageSpear1H, std::memory_order_release);
        g_speedSpear1H.store(speedSpear1H, std::memory_order_release);
        g_distanceSpear1H.store(distanceSpear1H, std::memory_order_release);
        g_staminaSpear1H.store(staminaSpear1H, std::memory_order_release);
        g_damageSpear2H.store(damageSpear2H, std::memory_order_release);
        g_speedSpear2H.store(speedSpear2H, std::memory_order_release);
        g_distanceSpear2H.store(distanceSpear2H, std::memory_order_release);
        g_staminaSpear2H.store(staminaSpear2H, std::memory_order_release);
        g_damageShield.store(damageShield, std::memory_order_release);
        g_speedShield.store(speedShield, std::memory_order_release);
        g_distanceShield.store(distanceShield, std::memory_order_release);
        g_staminaShield.store(staminaShield, std::memory_order_release);
        g_damageTorch.store(damageTorch, std::memory_order_release);
        g_speedTorch.store(speedTorch, std::memory_order_release);
        g_distanceTorch.store(distanceTorch, std::memory_order_release);
        g_staminaTorch.store(staminaTorch, std::memory_order_release);

        g_weaponXPOn.store(weaponXPOn, std::memory_order_release);
        g_archeryXPOn.store(archeryXPOn, std::memory_order_release);
        g_weaponXPMult.store(weaponXPMult, std::memory_order_release);
        g_archeryXPMult.store(archeryXPMult, std::memory_order_release);
        g_equipNextStack.store(equipNextStack, std::memory_order_release);
        g_preferLeftHand.store(preferLeftHand, std::memory_order_release);
        g_autoEquipPickup.store(autoEquipPickup, std::memory_order_release);
        g_weaponNeedsPerk.store(weaponNeedsPerk, std::memory_order_release);
        g_shieldPerkID.store(shieldPerkID, std::memory_order_release);

        g_staggerChance.store(staggerChance, std::memory_order_release);
        g_shieldStaggerChance.store(shieldStaggerChance, std::memory_order_release);
        g_staggerNeedsPerk.store(staggerNeedsPerk, std::memory_order_release);

        g_noReturnStaggerChance.store(noReturnStaggerChance, std::memory_order_release);
        g_noReturnStaggerReq.store(noReturnStaggerReq, std::memory_order_release);

        g_teleportFX.store(teleportFX, std::memory_order_release);
        g_teleportIMODID.store(teleportIMODID, std::memory_order_release);
        g_teleportIMODStrength.store(teleportIMODStrength, std::memory_order_release);
        g_teleportSoundID.store(teleportSoundID, std::memory_order_release);
        g_teleportSoundVolume.store(teleportSoundVolume, std::memory_order_release);

        g_minBounceDist.store(minBounceDist, std::memory_order_release);

        {
            std::scoped_lock lock(g_stringLock);

            g_npcNoReturnAnim = npcNoReturnAnim;
            g_npcReturnAnim = npcReturnAnim;

            g_weaponAnimRight = weaponAnimRight;
            g_telekAnim = telekAnim;
            g_weaponAnimLeft = weaponAnimLeft;
            g_shieldAnim = shieldAnim;
            g_shieldBashAnim = shieldBashAnim;
            g_throwTriggerEvent = throwTriggerEvent;

            g_weaponPerks1H = weaponPerks1H;
            g_weaponPerks2H = weaponPerks2H;
            g_telekPerks = telekPerks;
            g_noReturnPerks = noReturnPerks;
            g_shieldPerkPlugin = shieldPerkPlugin;

            g_spearKeywords = spearKeywords;

            g_staggerPerks1H = staggerPerks1H;
            g_staggerPerks2H = staggerPerks2H;
            g_noReturnStagger1H = noReturnStagger1H;
            g_noReturnStagger2H = noReturnStagger2H;

            g_teleportIMODPlugin = teleportIMODPlugin;
            g_teleportSoundPlugin = teleportSoundPlugin;


            g_impactSoundIDs = impactSoundIDs;
            g_impactPlugins = impactSoundPlugins;
        }

        if constexpr (kDebugSettings) {
            SKSE::log::info(
                "[SaberThrow/Settings] loaded throw travel settings: fmadSaberThrowSpeed={} fmadSaberThrowDistance={} fmadSaberThrowNoReturnSpeed={} fmadSaberThrowNoReturnDistance={} fmadSaberThrowStaminaCost={}",
                throwSpeed,
                throwDist,
                noReturnSpeed,
                noReturnDist,
                staminaCost);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded headshot/sneak settings: fSaberThrowHeadshotMultiplier={} fSneakAttackBonusMultiplier={}",
                headshotMult,
                sneakBonusMult);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded aim offset settings: fAimOffsetHeight={} fAimOffsetHeightSneak={} fAimOffsetHeight3rdPerson={} fAimOffsetHeightSneak3rdPerson={} fAimOffsetHeightLockedOnNoReturn={} fAimOffsetHeightLockedOnTelekineticShield={}",
                aimOffset,
                sneakAimOffset,
                thirdAimOffset,
                thirdSneakAimOffset,
                noReturnLockAim,
                telekLockAim);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded sword multipliers: 1H dmg/speed/dist={}/{}/{} 2H dmg/speed/dist={}/{}/{}",
                damageSword1H,
                speedSword1H,
                distanceSword1H,
                damageSword2H,
                speedSword2H,
                distanceSword2H);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded dagger multipliers: 1H dmg/speed/dist={}/{}/{}",
                damageDagger1H,
                speedDagger1H,
                distanceDagger1H);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded axe/mace multipliers: axe1H={}/{}/{} axe2H={}/{}/{} mace1H={}/{}/{} mace2H={}/{}/{}",
                damageAxe1H,
                speedAxe1H,
                distanceAxe1H,
                damageAxe2H,
                speedAxe2H,
                distanceAxe2H,
                damageMace1H,
                speedMace1H,
                distanceMace1H,
                damageMace2H,
                speedMace2H,
                distanceMace2H);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded spear multipliers: 1H dmg/speed/dist={}/{}/{} 2H dmg/speed/dist={}/{}/{}",
                damageSpear1H,
                speedSpear1H,
                distanceSpear1H,
                damageSpear2H,
                speedSpear2H,
                distanceSpear2H);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded shield multipliers: dmg/speed/dist={}/{}/{}",
                damageShield,
                speedShield,
                distanceShield);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded torch multipliers: dmg/speed/dist={}/{}/{}",
                damageTorch,
                speedTorch,
                distanceTorch);


            SKSE::log::info(
                "[SaberThrow/Settings] loaded stamina multipliers: sword1H={} sword2H={} dagger1H={} axe1H={} axe2H={} mace1H={} mace2H={} spear1H={} spear2H={} shield={} torch={}",
                staminaSword1H,
                staminaSword2H,
                staminaDagger1H,
                staminaAxe1H,
                staminaAxe2H,
                staminaMace1H,
                staminaMace2H,
                staminaSpear1H,
                staminaSpear2H,
                staminaShield,
                staminaTorch);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded XP/settings: iWeaponXPEnabled={} iArcheryXPEnabled={} fWeaponXPMult={} fArcheryXPMult={} iEquipNextItemInStack={} imadSaberThrowLeftHand={} imadSaberThrowAutoEquip={}",
                weaponXPOn,
                archeryXPOn,
                weaponXPMult,
                archeryXPMult,
                equipNextStack,
                preferLeftHand,
                autoEquipPickup);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded animation events: right='{}' telekinetic='{}' left='{}' shield='{}' shieldBash='{}' trigger='{}'",
                weaponAnimRight,
                telekAnim,
                weaponAnimLeft,
                shieldAnim,
                shieldBashAnim,
                throwTriggerEvent);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded NPC animation events: noReturn='{}' returning='{}' trigger='ThrowWeaponRelease'",
                npcNoReturnAnim,
                npcReturnAnim);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded throw weapon settings: requiresPerk={} oneHandPerkCount={} twoHandPerkCount={}",
                weaponNeedsPerk,
                weaponPerks1H.size(),
                weaponPerks2H.size());

            SKSE::log::info(
                "[SaberThrow/Settings] loaded throw shield setting: perk=0x{:08X}/'{}'",
                shieldPerkID,
                shieldPerkPlugin);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded spear keyword settings: sSpearKeywords='{}'",
                [&spearKeywords]() {
                    std::string joined;
                    for (const auto& keyword : spearKeywords) {
                        if (!joined.empty()) {
                            joined += ", ";
                        }
                        joined += keyword;
                    }
                    return joined;
                }());

            SKSE::log::info(
                "[SaberThrow/Settings] loaded stagger settings: returnChance={} shieldChance={} returnRequiresPerk={} return1HPerkCount={} return2HPerkCount={} noReturnChance={} noReturnRequiresPerk={} noReturn1HPerkCount={} noReturn2HPerkCount={}",
                staggerChance,
                shieldStaggerChance,
                staggerNeedsPerk,
                staggerPerks1H.size(),
                staggerPerks2H.size(),
                noReturnStaggerChance,
                noReturnStaggerReq,
                noReturnStagger1H.size(),
                noReturnStagger2H.size());

            SKSE::log::info(
                "[SaberThrow/Settings] loaded teleport FX settings: enabled={} imod=0x{:08X}/'{}' imodStrength={} sound=0x{:08X}/'{}' soundVolume={}",
                teleportFX,
                teleportIMODID,
                teleportIMODPlugin,
                teleportIMODStrength,
                teleportSoundID,
                teleportSoundPlugin,
                teleportSoundVolume);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded bounce settings: minimumDistanceBounce={}",
                minBounceDist);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded return throw continuation setting: imadSaberThrowContiueThrow={}",
                continueThrow);

            SKSE::log::info(
                "[SaberThrow/Settings] loaded dismember settings: imadSaberThrowDismember={} fMaxDismemberDistance={}",
                dismemberOn,
                dismemberMaxDist);
        }
    }

    Values Get()
    {
        Values values{};

        values.zOffset =
            g_zOffset.load(std::memory_order_acquire);

        values.aimOffset =
            g_aimOffset.load(std::memory_order_acquire);

        values.sneakAimOffset =
            g_sneakAimOffset.load(std::memory_order_acquire);

        values.thirdAimOffset =
            g_thirdAimOffset.load(std::memory_order_acquire);

        values.thirdSneakAimOffset =
            g_thirdSneakAimOffset.load(std::memory_order_acquire);

        values.noReturnLockAim =
            g_noReturnLockAim.load(std::memory_order_acquire);

        values.telekLockAim =
            g_telekLockAim.load(std::memory_order_acquire);

        values.throwSpeed =
            g_throwSpeed.load(std::memory_order_acquire);

        values.throwDist =
            g_throwDist.load(std::memory_order_acquire);

        values.noReturnSpeed =
            g_noReturnSpeed.load(std::memory_order_acquire);

        values.noReturnDist =
            g_noReturnDist.load(std::memory_order_acquire);

        values.weaponSpinMult =
            g_weaponSpinMult.load(std::memory_order_acquire);
        values.telekSpinMult =
            g_telekSpinMult.load(std::memory_order_acquire);
        values.weaponOrient = static_cast<ThrowOrientation>(
            g_weaponOrient.load(std::memory_order_acquire));
        values.telekOrient = static_cast<ThrowOrientation>(
            g_telekOrient.load(std::memory_order_acquire));
        values.weaponHotkeyOn =
            g_weaponHotkeyOn.load(std::memory_order_acquire);
        values.weaponHotkey =
            g_weaponHotkey.load(std::memory_order_acquire);
        values.returnOnHit =
            g_returnOnHit.load(std::memory_order_acquire);
        values.shieldHotkeyOn =
            g_shieldHotkeyOn.load(std::memory_order_acquire);
        values.shieldHotkey =
            g_shieldHotkey.load(std::memory_order_acquire);

        values.npcDamageMult =
            g_npcDamageMult.load(std::memory_order_acquire);

        values.npcSpeedMult =
            g_npcSpeedMult.load(std::memory_order_acquire);

        values.npcRangeMult =
            g_npcRangeMult.load(std::memory_order_acquire);

        values.npcRotMult =
            g_npcRotMult.load(std::memory_order_acquire);


        values.staminaCost =
            g_staminaCost.load(std::memory_order_acquire);

        values.continueThrow =
            g_continueThrow.load(std::memory_order_acquire);

        values.dismemberOn =
            g_dismemberOn.load(std::memory_order_acquire);

        values.dismemberMaxDist =
            g_dismemberMaxDist.load(std::memory_order_acquire);

        values.altDamage0 =
            g_altDamage0.load(std::memory_order_acquire);

        values.altDamage100 =
            g_altDamage100.load(std::memory_order_acquire);

        values.archeryDamage0 =
            g_archeryDamage0.load(std::memory_order_acquire);

        values.archeryDamage100 =
            g_archeryDamage100.load(std::memory_order_acquire);

        values.telekPerkPct =
            g_telekPerkPct.load(std::memory_order_acquire);

        values.noReturnPerkPct =
            g_noReturnPerkPct.load(std::memory_order_acquire);

        values.headshotMult =
            g_headshotMult.load(std::memory_order_acquire);

        values.sneakBonusMult =
            g_sneakBonusMult.load(std::memory_order_acquire);

        values.damageSword1H =
            g_damageSword1H.load(std::memory_order_acquire);

        values.speedSword1H =
            g_speedSword1H.load(std::memory_order_acquire);

        values.distanceSword1H =
            g_distanceSword1H.load(std::memory_order_acquire);

        values.staminaSword1H =
            g_staminaSword1H.load(std::memory_order_acquire);

        values.damageSword2H =
            g_damageSword2H.load(std::memory_order_acquire);

        values.speedSword2H =
            g_speedSword2H.load(std::memory_order_acquire);

        values.distanceSword2H =
            g_distanceSword2H.load(std::memory_order_acquire);

        values.staminaSword2H =
            g_staminaSword2H.load(std::memory_order_acquire);

        values.damageDagger1H =
            g_damageDagger1H.load(std::memory_order_acquire);

        values.speedDagger1H =
            g_speedDagger1H.load(std::memory_order_acquire);

        values.distanceDagger1H =
            g_distanceDagger1H.load(std::memory_order_acquire);

        values.staminaDagger1H =
            g_staminaDagger1H.load(std::memory_order_acquire);

        values.damageAxe1H =
            g_damageAxe1H.load(std::memory_order_acquire);

        values.speedAxe1H =
            g_speedAxe1H.load(std::memory_order_acquire);

        values.distanceAxe1H =
            g_distanceAxe1H.load(std::memory_order_acquire);

        values.staminaAxe1H =
            g_staminaAxe1H.load(std::memory_order_acquire);

        values.damageAxe2H =
            g_damageAxe2H.load(std::memory_order_acquire);

        values.speedAxe2H =
            g_speedAxe2H.load(std::memory_order_acquire);

        values.distanceAxe2H =
            g_distanceAxe2H.load(std::memory_order_acquire);

        values.staminaAxe2H =
            g_staminaAxe2H.load(std::memory_order_acquire);

        values.damageMace1H =
            g_damageMace1H.load(std::memory_order_acquire);

        values.speedMace1H =
            g_speedMace1H.load(std::memory_order_acquire);

        values.distanceMace1H =
            g_distanceMace1H.load(std::memory_order_acquire);

        values.staminaMace1H =
            g_staminaMace1H.load(std::memory_order_acquire);

        values.damageMace2H =
            g_damageMace2H.load(std::memory_order_acquire);

        values.speedMace2H =
            g_speedMace2H.load(std::memory_order_acquire);

        values.distanceMace2H =
            g_distanceMace2H.load(std::memory_order_acquire);

        values.staminaMace2H =
            g_staminaMace2H.load(std::memory_order_acquire);

        values.damageSpear1H =
            g_damageSpear1H.load(std::memory_order_acquire);

        values.speedSpear1H =
            g_speedSpear1H.load(std::memory_order_acquire);

        values.distanceSpear1H =
            g_distanceSpear1H.load(std::memory_order_acquire);

        values.staminaSpear1H =
            g_staminaSpear1H.load(std::memory_order_acquire);

        values.damageSpear2H =
            g_damageSpear2H.load(std::memory_order_acquire);

        values.speedSpear2H =
            g_speedSpear2H.load(std::memory_order_acquire);

        values.distanceSpear2H =
            g_distanceSpear2H.load(std::memory_order_acquire);

        values.staminaSpear2H =
            g_staminaSpear2H.load(std::memory_order_acquire);

        values.damageShield =
            g_damageShield.load(std::memory_order_acquire);

        values.speedShield =
            g_speedShield.load(std::memory_order_acquire);

        values.distanceShield =
            g_distanceShield.load(std::memory_order_acquire);

        values.staminaShield =
            g_staminaShield.load(std::memory_order_acquire);

        values.damageTorch =
            g_damageTorch.load(std::memory_order_acquire);

        values.speedTorch =
            g_speedTorch.load(std::memory_order_acquire);

        values.distanceTorch =
            g_distanceTorch.load(std::memory_order_acquire);

        values.staminaTorch =
            g_staminaTorch.load(std::memory_order_acquire);


        values.weaponXPOn =
            g_weaponXPOn.load(std::memory_order_acquire);

        values.archeryXPOn =
            g_archeryXPOn.load(std::memory_order_acquire);

        values.weaponXPMult =
            g_weaponXPMult.load(std::memory_order_acquire);

        values.archeryXPMult =
            g_archeryXPMult.load(std::memory_order_acquire);

        values.equipNextStack =
            g_equipNextStack.load(std::memory_order_acquire);

        values.preferLeftHand =
            g_preferLeftHand.load(std::memory_order_acquire);

        values.autoEquipPickup =
            g_autoEquipPickup.load(std::memory_order_acquire);

        values.weaponNeedsPerk =
            g_weaponNeedsPerk.load(std::memory_order_acquire);

        values.shieldPerkID =
            g_shieldPerkID.load(std::memory_order_acquire);

        values.staggerChance =
            g_staggerChance.load(std::memory_order_acquire);

        values.shieldStaggerChance =
            g_shieldStaggerChance.load(std::memory_order_acquire);

        values.staggerNeedsPerk =
            g_staggerNeedsPerk.load(std::memory_order_acquire);

        values.noReturnStaggerChance =
            g_noReturnStaggerChance.load(std::memory_order_acquire);

        values.noReturnStaggerReq =
            g_noReturnStaggerReq.load(std::memory_order_acquire);

        values.teleportFX =
            g_teleportFX.load(std::memory_order_acquire);

        values.teleportIMODID =
            g_teleportIMODID.load(std::memory_order_acquire);

        values.teleportIMODStrength =
            g_teleportIMODStrength.load(std::memory_order_acquire);

        values.teleportSoundID =
            g_teleportSoundID.load(std::memory_order_acquire);

        values.teleportSoundVolume =
            g_teleportSoundVolume.load(std::memory_order_acquire);

        values.minBounceDist =
            g_minBounceDist.load(std::memory_order_acquire);

        {
            std::scoped_lock lock(g_stringLock);

            values.npcNoReturnAnim = g_npcNoReturnAnim;
            values.npcReturnAnim = g_npcReturnAnim;

            values.weaponAnimRight = g_weaponAnimRight;
            values.telekAnim = g_telekAnim;
            values.weaponAnimLeft = g_weaponAnimLeft;
            values.shieldAnim = g_shieldAnim;
            values.shieldBashAnim = g_shieldBashAnim;
            values.throwTriggerEvent = g_throwTriggerEvent;

            values.weaponPerks1H = g_weaponPerks1H;
            values.weaponPerks2H = g_weaponPerks2H;
            values.telekPerks = g_telekPerks;
            values.noReturnPerks = g_noReturnPerks;

            values.shieldPerkPlugin = g_shieldPerkPlugin;

            values.spearKeywords = g_spearKeywords;

            values.staggerPerks1H = g_staggerPerks1H;
            values.staggerPerks2H = g_staggerPerks2H;
            values.noReturnStagger1H = g_noReturnStagger1H;
            values.noReturnStagger2H = g_noReturnStagger2H;

            values.teleportIMODPlugin = g_teleportIMODPlugin;
            values.teleportSoundPlugin = g_teleportSoundPlugin;


            values.impactSoundIDs = g_impactSoundIDs;
            values.impactSoundPlugins = g_impactPlugins;
        }

        return values;
    }
}
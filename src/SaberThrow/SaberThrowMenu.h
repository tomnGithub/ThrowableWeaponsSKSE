#pragma once

#include "SaberThrow/SaberThrow_SKSE.hpp"
#include "SaberThrow/Settings.h"

#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/T/TESGlobal.h"

#include <SKSEMenuFramework.h>
#include <SimpleIni.h>

#include "SKSE/InputMap.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <Windows.h>

namespace SaberThrow::Menu
{
    namespace Detail
    {
        constexpr const char* kMenuSectionName = "Throwable Weapons SKSE";
        constexpr const char* kSection = "Main";
        constexpr const char* kPrimaryIniPath =
            "Data\\SKSE\\Plugins\\ThrowableWeaponsSKSE.ini";
        constexpr const char* kLegacyMCMIniPath =
            "Data\\MCM\\Settings\\madSaberThrow.ini";

        enum class SettingIniSource
        {
            kPrimary,
            kLegacy
        };

        enum class HotkeyBindingTarget : std::uint8_t
        {
            None = 0,
            ThrowWeapon,
            ThrowWeaponModifier,
            ThrowWeaponGamepad,
            ThrowWeaponGamepadModifier,
            ThrowShield,
            ThrowShieldModifier,
            ThrowShieldGamepad,
            ThrowShieldGamepadModifier
        };

        inline std::atomic<HotkeyBindingTarget> g_hotkeyTarget{
            HotkeyBindingTarget::None
        };
        inline std::atomic<HotkeyBindingTarget> g_pendingHotkey{
            HotkeyBindingTarget::None
        };
        inline std::atomic<std::uint32_t> g_pendingKeyCode{ 0 };

        inline std::atomic_bool g_registered{ false };
        inline std::atomic_bool g_spellRefreshPending{ false };
        inline std::int64_t g_menuListenerID{ -1 };
        inline SKSEMenuFramework::Model::InputEvent* g_hotkeyInput{ nullptr };
        inline std::mutex g_iniLock{};
        inline std::unordered_map<std::string, bool> g_iniBoolCache{};
        inline std::unordered_map<std::string, SettingIniSource> g_iniSources{};

        inline void DrawHelp(const char* help)
        {
            if (!help || help[0] == '\0') {
                return;
            }

            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("[?]");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s", help);
            }
        }

        inline void DrawHeader(const char* text)
        {
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Text("%s", text);
            ImGuiMCP::Separator();
        }

        inline bool SaveValueToIni(
            const char* path,
            const char* section,
            const char* key,
            const char* value)
        {
            if (!path || !section || !key || !value) {
                return false;
            }

            std::error_code ec;
            const std::filesystem::path filePath(path);
            if (const auto parent = filePath.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent, ec);
            }

            CSimpleIniA ini;
            ini.SetUnicode();
            ini.SetMultiKey(false);

            if (std::filesystem::exists(filePath, ec)) {
                const SI_Error loadResult = ini.LoadFile(path);
                if (loadResult < 0) {
                    SKSE::log::warn(
                        "[SaberThrow/Menu] Failed to load '{}' before saving key '{}': rc={}",
                        path,
                        key,
                        loadResult);
                }
            }

            ini.SetValue(section, key, value);
            const SI_Error saveResult = ini.SaveFile(path);
            if (saveResult < 0) {
                SKSE::log::error(
                    "[SaberThrow/Menu] Failed to save key '{}' to '{}': rc={}",
                    key,
                    path,
                    saveResult);
                return false;
            }

            return true;
        }

        inline bool IniContainsSetting(const char* path, const char* key)
        {
            if (!path || !key) {
                return false;
            }

            CSimpleIniA ini;
            ini.SetUnicode();
            ini.SetMultiKey(false);

            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || ini.LoadFile(path) < 0) {
                return false;
            }

            return ini.GetValue(kSection, key, nullptr) != nullptr;
        }

        inline SettingIniSource ResolveSettingIniSource(const char* key)
        {
            const std::string cacheKey = key ? key : "";
            if (const auto it = g_iniSources.find(cacheKey);
                it != g_iniSources.end()) {
                return it->second;
            }

            SettingIniSource source = SettingIniSource::kLegacy;
            if (IniContainsSetting(kPrimaryIniPath, key)) {
                source = SettingIniSource::kPrimary;
            }
            else if (IniContainsSetting(kLegacyMCMIniPath, key)) {
                source = SettingIniSource::kLegacy;
            }

            g_iniSources.emplace(cacheKey, source);
            return source;
        }

        inline const char* GetIniPath(SettingIniSource source)
        {
            return source == SettingIniSource::kPrimary ?
                kPrimaryIniPath :
                kLegacyMCMIniPath;
        }

        inline bool PersistRaw(const char* key, const char* value)
        {
            std::scoped_lock lock(g_iniLock);

            const bool saved = SaveValueToIni(
                GetIniPath(ResolveSettingIniSource(key)),
                kSection,
                key,
                value);

            if (!saved) {
                return false;
            }

            Settings::LoadMCMSettings();
            return true;
        }

        inline bool PersistBool(const char* key, bool value)
        {
            return PersistRaw(key, value ? "1" : "0");
        }

        inline bool PersistFloat(const char* key, float value)
        {
            char buffer[64]{};
            std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
            return PersistRaw(key, buffer);
        }

        inline bool PersistUInt32(const char* key, std::uint32_t value)
        {
            char buffer[32]{};
            std::snprintf(buffer, sizeof(buffer), "%u", value);
            return PersistRaw(key, buffer);
        }

        inline bool ReadBoolFromIniFile(
            const char* path,
            const char* key,
            bool defaultValue,
            bool& found)
        {
            found = false;

            CSimpleIniA ini;
            ini.SetUnicode();
            ini.SetMultiKey(false);

            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || ini.LoadFile(path) < 0) {
                return defaultValue;
            }

            if (ini.GetValue(kSection, key, nullptr) == nullptr) {
                return defaultValue;
            }

            found = true;
            return ini.GetLongValue(kSection, key, defaultValue ? 1L : 0L) != 0;
        }

        inline bool ReadIniOnlyBool(const char* key, bool defaultValue)
        {
            bool found = false;
            bool value = ReadBoolFromIniFile(kPrimaryIniPath, key, defaultValue, found);
            if (found) {
                return value;
            }

            value = ReadBoolFromIniFile(kLegacyMCMIniPath, key, defaultValue, found);
            return found ? value : defaultValue;
        }

        inline void SetConfiguredGlobalValueMainThread(
            RE::FormID localFormID,
            bool enabled,
            const char* label)
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            auto* global = dataHandler ?
                dataHandler->LookupForm<RE::TESGlobal>(
                    localFormID,
                    ::SaberThrow::kSpellCastPlugin) :
                nullptr;

            if (!global) {
                SKSE::log::warn(
                    "[SaberThrow/Menu] Could not update {} global: 0x{:06X}/'{}'.",
                    label ? label : "configured toggle",
                    localFormID,
                    ::SaberThrow::kSpellCastPlugin);
                return;
            }

            const float desiredValue = enabled ? 1.0f : 0.0f;
            if (global->value == desiredValue) {
                return;
            }

            global->value = desiredValue;
            SKSE::log::info(
                "[SaberThrow/Menu] Set {} global 0x{:08X} to {:.0f}.",
                label ? label : "configured toggle",
                global->GetFormID(),
                desiredValue);
        }

        inline void ApplyConfiguredToggleGlobalsMainThread()
        {
            constexpr RE::FormID kQuestMarkerID = 0x000813;
            constexpr RE::FormID kCombatPickupID = 0x000823;

            SetConfiguredGlobalValueMainThread(
                kQuestMarkerID,
                ReadIniOnlyBool("imadSaberShowDroppedQuestMarker", true),
                "Show Dropped Quest Marker");
            SetConfiguredGlobalValueMainThread(
                kCombatPickupID,
                ReadIniOnlyBool("imadSaberThrowAfterCombatPickup", false),
                "After Combat Pickup");
        }

        inline bool PlayerHasAnyThrowWeaponUnlockPerkMainThread(
            RE::PlayerCharacter* player,
            const Settings::Values& settings)
        {
            return ::SaberThrow::PlayerHasAnyPerk(
                player,
                settings.weaponPerks1H) ||
                ::SaberThrow::PlayerHasAnyPerk(
                    player,
                    settings.weaponPerks2H);
        }

        inline bool PlayerHasConfiguredThrowShieldPerkMainThread(
            RE::PlayerCharacter* player,
            const Settings::Values& settings)
        {
            if (!player) {
                return false;
            }

            auto* perk = ::SaberThrow::GetPerk(
                settings.shieldPerkID,
                settings.shieldPerkPlugin);
            return perk && player->HasPerk(perk);
        }

        inline void SetPlayerSpellPresenceMainThread(
            RE::PlayerCharacter* player,
            RE::SpellItem* spell,
            bool shouldHaveSpell,
            const char* label)
        {
            if (!player || !spell) {
                SKSE::log::warn(
                    "[SaberThrow/Menu] Could not update {} spell: player or spell form is unavailable.",
                    label ? label : "configured throw");
                return;
            }

            const bool currentlyHasSpell = player->HasSpell(spell);
            if (shouldHaveSpell == currentlyHasSpell) {
                return;
            }

            const bool changed = shouldHaveSpell ?
                player->AddSpell(spell) :
                player->RemoveSpell(spell);

            if (changed && shouldHaveSpell) {
                std::string notification = label && label[0] != '\0' ?
                    std::string(label) :
                    std::string("Throw power");
                notification += " added";
                ::SaberThrow::ShowDebugNotifyNoSound(
                    notification.c_str());
            }

            SKSE::log::info(
                "[SaberThrow/Menu] {} {} spell during player-state refresh: {}.",
                shouldHaveSpell ? "Added" : "Removed",
                label ? label : "configured throw",
                changed ? "success" : "game rejected the change");
        }

        inline void RefreshPlayerThrowPowerSpellsMainThread()
        {
            Settings::LoadMCMSettings();
            const auto settings = Settings::Get();

            ApplyConfiguredToggleGlobalsMainThread();

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                SKSE::log::warn(
                    "[SaberThrow/Menu] Could not refresh player menu state: no player.");
                return;
            }

            const bool needWeaponSpell =
                !settings.weaponNeedsPerk ||
                PlayerHasAnyThrowWeaponUnlockPerkMainThread(player, settings);

            const bool needShieldSpell =
                !settings.shieldNeedsPerk ||
                PlayerHasConfiguredThrowShieldPerkMainThread(player, settings);

            auto* noReturnSpell =
                ::SaberThrow::GetThrowTriggerSpell(
                    ::SaberThrow::kNoReturnSpellID);
            auto* shieldSpell =
                ::SaberThrow::GetThrowTriggerSpell(
                    ::SaberThrow::kShieldSpellID);

            SetPlayerSpellPresenceMainThread(
                player,
                noReturnSpell,
                needWeaponSpell,
                "Throw Weapon");
            SetPlayerSpellPresenceMainThread(
                player,
                shieldSpell,
                needShieldSpell,
                "Throw Shield");
        }

        inline void QueuePlayerThrowPowerSpellRefresh()
        {
            bool expected = false;
            if (!g_spellRefreshPending.compare_exchange_strong(expected, true)) {
                return;
            }

            auto refreshTask = []() {
                RefreshPlayerThrowPowerSpellsMainThread();
                g_spellRefreshPending.store(false, std::memory_order_release);
                };

            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask(std::move(refreshTask));
            }
            else {
                g_spellRefreshPending.store(false, std::memory_order_release);
                SKSE::log::warn(
                    "[SaberThrow/Menu] Could not queue throw-spell refresh: no SKSE task interface.");
            }
        }

        inline void __stdcall OnMenuFrameworkEvent(
            SKSEMenuFramework::Model::EventType eventType)
        {
            constexpr std::int32_t kCloseMenuEvent = 2;
            if (static_cast<std::int32_t>(eventType) == kCloseMenuEvent) {
                g_hotkeyTarget.store(
                    HotkeyBindingTarget::None,
                    std::memory_order_release);
                QueuePlayerThrowPowerSpellRefresh();
            }
        }

        inline bool& GetIniOnlyBool(const char* key, bool defaultValue)
        {
            const auto [it, inserted] = g_iniBoolCache.try_emplace(key, defaultValue);
            if (inserted) {
                it->second = ReadIniOnlyBool(key, defaultValue);
            }
            return it->second;
        }

        inline void DrawToggle(
            const char* key,
            const char* label,
            bool currentValue,
            const char* help)
        {
            bool value = currentValue;
            const std::string controlLabel = std::string(label) + "##" + key;
            if (ImGuiMCP::Checkbox(controlLabel.c_str(), &value)) {
                PersistBool(key, value);
            }
            DrawHelp(help);
        }

        inline void DrawIniOnlyToggle(
            const char* key,
            const char* label,
            bool defaultValue,
            const char* help)
        {
            bool& cachedValue = GetIniOnlyBool(key, defaultValue);
            bool value = cachedValue;
            const std::string controlLabel = std::string(label) + "##" + key;
            if (ImGuiMCP::Checkbox(controlLabel.c_str(), &value)) {
                if (PersistBool(key, value)) {
                    cachedValue = value;
                }
            }
            DrawHelp(help);
        }

        inline void DrawSlider(
            const char* key,
            const char* label,
            float currentValue,
            float minimum,
            float maximum,
            float step,
            const char* format,
            const char* help)
        {
            float value = currentValue;
            const std::string controlLabel = std::string(label) + "##" + key;
            if (ImGuiMCP::SliderFloat(
                controlLabel.c_str(),
                &value,
                minimum,
                maximum,
                format)) {
                value = std::clamp(value, minimum, maximum);
                if (step > 0.0f) {
                    value = std::round(value / step) * step;
                    value = std::clamp(value, minimum, maximum);
                }
                PersistFloat(key, value);
            }
            DrawHelp(help);
        }

        inline const char* GetOrientationLabel(Settings::ThrowOrientation orientation)
        {
            switch (orientation) {
            case Settings::ThrowOrientation::Horizontal:
                return "Horizontal";
            case Settings::ThrowOrientation::SpearLike:
                return "Spear-Like";
            case Settings::ThrowOrientation::Vertical:
            default:
                return "Vertical";
            }
        }

        inline void DrawOrientationDropdown(
            const char* key,
            const char* label,
            Settings::ThrowOrientation currentValue,
            const char* help)
        {
            const std::string controlLabel = std::string(label) + "##" + key;
            if (ImGuiMCP::BeginCombo(controlLabel.c_str(), GetOrientationLabel(currentValue))) {
                constexpr std::array<Settings::ThrowOrientation, 3> options{
                    Settings::ThrowOrientation::Vertical,
                    Settings::ThrowOrientation::Horizontal,
                    Settings::ThrowOrientation::SpearLike
                };

                for (const auto option : options) {
                    const bool selected = option == currentValue;
                    if (ImGuiMCP::Selectable(GetOrientationLabel(option), selected)) {
                        PersistUInt32(key, static_cast<std::uint32_t>(option));
                    }
                    if (selected) {
                        ImGuiMCP::SetItemDefaultFocus();
                    }
                }
                ImGuiMCP::EndCombo();
            }
            DrawHelp(help);
        }

        inline std::string GetHotkeyLabel(std::uint32_t code)
        {
            if (code == 0) {
                return "Unbound";
            }

            const auto label = SKSE::InputMap::GetKeyName(code);
            if (!label.empty()) {
                return label;
            }

            char buffer[32]{};
            std::snprintf(buffer, sizeof(buffer), "Key 0x%X", code);
            return buffer;
        }

        inline bool __stdcall OnHotkeyBindingInput(RE::InputEvent* event)
        {
            const auto target =
                g_hotkeyTarget.load(std::memory_order_acquire);
            if (target == HotkeyBindingTarget::None || !event) {
                return false;
            }

            const auto* button = event->AsButtonEvent();
            if (!button || !button->IsDown()) {
                return false;
            }

            const bool gamepadTarget =
                target == HotkeyBindingTarget::ThrowWeaponGamepad ||
                target == HotkeyBindingTarget::ThrowWeaponGamepadModifier ||
                target == HotkeyBindingTarget::ThrowShieldGamepad ||
                target == HotkeyBindingTarget::ThrowShieldGamepadModifier;

            std::uint32_t code = button->GetIDCode();
            switch (event->GetDevice()) {
            case RE::INPUT_DEVICE::kKeyboard:
                if (gamepadTarget) {
                    return false;
                }
                break;
            case RE::INPUT_DEVICE::kMouse:
                if (gamepadTarget) {
                    return false;
                }
                code += SKSE::InputMap::kMacro_MouseButtonOffset;
                break;
            case RE::INPUT_DEVICE::kGamepad:
                if (!gamepadTarget) {
                    return false;
                }
                code = SKSE::InputMap::GamepadMaskToKeycode(code);
                break;
            default:
                return false;
            }

            if (code >= SKSE::InputMap::kMaxMacros) {
                return false;
            }

            g_pendingKeyCode.store(code, std::memory_order_relaxed);
            g_pendingHotkey.store(
                target,
                std::memory_order_release);
            g_hotkeyTarget.store(
                HotkeyBindingTarget::None,
                std::memory_order_release);
            return true;
        }

        inline void ApplyPendingHotkeyBinding()
        {
            const auto target = g_pendingHotkey.exchange(
                HotkeyBindingTarget::None,
                std::memory_order_acq_rel);
            if (target == HotkeyBindingTarget::None) {
                return;
            }

            const std::uint32_t code =
                g_pendingKeyCode.load(std::memory_order_acquire);

            switch (target) {
            case HotkeyBindingTarget::ThrowWeapon:
                PersistUInt32("iThrowWeaponHotkey", code);
                break;
            case HotkeyBindingTarget::ThrowWeaponModifier:
                PersistUInt32("iThrowWeaponHotkeyModifier", code);
                break;
            case HotkeyBindingTarget::ThrowWeaponGamepad:
                PersistUInt32("iThrowWeaponGamepadHotkey", code);
                break;
            case HotkeyBindingTarget::ThrowWeaponGamepadModifier:
                PersistUInt32("iThrowWeaponGamepadHotkeyModifier", code);
                break;
            case HotkeyBindingTarget::ThrowShield:
                PersistUInt32("iThrowShieldHotkey", code);
                break;
            case HotkeyBindingTarget::ThrowShieldModifier:
                PersistUInt32("iThrowShieldHotkeyModifier", code);
                break;
            case HotkeyBindingTarget::ThrowShieldGamepad:
                PersistUInt32("iThrowShieldGamepadHotkey", code);
                break;
            case HotkeyBindingTarget::ThrowShieldGamepadModifier:
                PersistUInt32("iThrowShieldGamepadHotkeyModifier", code);
                break;
            case HotkeyBindingTarget::None:
            default:
                break;
            }
        }

        inline void DrawHotkeyBinding(
            const char* key,
            const char* label,
            std::uint32_t currentValue,
            HotkeyBindingTarget target,
            const char* help)
        {
            const bool capturing =
                g_hotkeyTarget.load(std::memory_order_acquire) == target;

            std::string buttonLabel = label;
            buttonLabel += ": ";
            buttonLabel += capturing ? "Press a key or button..." : GetHotkeyLabel(currentValue);
            buttonLabel += "##";
            buttonLabel += key;

            if (ImGuiMCP::Button(buttonLabel.c_str())) {
                g_hotkeyTarget.store(target, std::memory_order_release);
            }

            ImGuiMCP::SameLine();
            const std::string clearLabel = std::string("Clear##") + key;
            if (ImGuiMCP::SmallButton(clearLabel.c_str())) {
                if (g_hotkeyTarget.load(std::memory_order_acquire) == target) {
                    g_hotkeyTarget.store(
                        HotkeyBindingTarget::None,
                        std::memory_order_release);
                }
                PersistUInt32(key, 0);
            }

            DrawHelp(help);
        }

        inline void DrawWeaponType(
            const char* title,
            const char* helpWeaponName,
            const char* damageKey,
            float damage,
            const char* speedKey,
            float speed,
            const char* distanceKey,
            float distance,
            const char* staminaKey,
            float stamina)
        {
            const std::string damageHelp =
                std::string("Damage multiplier for thrown ") + helpWeaponName + ".";
            const std::string speedHelp =
                std::string("Speed multiplier for thrown ") + helpWeaponName + ".";
            const std::string distanceHelp =
                std::string("Distance multiplier for thrown ") + helpWeaponName + ".";
            const std::string staminaHelp =
                std::string("Multiplier applied to the stamina cost of throwing ") +
                helpWeaponName + ".";

            DrawHeader(title);
            DrawSlider(
                damageKey,
                "Damage Multiplier",
                damage,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                damageHelp.c_str());
            DrawSlider(
                speedKey,
                "Speed Multiplier",
                speed,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                speedHelp.c_str());
            DrawSlider(
                distanceKey,
                "Distance Multiplier",
                distance,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                distanceHelp.c_str());
            DrawSlider(
                staminaKey,
                "Stamina Cost Multiplier",
                stamina,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                staminaHelp.c_str());
        }

        inline void __stdcall RenderGeneral()
        {
            const auto settings = Settings::Get();
            const float pickupRadiusMultiplier = Settings::GetPickupRadiusMultiplier();

            DrawHeader("Tracking & Recovery");
            DrawIniOnlyToggle(
                "imadSaberShowDroppedQuestMarker",
                "Show Dropped Weapon Quest Marker",
                true,
                "When enabled, shows a quest marker on the dropped thrown weapon so it can be "
                "found more easily.");
            DrawIniOnlyToggle(
                "imadSaberThrowAfterCombatPickup",
                "Auto Pick Up Weapons When Combat Ends",
                false,
                "When enabled, all thrown weapons will automatically return to the player when "
                "Combat ends.");
            DrawSlider(
                "fmadSaberThrowPickupRadiusMultiplier",
                "Pickup Radius Multiplier",
                pickupRadiusMultiplier,
                0.25f,
                1.25f,
                0.05f,
                "%.2fx",
                "Multiplies how close the player must be to a dropped thrown item for proximity "
                "pickup. 1.0x uses the normal 200-unit pickup radius. This does not affect NPC "
                "throws or NPC recovery.");

            DrawHeader("Throw Physics");
            DrawSlider(
                "fmadSaberThrowZOffset",
                "Height Offset",
                settings.zOffset,
                -200.0f,
                200.0f,
                0.01f,
                "%.2f",
                "Offset the height at which the weapon is thrown.");
            DrawSlider(
                "fMinimumDistanceBounce",
                "Minimum Bounce Height On Impact",
                settings.minBounceDist,
                0.0f,
                200.0f,
                0.01f,
                "%.2f",
                "Minimum Height and object should bounce on impact. Increase this if you notice "
                "weapons falling through the floor.");

            DrawHeader("Throw Behavior");
            DrawSlider(
                "fmadSaberThrowStaminaCost",
                "Base Stamina Cost",
                settings.staminaCost,
                0.0f,
                1000.0f,
                1.0f,
                "%.0f",
                "Base Stamina of Throw Weapon and Throw Shield.");
            DrawToggle(
                "iEquipNextItemInStack",
                "Equip Next Item in Stack",
                settings.equipNextStack,
                "When enabled, throwing a weapon automatically equips another copy of the same "
                "item if the player has one in thier inventory.");
            DrawToggle(
                "imadSaberThrowLeftHand",
                "Prefer Left Hand for Throwing",
                settings.preferLeftHand,
                "When enabled, Throw Weapon prefers the weapon equipped in the left hand. If "
                "that hand has no weapon, it falls back to the right hand.");

            DrawHeader("Sneak Attacks");
            DrawSlider(
                "fSneakAttackBonusMultiplier",
                "Sneak Attack Bonus Multiplier",
                settings.sneakBonusMult,
                0.0f,
                10.0f,
                0.01f,
                "%.2fx",
                "Scales the sneak-attack multiplier applied to thrown weapon damage. 1.0 = full "
                "sneak damage, 0.33 = one-third Sneak damage.");
        }

        inline void __stdcall RenderThrowWeapon()
        {
            const auto settings = Settings::Get();

            DrawHeader("Unlock Requirements");
            DrawToggle(
                "iThrowWeaponRequiresPerk",
                "Throw Weapon Requires Perk",
                settings.weaponNeedsPerk,
                "When enabled, the Throw Weapon power must be unlocked by perk. By default, "
                "having one of the following perks will unlock it: Eagle Eye, Savage Strike, or "
                "Devastating Blow. These can be changed in the madSaberThrow.ini.");

            DrawHeader("Range & Speed");
            DrawSlider(
                "fmadSaberThrowNoReturnDistance",
                "Throw Distance",
                settings.noReturnDist,
                100.0f,
                10000.0f,
                100.0f,
                "%.0f",
                "How far the weapon travels when using Throw Weapon.");
            DrawSlider(
                "fmadSaberThrowNoReturnSpeed",
                "Throw Speed",
                settings.noReturnSpeed,
                0.1f,
                10.0f,
                0.1f,
                "%.2f",
                "How fast the weapon travels when using Throw Weapon.");

            DrawHeader("Throw Style");
            DrawSlider(
                "fThrowWeaponSpinMultiplier",
                "Spin Multiplier",
                settings.weaponSpinMult,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                "Multiplies Throw Weapon spin speed. Set to 0 for no spin.");
            DrawOrientationDropdown(
                "iThrowWeaponDefaultOrientation",
                "Default Orientation",
                settings.weaponOrient,
                "Default Throw Weapon visual orientation. WeaponTypeBoomerang and configured "
                "spear keywords can override this per weapon.");

            DrawHeader("Recovery");
            DrawToggle(
                "imadSaberThrowAutoEquip",
                "Auto Equip Weapon When Near",
                settings.autoEquipPickup,
                "When enabled, thrown weapon auto equips itself when near. If turned off, the "
                "weapon is added to your inventory instead.");
            DrawToggle(
                "iThrowWeaponRecastBoundWeaponOnImpact",
                "Recast Bound Weapon on Impact",
                settings.recastBoundOnImpact,
                "When enabled, bound weapon spells will be recast on impact."
                "No magicka cost is applied but spell's duration will match its previous remaining duration.");

            DrawHeader("Damage Scaling");
            DrawSlider(
                "fThrowDamageAt0Archery",
                "Damage Multiplier at 0 Archery",
                settings.archeryDamage0,
                0.25f,
                4.0f,
                0.05f,
                "%.2fx",
                "Damage multiplier applied to thrown weapon hits when Archery is 0. The "
                "multiplier blends from this value to the 100 Archery value as Archery "
                "increases. 1.0 = normal damage, 0.75 = 25% less damage, 1.25 = 25% more "
                "damage.");
            DrawSlider(
                "fThrowDamageAt100Archery",
                "Damage Multiplier at 100 Archery",
                settings.archeryDamage100,
                0.25f,
                4.0f,
                0.05f,
                "%.2fx",
                "Damage multiplier applied to thrown weapon hits when Archery is 100. The "
                "multiplier blends from the 0 Archery value to this value as Archery increases. "
                "1.0 = normal damage, 0.75 = 25% less damage, 1.25 = 25% more damage.");
            DrawSlider(
                "fNoReturnThrowPerkDamageIncreasePercent",
                "Configured Perk Damage Increase",
                settings.noReturnPerkPct,
                0.0f,
                1000.0f,
                1.0f,
                "%.0f%%",
                "Additional weapon damage when the player has the "
                "sNoReturnThrowDamagePerk configured in the INI.");

            DrawHeader("Stagger");
            DrawSlider(
                "fStaggerChanceNoReturn",
                "Stagger Chance",
                settings.noReturnStaggerChance,
                0.0f,
                1.0f,
                0.01f,
                "%.2f",
                "Chance to stagger the target. 0.0 = never, 1.0 = always.");
            DrawToggle(
                "iStaggerRequiresPerkNoReturn",
                "Stagger Requires Perk",
                settings.noReturnStaggerReq,
                "Can only stagger if the player has the perk specified in madSaberThrow.ini. "
                "Default perk is Power Shot (Archery).");
        }

        inline void __stdcall RenderThrowShield()
        {
            const auto settings = Settings::Get();

            DrawHeader("Unlock Requirements");
            DrawToggle(
                "iThrowShieldRequiresPerk",
                "Throw Shield Requires Perk",
                settings.shieldNeedsPerk,
                "When enabled, the Throw Shield power must be unlocked by the perk configured "
                "in madSaberThrow.ini. The default perk is Disarming Bash (Block).");

            DrawHeader("Shield Multipliers");
            DrawSlider("fDamageShield", "Damage Multiplier", settings.damageShield,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Damage multiplier for thrown shields.");
            DrawSlider("fSpeedShield", "Speed Multiplier", settings.speedShield,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Speed multiplier for thrown shields.");
            DrawSlider("fDistanceShield", "Distance Multiplier", settings.distanceShield,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Distance multiplier for thrown shields.");
            DrawSlider("fStaminaShield", "Stamina Cost Multiplier", settings.staminaShield,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Multiplier applied to the stamina cost of throwing a shield.");

            DrawHeader("Torch Multipliers");
            DrawSlider("fDamageTorch", "Damage Multiplier", settings.damageTorch,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Damage multiplier for thrown torches.");
            DrawSlider("fSpeedTorch", "Speed Multiplier", settings.speedTorch,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Speed multiplier for thrown torches.");
            DrawSlider("fDistanceTorch", "Distance Multiplier", settings.distanceTorch,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Distance multiplier for thrown torches.");
            DrawSlider("fStaminaTorch", "Stamina Cost Multiplier", settings.staminaTorch,
                0.0f, 10.0f, 0.05f, "%.2fx",
                "Multiplier applied to the stamina cost of throwing a torch.");

            DrawHeader("Stagger");
            DrawSlider(
                "fStaggerChanceShield",
                "Stagger Chance",
                settings.shieldStaggerChance,
                0.0f,
                1.0f,
                0.01f,
                "%.2f",
                "Chance for Throw Shield to stagger the target. 0.0 = never, 1.0 = always.");
        }

        inline void __stdcall RenderTelekineticThrow()
        {
            const auto settings = Settings::Get();

            DrawHeader("Range & Speed");
            DrawSlider(
                "fmadSaberThrowSpeed",
                "Throw Speed",
                settings.throwSpeed,
                0.1f,
                10.0f,
                0.1f,
                "%.2f",
                "How fast the weapon travels when using Telekinetic Throw.");
            DrawSlider(
                "fmadSaberThrowDistance",
                "Throw Distance",
                settings.throwDist,
                100.0f,
                10000.0f,
                100.0f,
                "%.0f",
                "How far the weapon travels when using Telekinetic Throw.");

            DrawHeader("Throw Style");
            DrawSlider(
                "fTelekineticThrowSpinMultiplier",
                "Spin Multiplier",
                settings.telekSpinMult,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                "Multiplies Telekinetic Throw spin speed. Set to 0 for no spin.");
            DrawOrientationDropdown(
                "iTelekineticThrowDefaultOrientation",
                "Default Orientation",
                settings.telekOrient,
                "Default Telekinetic Throw visual orientation. WeaponTypeBoomerang and configured "
                "spear keywords can override this per weapon.");

            DrawHeader("Behavior");
            DrawToggle(
                "imadSaberThrowContiueThrow",
                "Keep Going after NPC Hit",
                settings.continueThrow,
                "If enabled, thrown weapon will continue its path even after an actor is hit.");

            DrawHeader("Damage Scaling");
            DrawSlider(
                "fThrowDamageAt0Alteration",
                "Damage Multiplier at 0 Alteration",
                settings.altDamage0,
                0.25f,
                4.0f,
                0.05f,
                "%.2fx",
                "Damage multiplier applied to thrown weapon hits when Alteration is 0. The "
                "multiplier blends from this value to the 100 Alteration value as Alteration "
                "increases. 1.0 = normal damage, 0.75 = 25% less damage, 1.25 = 25% more "
                "damage.");
            DrawSlider(
                "fThrowDamageAt100Alteration",
                "Damage Multiplier at 100 Alteration",
                settings.altDamage100,
                0.25f,
                4.0f,
                0.05f,
                "%.2fx",
                "Damage multiplier applied to thrown weapon hits when Alteration is 100. The "
                "multiplier blends from the 0 Alteration value to this value as Alteration "
                "increases. 1.0 = normal damage, 0.75 = 25% less damage, 1.25 = 25% more "
                "damage.");
            DrawSlider(
                "fTelekineticThrowPerkDamageIncreasePercent",
                "Configured Perk Damage Increase",
                settings.telekPerkPct,
                0.0f,
                1000.0f,
                1.0f,
                "%.0f%%",
                "Additional returning telekinetic weapon damage when the player has "
                "the sTelekineticThrowDamagePerk configured in the INI.");

            DrawHeader("Stagger");
            DrawSlider(
                "fStaggerChance",
                "Stagger Chance",
                settings.staggerChance,
                0.0f,
                1.0f,
                0.01f,
                "%.2f",
                "Chance to stagger the target. 0.0 = never, 1.0 = always.");
            DrawToggle(
                "iStaggerRequiresPerk",
                "Stagger Requires Perk",
                settings.staggerNeedsPerk,
                "Can only stagger if the player has the perk specified in madSaberThrow.ini. "
                "Default perk is Impact (Destruction).");
        }

        inline void __stdcall RenderHotkeys()
        {
            ApplyPendingHotkeyBinding();
            const auto settings = Settings::Get();
            const auto gamepadHotkeys = Settings::GetGamepadHotkeys();

            DrawHeader("Throw Weapon");
            ImGuiMCP::SeparatorText("Keyboard / Mouse");
            DrawToggle(
                "iThrowWeaponHotkeyEnabled",
                "Enable Hotkey",
                settings.weaponHotkeyOn,
                "Enables the keyboard and mouse hotkey for Throw Weapon.");
            DrawHotkeyBinding(
                "iThrowWeaponHotkey",
                "Hotkey",
                settings.weaponHotkey,
                HotkeyBindingTarget::ThrowWeapon,
                "Keyboard key or mouse button to bind.");
            DrawToggle(
                "iThrowWeaponHotkeyUseModifier",
                "Use Modifier",
                settings.weaponHotkeyUseModifier,
                "Requires the configured modifier to be held when using the hotkey.");
            DrawHotkeyBinding(
                "iThrowWeaponHotkeyModifier",
                "Modifier",
                settings.weaponHotkeyModifier,
                HotkeyBindingTarget::ThrowWeaponModifier,
                "Keyboard key or mouse button that must be held.");

            ImGuiMCP::SeparatorText("Gamepad");
            DrawToggle(
                "iThrowWeaponGamepadHotkeyEnabled",
                "Enable Hotkey",
                gamepadHotkeys.weaponHotkeyOn,
                "Enables the gamepad hotkey for Throw Weapon.");
            DrawHotkeyBinding(
                "iThrowWeaponGamepadHotkey",
                "Hotkey",
                gamepadHotkeys.weaponHotkey,
                HotkeyBindingTarget::ThrowWeaponGamepad,
                "Gamepad button to bind.");
            DrawToggle(
                "iThrowWeaponGamepadHotkeyUseModifier",
                "Use Modifier",
                gamepadHotkeys.weaponHotkeyUseModifier,
                "Requires the configured gamepad modifier to be held when using the hotkey.");
            DrawHotkeyBinding(
                "iThrowWeaponGamepadHotkeyModifier",
                "Modifier",
                gamepadHotkeys.weaponHotkeyModifier,
                HotkeyBindingTarget::ThrowWeaponGamepadModifier,
                "Gamepad button that must be held.");

            DrawToggle(
                "iThrowWeaponHotkeyReturnOnHit",
                "Return on Hit",
                settings.returnOnHit,
                "When enabled, the Throw Weapon hotkey uses Telekinetic Throw returning behavior "
                "while retaining the Throw Weapon spell's normal stamina and perk checks.");

            DrawHeader("Shield Throw");
            ImGuiMCP::SeparatorText("Keyboard / Mouse");
            DrawToggle(
                "iThrowShieldHotkeyEnabled",
                "Enable Hotkey",
                settings.shieldHotkeyOn,
                "Enables the keyboard and mouse hotkey for Throw Shield.");
            DrawHotkeyBinding(
                "iThrowShieldHotkey",
                "Hotkey",
                settings.shieldHotkey,
                HotkeyBindingTarget::ThrowShield,
                "Keyboard key or mouse button to bind.");
            DrawToggle(
                "iThrowShieldHotkeyUseModifier",
                "Use Modifier",
                settings.shieldHotkeyUseModifier,
                "Requires the configured modifier to be held when using the hotkey.");
            DrawHotkeyBinding(
                "iThrowShieldHotkeyModifier",
                "Modifier",
                settings.shieldHotkeyModifier,
                HotkeyBindingTarget::ThrowShieldModifier,
                "Keyboard key or mouse button that must be held.");

            ImGuiMCP::SeparatorText("Gamepad");
            DrawToggle(
                "iThrowShieldGamepadHotkeyEnabled",
                "Enable Hotkey",
                gamepadHotkeys.shieldHotkeyOn,
                "Enables the gamepad hotkey for Throw Shield.");
            DrawHotkeyBinding(
                "iThrowShieldGamepadHotkey",
                "Hotkey",
                gamepadHotkeys.shieldHotkey,
                HotkeyBindingTarget::ThrowShieldGamepad,
                "Gamepad button to bind.");
            DrawToggle(
                "iThrowShieldGamepadHotkeyUseModifier",
                "Use Modifier",
                gamepadHotkeys.shieldHotkeyUseModifier,
                "Requires the configured gamepad modifier to be held when using the hotkey.");
            DrawHotkeyBinding(
                "iThrowShieldGamepadHotkeyModifier",
                "Modifier",
                gamepadHotkeys.shieldHotkeyModifier,
                HotkeyBindingTarget::ThrowShieldGamepadModifier,
                "Gamepad button that must be held.");
        }

        inline void __stdcall RenderHitbox()
        {
            const auto settings = Settings::Get();

            DrawHeader("Hitbox Multipliers");
            DrawSlider(
                "fActorSweepRadiusMultiplier",
                "Weapon Hitbox vs NPCs Multiplier",
                settings.actorSweepRadiusMult,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                "Adjust this to make the weapon's hitbox vs NPCs larger or smaller");
            DrawSlider(
                "fGroundSweepRadiusMultiplier",
                "Weapon Hitbox vs Static Objects Multiplier",
                settings.groundSweepRadiusMult,
                0.0f,
                10.0f,
                0.05f,
                "%.2fx",
                "Adjust this to make the weapon's hitbox vs static objects and the ground larger or smaller");
        }

        inline void __stdcall RenderHeadAndLimbs()
        {
            const auto settings = Settings::Get();

            DrawHeader("Locational Damage");
            DrawSlider(
                "fSaberThrowHeadshotMultiplier",
                "Headshot Damage Multiplier",
                settings.headshotMult,
                0.0f,
                100.0f,
                0.05f,
                "%.2fx",
                "Thrown weapon headshot damage multiplier, 1.0 = normal damage, 2.0 = double "
                "damage.");

            DrawHeader("Dismembering Framework Support");
            DrawToggle(
                "imadSaberThrowDismember",
                "Enable Dismemberment",
                settings.dismemberOn,
                "Requires Dismembering Framework. When enabled, thrown weapons can dismember if "
                "they kill.");
            DrawSlider(
                "fMaxDismemberDistance",
                "Max Distance to Node",
                settings.dismemberMaxDist,
                1.0f,
                1000.0f,
                0.1f,
                "%.2f",
                "Max units away a thrown weapon can be from a node (neck, ankle, etc.) to "
                "trigger dismemberment.");
        }

        inline void __stdcall RenderExperience()
        {
            const auto settings = Settings::Get();

            DrawHeader("Experience Gain");
            DrawToggle(
                "iWeaponXPEnabled",
                "Enable Weapon XP",
                settings.weaponXPOn,
                "When enabled, thrown weapon hits can award weapon skill experience.");
            DrawToggle(
                "iArcheryXPEnabled",
                "Enable Archery XP",
                settings.archeryXPOn,
                "When enabled, thrown weapon hits can award Archery experience.");

            DrawHeader("Experience Multipliers");
            DrawSlider(
                "fWeaponXPMult",
                "Weapon XP Multiplier",
                settings.weaponXPMult,
                0.0f,
                100.0f,
                0.05f,
                "%.2fx",
                "Multiplier applied to weapon skill experience from thrown weapon hits. 1.0 = "
                "normal XP, 0.5 = half XP, 2.0 = double XP.");
            DrawSlider(
                "fArcheryXPMult",
                "Archery XP Multiplier",
                settings.archeryXPMult,
                0.0f,
                100.0f,
                0.05f,
                "%.2fx",
                "Multiplier applied to Archery experience from thrown weapon hits. 1.0 = normal "
                "XP, 0.5 = half XP, 2.0 = double XP.");
        }

        inline void __stdcall RenderAiming()
        {
            const auto settings = Settings::Get();

            DrawHeader("Aim Offsets");
            DrawSlider("fAimOffsetHeight", "First-Person Aim Height Offset",
                settings.aimOffset, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset while standing.");
            DrawSlider("fAimOffsetHeightSneak", "First-Person Sneak Aim Height Offset",
                settings.sneakAimOffset, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset while sneaking.");
            DrawSlider("fAimOffsetHeight3rdPerson", "Third-Person Aim Height Offset",
                settings.thirdAimOffset, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset in third person while standing.");
            DrawSlider("fAimOffsetHeightSneak3rdPerson", "Third-Person Sneak Aim Height Offset",
                settings.thirdSneakAimOffset, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset in third person while sneaking.");
            DrawSlider("fAimOffsetHeightLockedOnNoReturn", "Locked-On Throw Weapon Aim Height Offset",
                settings.noReturnLockAim, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset for locked-on Throw Weapon.");
            DrawSlider("fAimOffsetHeightLockedOnTelekineticShield", "Locked-On Telekinetic / Shield Aim Height Offset",
                settings.telekLockAim, -1000.0f, 1000.0f, 0.01f, "%.2f",
                "Adjusts the vertical aiming offset for locked-on telekinetic and shield throws.");
        }

        inline void __stdcall RenderWeaponTypes()
        {
            const auto settings = Settings::Get();

            DrawWeaponType("One-Handed Sword", "1h swords",
                "fDamageSword1H", settings.damageSword1H,
                "fSpeedSword1H", settings.speedSword1H,
                "fDistanceSword1H", settings.distanceSword1H,
                "fStaminaSword1H", settings.staminaSword1H);
            DrawWeaponType("One-Handed Dagger", "1h daggers",
                "fDamageDagger1H", settings.damageDagger1H,
                "fSpeedDagger1H", settings.speedDagger1H,
                "fDistanceDagger1H", settings.distanceDagger1H,
                "fStaminaDagger1H", settings.staminaDagger1H);
            DrawWeaponType("Two-Handed Sword", "2h swords",
                "fDamageSword2H", settings.damageSword2H,
                "fSpeedSword2H", settings.speedSword2H,
                "fDistanceSword2H", settings.distanceSword2H,
                "fStaminaSword2H", settings.staminaSword2H);
            DrawWeaponType("One-Handed Axe", "1h axes",
                "fDamageAxe1H", settings.damageAxe1H,
                "fSpeedAxe1H", settings.speedAxe1H,
                "fDistanceAxe1H", settings.distanceAxe1H,
                "fStaminaAxe1H", settings.staminaAxe1H);
            DrawWeaponType("Two-Handed Axe", "2h axes",
                "fDamageAxe2H", settings.damageAxe2H,
                "fSpeedAxe2H", settings.speedAxe2H,
                "fDistanceAxe2H", settings.distanceAxe2H,
                "fStaminaAxe2H", settings.staminaAxe2H);
            DrawWeaponType("One-Handed Mace", "1h maces",
                "fDamageMace1H", settings.damageMace1H,
                "fSpeedMace1H", settings.speedMace1H,
                "fDistanceMace1H", settings.distanceMace1H,
                "fStaminaMace1H", settings.staminaMace1H);
            DrawWeaponType("Two-Handed Warhammer", "2h warhammers",
                "fDamageMace2H", settings.damageMace2H,
                "fSpeedMace2H", settings.speedMace2H,
                "fDistanceMace2H", settings.distanceMace2H,
                "fStaminaMace2H", settings.staminaMace2H);
            DrawWeaponType("One-Handed Spear", "1h spears",
                "fDamageSpear1H", settings.damageSpear1H,
                "fSpeedSpear1H", settings.speedSpear1H,
                "fDistanceSpear1H", settings.distanceSpear1H,
                "fStaminaSpear1H", settings.staminaSpear1H);
            DrawWeaponType("Two-Handed Spear", "2h spears",
                "fDamageSpear2H", settings.damageSpear2H,
                "fSpeedSpear2H", settings.speedSpear2H,
                "fDistanceSpear2H", settings.distanceSpear2H,
                "fStaminaSpear2H", settings.staminaSpear2H);

        }

    }

    inline bool Register()
    {
        bool expected = false;
        if (!Detail::g_registered.compare_exchange_strong(expected, true)) {
            return true;
        }

        ::SaberThrow::SetInvLoadObserver(
            &Detail::RefreshPlayerThrowPowerSpellsMainThread);

        if (!SKSEMenuFramework::IsInstalled() || !GetMenuFrameworkModule()) {
            Detail::g_registered.store(false);
            SKSE::log::warn(
                "[SaberThrow/Menu] SKSE Menu Framework is unavailable; "
                "the Throwable Weapons menu was not registered.");
            return false;
        }

        SKSEMenuFramework::SetSection(Detail::kMenuSectionName);
        SKSEMenuFramework::AddSectionItem("General", Detail::RenderGeneral);
        SKSEMenuFramework::AddSectionItem("Throw Weapon", Detail::RenderThrowWeapon);
        SKSEMenuFramework::AddSectionItem("Throw Shield", Detail::RenderThrowShield);
        SKSEMenuFramework::AddSectionItem("Telekinetic Throw", Detail::RenderTelekineticThrow);
        SKSEMenuFramework::AddSectionItem("Hotkeys", Detail::RenderHotkeys);
        SKSEMenuFramework::AddSectionItem("Hitbox", Detail::RenderHitbox);
        SKSEMenuFramework::AddSectionItem("Head & Limbs", Detail::RenderHeadAndLimbs);
        SKSEMenuFramework::AddSectionItem("Experience", Detail::RenderExperience);
        SKSEMenuFramework::AddSectionItem("Aiming", Detail::RenderAiming);
        SKSEMenuFramework::AddSectionItem("Weapon Types", Detail::RenderWeaponTypes);

        if (!Detail::g_hotkeyInput) {
            Detail::g_hotkeyInput =
                SKSEMenuFramework::AddInputEvent(&Detail::OnHotkeyBindingInput);
            SKSE::log::info(
                "[SaberThrow/Menu] Registered Menu Framework input capture for hotkey binding.");
        }

        if (Detail::g_menuListenerID < 0) {
            using FrameworkEventCallback =
                void(__stdcall*)(SKSEMenuFramework::Model::EventType);
            using RegisterFrameworkEvent =
                std::int64_t(__cdecl*)(FrameworkEventCallback);

            const HMODULE frameworkModule =
                GetModuleHandleW(L"SKSEMenuFramework.dll");
            const auto registerEvent = frameworkModule ?
                reinterpret_cast<RegisterFrameworkEvent>(
                    GetProcAddress(frameworkModule, "RegisterEvent")) :
                nullptr;

            if (registerEvent) {
                Detail::g_menuListenerID =
                    registerEvent(&Detail::OnMenuFrameworkEvent);

                if (Detail::g_menuListenerID < 0) {
                    SKSE::log::warn(
                        "[SaberThrow/Menu] SKSE Menu Framework rejected the global event listener.");
                }
            }
            else {
                SKSE::log::warn(
                    "[SaberThrow/Menu] Could not resolve SKSE Menu Framework RegisterEvent export; "
                    "menu-close spell/global refresh is disabled.");
            }
        }

        SKSE::log::info(
            "[SaberThrow/Menu] Registered Throwable Weapons SKSE with SKSE Menu Framework.");
        return true;
    }
}

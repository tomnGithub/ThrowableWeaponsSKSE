#pragma once

#include "RE/A/Actor.h"
#include "SKSE/SKSE.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <Windows.h>

#include <cstdint>

namespace SaberThrowAPI
{
    constexpr const char* kPluginName = "SaberThrow";
    constexpr const char* kPrimaryDLLName = "ThrowableWeaponsSKSE.dll";
    constexpr const char* kFallbackDLLName = "SaberThrow.dll";

    enum class InterfaceVersion : std::uint32_t
    {
        V1 = 1,
        V2 = 2
    };

    class IVSaberThrow1
    {
    public:
        [[nodiscard]] virtual std::uint32_t GetInterfaceVersion() const noexcept = 0;

        [[nodiscard]] virtual bool StartNPCNoReturnRightHandWeaponThrow(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept = 0;

        [[nodiscard]] virtual bool StartNPCNoReturnRightHandWeaponThrowMainThread(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept = 0;

        [[nodiscard]] virtual bool IsNPCNoReturnRightHandWeaponThrowPendingMainThread(
            RE::Actor* sourceActor) const noexcept = 0;
    };

    class IVSaberThrow2 : public IVSaberThrow1
    {
    public:
        [[nodiscard]] virtual bool StartNPCTelekineticRightHandWeaponThrow(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept = 0;

        [[nodiscard]] virtual bool StartNPCTelekineticRightHandWeaponThrowMainThread(
            RE::Actor* sourceActor,
            float totalTravel = -1.0f,
            float stepDist = -1.0f,
            float spinRateRadTick = 0.10f) noexcept = 0;

        [[nodiscard]] virtual bool IsNPCTelekineticRightHandWeaponThrowPendingMainThread(
            RE::Actor* sourceActor) const noexcept = 0;
    };

    using _RequestPluginAPI = void* (*)(InterfaceVersion interfaceVersion);

    [[nodiscard]] inline void* RequestPluginAPIRaw(InterfaceVersion interfaceVersion) noexcept
    {
        HMODULE pluginHandle = GetModuleHandleA(kPrimaryDLLName);
        if (!pluginHandle) {
            pluginHandle = GetModuleHandleA(kFallbackDLLName);
        }
        if (!pluginHandle) {
            return nullptr;
        }

        auto requestAPI = reinterpret_cast<_RequestPluginAPI>(
            GetProcAddress(pluginHandle, "RequestPluginAPI"));
        return requestAPI ? requestAPI(interfaceVersion) : nullptr;
    }

    [[nodiscard]] inline IVSaberThrow1* RequestPluginAPI(
        InterfaceVersion interfaceVersion = InterfaceVersion::V1) noexcept
    {
        if (interfaceVersion != InterfaceVersion::V1) {
            return nullptr;
        }
        return reinterpret_cast<IVSaberThrow1*>(
            RequestPluginAPIRaw(InterfaceVersion::V1));
    }

    [[nodiscard]] inline IVSaberThrow2* RequestPluginAPIV2() noexcept
    {
        return reinterpret_cast<IVSaberThrow2*>(
            RequestPluginAPIRaw(InterfaceVersion::V2));
    }
}

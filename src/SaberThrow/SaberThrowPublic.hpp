#pragma once

#include "SaberThrow/SaberThrowNPC.hpp"

namespace SaberThrow
{
    inline void StopSaberThrow()
    {
        g_saberRunning.store(false, std::memory_order_release);
        g_forceReturn.store(false, std::memory_order_release);
        g_saberSession.fetch_add(1, std::memory_order_acq_rel);

        for (auto& state : g_throwStates) {
            if (!state) {
                continue;
            }

            g_currentState = state.get();
            StopLoopSound("StopSaberThrow");
            ClearState("StopSaberThrow");
        }

        g_throwStates.clear();
        g_currentState = &g_fallbackState;
        QueueUpdateTask();
    }

    inline void RequestSaberReturnEarly()
    {
        g_forceReturn.store(true, std::memory_order_release);
        QueueUpdateTask();
    }

    inline void StartSaberThrow(
        const char* modName,
        RE::TESObjectREFR* ref,
        float snapForwardDist = 120.0f,
        float totalTravel = 10.0f,
        float stepDist = 0.15f,
        float spinRateRadTick = 0.10f,
        bool noReturnDynamic = false,
        bool noReturnSpearMode = kDefaultSpearMode,
        bool moveEquipToTemp = false,
        bool hasThrowHand = false,
        bool throwWasLeft = false)
    {

        std::string modNameString = modName ? modName : "";
        modNameString.erase(
            std::remove(modNameString.begin(), modNameString.end(), '|'),
            modNameString.end());

        if (modNameString.empty()) {
            return;
        }

        if (!ref) {
            return;
        }


        const std::uint32_t mySession = g_saberSession.load(std::memory_order_acquire);

        RE::NiPointer<RE::TESObjectREFR> refHandle(ref);

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return;
        }

        taskInterface->AddTask(
            [modNameString, refHandle, snapForwardDist, totalTravel, stepDist, spinRateRadTick, noReturnDynamic, noReturnSpearMode, moveEquipToTemp, hasThrowHand, throwWasLeft, mySession]() mutable
            {
                StartThrow(
                    modNameString,
                    refHandle,
                    snapForwardDist,
                    totalTravel,
                    stepDist,
                    spinRateRadTick,
                    noReturnDynamic,
                    noReturnSpearMode,
                    moveEquipToTemp,
                    mySession,
                    nullptr,
                    false,
                    RE::NiPoint3{},
                    RE::NiPoint3{},
                    hasThrowHand,
                    throwWasLeft);
            });
    }

    inline void ThrowWithPrecomputedPoints(
        const char* modName,
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& snapPos,
        const RE::NiPoint3& farPos,
        float totalTravel = 10.0f,
        float stepDist = 0.15f,
        float spinRateRadTick = 0.10f,
        bool noReturnDynamic = false,
        bool noReturnSpearMode = kDefaultSpearMode,
        bool moveEquipToTemp = false,
        bool hasThrowHand = false,
        bool throwWasLeft = false)
    {
        std::string modNameString = modName ? modName : "";
        modNameString.erase(
            std::remove(modNameString.begin(), modNameString.end(), '|'),
            modNameString.end());

        if (modNameString.empty() || !ref) {
            return;
        }

        const std::uint32_t mySession = g_saberSession.load(std::memory_order_acquire);

        RE::NiPointer<RE::TESObjectREFR> refHandle(ref);

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return;
        }

        taskInterface->AddTask(
            [modNameString, refHandle, snapPos, farPos, totalTravel, stepDist, spinRateRadTick, noReturnDynamic, noReturnSpearMode, moveEquipToTemp, hasThrowHand, throwWasLeft, mySession]() mutable
            {
                StartThrow(
                    modNameString,
                    refHandle,
                    kDefaultSnapDist,
                    totalTravel,
                    stepDist,
                    spinRateRadTick,
                    noReturnDynamic,
                    noReturnSpearMode,
                    moveEquipToTemp,
                    mySession,
                    nullptr,
                    true,
                    snapPos,
                    farPos,
                    hasThrowHand,
                    throwWasLeft);
            });
    }

    inline void Throw(
        const char* modName,
        RE::TESObjectREFR* ref,
        float totalTravel = 10.0f,
        float stepDist = 0.15f,
        float spinRateRadTick = 0.10f,
        bool noReturnDynamic = false,
        bool noReturnSpearMode = kDefaultSpearMode,
        bool moveEquipToTemp = false,
        bool hasThrowHand = false,
        bool throwWasLeft = false)
    {

        StartSaberThrow(
            modName,
            ref,
            kDefaultSnapDist,
            totalTravel,
            stepDist,
            spinRateRadTick,
            noReturnDynamic,
            noReturnSpearMode,
            moveEquipToTemp,
            hasThrowHand,
            throwWasLeft);
    }

    inline void ThrowNoReturn(
        const char* modName,
        RE::TESObjectREFR* ref,
        float totalTravel = 10.0f,
        float stepDist = 0.15f,
        float spinRateRadTick = 0.10f)
    {

        Throw(modName, ref, totalTravel, stepDist, spinRateRadTick, true, kDefaultSpearMode);
    }


}

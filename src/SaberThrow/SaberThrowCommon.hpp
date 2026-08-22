#pragma once

#include "RE/A/ActiveEffect.h"
#include "RE/A/Actor.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/A/ActorValues.h"
#include "RE/A/AlchemyItem.h"
#include "RE/B/BGSPerk.h"
#include "RE/B/BGSAttackData.h"
#include "RE/B/BGSAttackDataMap.h"
#include "RE/B/BGSEntryPoint.h"
#include "RE/B/BGSListForm.h"
#include "RE/B/BSFixedString.h"
#include "RE/B/BSAudioManager.h"
#include "RE/B/BGSSoundDescriptorForm.h"
#include "RE/B/BGSKeyword.h"
#include "RE/B/BGSKeywordForm.h"
#include "RE/B/BSAnimationGraphEvent.h"
#include "RE/B/BSInputDeviceManager.h"
#include "RE/B/BSTEvent.h"
#include "RE/B/ButtonEvent.h"
#include "RE/B/BSSoundHandle.h"
#include "RE/I/ImageSpaceModifierInstanceForm.h"
#include "RE/I/IObjectHandlePolicy.h"
#include "RE/I/IStackCallbackFunctor.h"
#include "RE/T/TESImageSpaceModifier.h"
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"
#include "RE/V/VirtualMachine.h"
#include "RE/E/Effect.h"
#include "RE/E/EffectSetting.h"
#include "RE/E/EnchantmentItem.h"
#include "RE/E/ExtraCharge.h"
#include "RE/E/ExtraDataList.h"
#include "RE/E/ExtraEnchantment.h"
#include "RE/E/ExtraPoison.h"
#include "RE/E/ExtraWorn.h"
#include "RE/E/ExtraWornLeft.h"
#include "RE/F/FunctionArguments.h"
#include "RE/G/GameSettingCollection.h"
#include "RE/B/bhkPickData.h"
#include "RE/B/bhkWorld.h"
#include "RE/C/CFilter.h"
#include "RE/C/CollisionLayers.h"
#include "RE/C/Character.h"
#include "RE/H/HitData.h"
#include "RE/H/HUDData.h"
#include "RE/M/MagicItem.h"
#include "RE/M/MagicCaster.h"
#include "RE/M/MagicTarget.h"
#include "RE/M/MagicSystem.h"
#include "RE/M/MenuOpenCloseEvent.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hkpCollidable.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiTransform.h"
#include "RE/P/PlayerCamera.h"
#include "RE/P/ProcessLists.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/S/Setting.h"
#include "RE/S/SpellItem.h"
#include "SKSE/Events.h"
#include "RE/T/TESHitEvent.h"
#include "RE/T/TESBoundObject.h"
#include "RE/T/TESCombatEvent.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESSpellCastEvent.h"
#include "RE/T/TESEquipEvent.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESObjectARMO.h"
#include "RE/T/TESObjectWEAP.h"
#include "RE/H/hkpMotion.h"
#include "RE/I/InventoryChanges.h"
#include "RE/I/InventoryEntryData.h"
#include "SKSE/SKSE.h"
#include "SKSE/Trampoline.h"

#include "SaberThrow/Settings.h"

#include "TrueDirectionalMovementAPI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <exception>
#include <random>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "DismemberingFramworkAPI.h"

namespace SaberThrow
{

    inline bool ActorDead(const RE::Actor* actor)
    {
        return actor && actor->GetActorRuntimeData().boolBits.all(RE::Actor::BOOL_BITS::kDead);
    }

    inline RE::NiMatrix3 EulerToMatrix(float pitch, float roll, float yaw)
    {
        const float cx = std::cos(pitch), sx = std::sin(pitch);
        const float cy = std::cos(roll), sy = std::sin(roll);
        const float cz = std::cos(yaw), sz = std::sin(yaw);

        RE::NiMatrix3 m;
        m.entry[0][0] = cz * cy;
        m.entry[0][1] = cz * sy * sx - sz * cx;
        m.entry[0][2] = cz * sy * cx + sz * sx;
        m.entry[1][0] = sz * cy;
        m.entry[1][1] = sz * sy * sx + cz * cx;
        m.entry[1][2] = sz * sy * cx - cz * sx;
        m.entry[2][0] = -sy;
        m.entry[2][1] = cy * sx;
        m.entry[2][2] = cy * cx;
        return m;
    }

    inline float Wrap0To2Pi(float a)
    {
        constexpr float TWO_PI = 6.2831853071795864769f;
        while (a >= TWO_PI) {
            a -= TWO_PI;
        }
        while (a < 0.0f) {
            a += TWO_PI;
        }
        return a;
    }

    inline float WrapNegPiToPi(float a)
    {
        constexpr float PI = 3.14159265358979323846f;
        constexpr float TWO_PI = 6.2831853071795864769f;
        while (a > PI) {
            a -= TWO_PI;
        }
        while (a < -PI) {
            a += TWO_PI;
        }
        return a;
    }

    inline RE::NiAVObject* Get3D(RE::TESObjectREFR* ref)
    {
        if (!ref) {
            return nullptr;
        }

        auto* node = ref->Get3D(false);
        return node;
    }

    inline std::uint64_t GetRayMask()
    {
        using L = RE::COL_LAYER;
        auto bit = [](L l) { return std::uint64_t{ 1 } << static_cast<int>(l); };
        static const std::uint64_t kMask =
            bit(L::kUnidentified) |
            bit(L::kStatic) |
            bit(L::kAnimStatic) |
            bit(L::kTransparent) |
            bit(L::kClutter) |
            bit(L::kWeapon) |
            bit(L::kProjectile) |
            bit(L::kTrees) |
            bit(L::kProps) |
            bit(L::kWater) |
            bit(L::kTerrain) |
            bit(L::kGround) |
            bit(L::kDebrisLarge) |
            bit(L::kTransparentSmallAnim) |
            bit(L::kInvisibleWall) |
            bit(L::kCharController) |
            bit(L::kStairHelper) |
            bit(L::kDeadBip) |
            bit(L::kBipedNoCC) |
            bit(L::kCollisionBox);
        return kMask;
    }

    inline bool LayerInMask(RE::NiAVObject* obj, std::uint64_t mask)
    {
        if (!obj) {
            return false;
        }
        const int layer = static_cast<int>(obj->GetCollisionLayer());
        return (layer >= 0 && layer < 64) && ((mask >> layer) & 1u);
    }

    inline bool RayIsClear(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore = {},
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr)
    {
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }

        if (!caster) {
            return true;
        }

        auto* cell = caster->GetParentCell();
        if (!cell) {
            return true;
        }

        auto* world = cell->GetbhkWorld();
        if (!world) {
            return true;
        }

        const float hwScale = RE::bhkWorld::GetWorldScale();

        RE::bhkPickData pickData{};
        pickData.rayInput.from = RE::hkVector4(from.x * hwScale, from.y * hwScale, from.z * hwScale, 0.0f);
        pickData.rayInput.to = RE::hkVector4(to.x * hwScale, to.y * hwScale, to.z * hwScale, 0.0f);
        pickData.rayInput.enableShapeCollectionFilter = false;

        RE::CFilter cfilter{};
        caster->GetCollisionFilterInfo(cfilter);
        constexpr std::uint32_t kItemPickerLayer = 40;
        cfilter.SetCollisionLayer(static_cast<RE::COL_LAYER>(kItemPickerLayer));
        pickData.rayInput.filterInfo = cfilter;

        world->PickObject(pickData);

        if (!pickData.rayOutput.HasHit()) {
            return true;
        }

        if (outHitPos) {
            const float f = pickData.rayOutput.hitFraction;
            outHitPos->x = from.x + ((to.x - from.x) * f);
            outHitPos->y = from.y + ((to.y - from.y) * f);
            outHitPos->z = from.z + ((to.z - from.z) * f);
        }

        RE::NiAVObject* hitObj = nullptr;
        {
            auto* collidable = pickData.rayOutput.rootCollidable;
            if (collidable) {
                typedef RE::NiAVObject* (*_GetAV)(const RE::hkpCollidable*);
                static auto getAV = REL::Relocation<_GetAV>(RELOCATION_ID(76160, 77988));
                hitObj = getAV(collidable);
            }
        }

        if (!hitObj) {
            return false;
        }

        for (auto* skip : ignore) {
            if (skip && skip == hitObj) {
                return true;
            }
        }

        RE::TESObjectREFR* hitRef = RE::TESObjectREFR::FindReferenceFor3D(hitObj);
        RE::Actor* hitActor = hitRef ? hitRef->As<RE::Actor>() : nullptr;

        if (outHitRef) {
            *outHitRef = hitRef;
        }
        if (outHitActor) {
            *outHitActor = hitActor;
        }

        if (hitActor && hitActor != caster) {
            return false;
        }

        const std::uint64_t mask = GetRayMask();
        if (LayerInMask(hitObj, mask)) {
            return false;
        }

        return true;
    }

    inline bool RayHitActor(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore = {},
        RE::Actor** outHitActor = nullptr,
        RE::TESObjectREFR** outHitRef = nullptr,
        RE::NiPoint3* outHitPos = nullptr)
    {
        if (outHitActor) {
            *outHitActor = nullptr;
        }
        if (outHitRef) {
            *outHitRef = nullptr;
        }
        if (outHitPos) {
            *outHitPos = RE::NiPoint3{};
        }

        if (!caster) {
            return false;
        }

        auto* cell = caster->GetParentCell();
        if (!cell) {
            return false;
        }

        auto* world = cell->GetbhkWorld();
        if (!world) {
            return false;
        }

        const float hwScale = RE::bhkWorld::GetWorldScale();

        RE::bhkPickData pickData{};
        pickData.rayInput.from = RE::hkVector4(from.x * hwScale, from.y * hwScale, from.z * hwScale, 0.0f);
        pickData.rayInput.to = RE::hkVector4(to.x * hwScale, to.y * hwScale, to.z * hwScale, 0.0f);
        pickData.rayInput.enableShapeCollectionFilter = false;

        RE::CFilter cfilter{};
        caster->GetCollisionFilterInfo(cfilter);
        constexpr std::uint32_t kItemPickerLayer = 40;
        cfilter.SetCollisionLayer(static_cast<RE::COL_LAYER>(kItemPickerLayer));
        pickData.rayInput.filterInfo = cfilter;

        world->PickObject(pickData);

        if (!pickData.rayOutput.HasHit()) {
            return false;
        }

        if (outHitPos) {
            const float f = pickData.rayOutput.hitFraction;
            outHitPos->x = from.x + ((to.x - from.x) * f);
            outHitPos->y = from.y + ((to.y - from.y) * f);
            outHitPos->z = from.z + ((to.z - from.z) * f);
        }

        RE::NiAVObject* hitObj = nullptr;
        {
            auto* collidable = pickData.rayOutput.rootCollidable;
            if (collidable) {
                typedef RE::NiAVObject* (*_GetAV)(const RE::hkpCollidable*);
                static auto getAV = REL::Relocation<_GetAV>(RELOCATION_ID(76160, 77988));
                hitObj = getAV(collidable);
            }
        }

        if (!hitObj) {
            return false;
        }

        for (auto* skip : ignore) {
            if (skip && skip == hitObj) {
                return false;
            }
        }

        RE::TESObjectREFR* hitRef = RE::TESObjectREFR::FindReferenceFor3D(hitObj);
        RE::Actor* hitActor = hitRef ? hitRef->As<RE::Actor>() : nullptr;

        if (!hitActor || hitActor == caster) {
            return false;
        }

        if (outHitRef) {
            *outHitRef = hitRef;
        }
        if (outHitActor) {
            *outHitActor = hitActor;
        }


        return true;
    }

    inline void Refresh3D(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return;
        }

        ref->Update3DPosition(true);
    }

    inline bool ForceKeyframedMotion(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return false;
        }

        const bool ok = ref->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true);
        return ok;
    }

    inline bool ForceDynamicMotion(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return false;
        }

        const bool ok = ref->SetMotionType(RE::hkpMotion::MotionType::kDynamic, true);
        return ok;
    }

    struct ThrownPickupPromptSuppressionEntry
    {
        RE::FormID formID{ 0 };
    };

    inline std::mutex g_pickupLock;
    inline std::vector<ThrownPickupPromptSuppressionEntry> g_pickupBlocks;
    inline std::atomic<RE::FormID> g_crosshairRefID{ 0 };

    inline void AddPickupBlock(RE::TESObjectREFR* ref)
    {
        if (!ref) {
            return;
        }

        const RE::FormID formID = ref->GetFormID();
        if (formID == 0) {
            return;
        }

        std::scoped_lock lock(g_pickupLock);
        const bool alreadyRegistered = std::any_of(
            g_pickupBlocks.begin(),
            g_pickupBlocks.end(),
            [formID](const ThrownPickupPromptSuppressionEntry& entry) {
                return entry.formID == formID;
            });

        if (!alreadyRegistered) {
            g_pickupBlocks.push_back({ formID });
        }
    }

    inline void RemovePickupBlock(RE::TESObjectREFR* ref)
    {
        if (!ref) {
            return;
        }

        const RE::FormID formID = ref->GetFormID();
        if (formID == 0) {
            return;
        }

        std::scoped_lock lock(g_pickupLock);
        const auto removeBegin = std::remove_if(
            g_pickupBlocks.begin(),
            g_pickupBlocks.end(),
            [formID](const ThrownPickupPromptSuppressionEntry& entry) {
                return entry.formID == formID;
            });
        g_pickupBlocks.erase(
            removeBegin,
            g_pickupBlocks.end());
    }

    inline void ClearPickupBlockRefs()
    {
        std::scoped_lock lock(g_pickupLock);
        g_pickupBlocks.clear();
        g_crosshairRefID.store(0, std::memory_order_release);
    }

    [[nodiscard]] inline bool IsThrownPickupPromptSuppressedFormID(
        RE::FormID formID)
    {
        if (formID == 0) {
            return false;
        }

        std::scoped_lock lock(g_pickupLock);
        return std::any_of(
            g_pickupBlocks.begin(),
            g_pickupBlocks.end(),
            [formID](const ThrownPickupPromptSuppressionEntry& entry) {
                return entry.formID == formID;
            });
    }

    namespace ThrownPickupPromptUI
    {
        inline constexpr std::uint32_t kHUDNoLabelType = 3;

        class CrosshairRefEventSink final :
            public RE::BSTEventSink<SKSE::CrosshairRefEvent>
        {
        public:
            static CrosshairRefEventSink* GetSingleton() noexcept
            {
                static CrosshairRefEventSink singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const SKSE::CrosshairRefEvent* event,
                RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override
            {
                RE::FormID formID = 0;
                if (event) {
                    auto crosshairPtr = event->crosshairRef;
                    auto* crosshairRef = crosshairPtr ? crosshairPtr.get() : nullptr;
                    if (crosshairRef) {
                        formID = crosshairRef->GetFormID();
                    }
                }

                g_crosshairRefID.store(formID, std::memory_order_release);
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        [[nodiscard]] inline RE::TESObjectREFRPtr ResolveHUDCrosshairRef(
            const RE::HUDData* data)
        {
            if (!data) {
                return {};
            }

            constexpr std::ptrdiff_t kHUDCrosshairOffset = 0x28;
            static_assert(sizeof(RE::ObjectRefHandle) == sizeof(std::uint32_t));

            std::uint32_t nativeHandle = 0;
            const auto* bytes = reinterpret_cast<const std::byte*>(data);
            std::memcpy(
                std::addressof(nativeHandle),
                bytes + kHUDCrosshairOffset,
                sizeof(nativeHandle));

            if (nativeHandle == 0) {
                return {};
            }

            RE::ObjectRefHandle handle;
            std::memcpy(
                std::addressof(handle),
                std::addressof(nativeHandle),
                sizeof(nativeHandle));
            return handle.get();
        }

        inline void HideActivateCard(RE::HUDData* data) noexcept
        {
            if (!data) {
                return;
            }

            constexpr std::ptrdiff_t kHUDTypeOffset = 0x10;
            constexpr std::ptrdiff_t kHUDCrosshairOffset = 0x28;
            constexpr std::ptrdiff_t kHUDShowOffset = 0x40;
            constexpr std::ptrdiff_t kHUDTypeDataOffset = 0x44;

            const std::uint32_t type = kHUDNoLabelType;
            const std::uint32_t zero32 = 0;
            const std::uint8_t hide = 0;
            auto* bytes = reinterpret_cast<std::byte*>(data);

            std::memcpy(
                bytes + kHUDTypeOffset,
                std::addressof(type),
                sizeof(type));
            data->text.clear();

            std::memcpy(
                bytes + kHUDCrosshairOffset,
                std::addressof(zero32),
                sizeof(zero32));

            std::memcpy(
                bytes + kHUDShowOffset,
                std::addressof(hide),
                sizeof(hide));

            std::memcpy(
                bytes + kHUDTypeDataOffset,
                std::addressof(zero32),
                sizeof(zero32));
        }

        struct SendHUDMessageHook
        {
            static void thunk(
                RE::UIMessageQueue* a_this,
                const RE::BSFixedString& a_menuName,
                RE::UI_MESSAGE_TYPE a_type,
                RE::HUDData* a_data)
            {
                RE::FormID crosshairFormID = 0;
                if (auto crosshairPtr = ResolveHUDCrosshairRef(a_data); crosshairPtr) {
                    crosshairFormID = crosshairPtr->GetFormID();
                }
                if (crosshairFormID == 0) {
                    crosshairFormID =
                        g_crosshairRefID.load(std::memory_order_acquire);
                }

                if (IsThrownPickupPromptSuppressedFormID(crosshairFormID)) {
                    HideActivateCard(a_data);
                }

                func(a_this, a_menuName, a_type, a_data);
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };
    }

    inline bool InstallPickupBlockHook()
    {
        static std::once_flag installOnce;
        static bool installed = false;

        std::call_once(installOnce, []() {
            try {
                auto* crosshairSource = SKSE::GetCrosshairRefEventSource();
                if (!crosshairSource) {
                    SKSE::log::error(
                        "[SaberThrow] cannot install thrown-item pickup prompt suppression: no SKSE crosshair event source.");
                    return;
                }

                crosshairSource->AddEventSink(
                    ThrownPickupPromptUI::CrosshairRefEventSink::GetSingleton());

                auto& trampoline = SKSE::GetTrampoline();
                if (trampoline.empty()) {
                    SKSE::AllocTrampoline(32);
                }
                if (trampoline.free_size() < 14) {
                    SKSE::log::error(
                        "[SaberThrow] cannot install thrown-item pickup prompt suppression hook: insufficient trampoline space.");
                    return;
                }

                REL::Relocation<std::uintptr_t> target{
                    RELOCATION_ID(39535, 40621)
                };

                const auto runtimeVersion = REL::Module::get().version();
                const bool isAE =
                    runtimeVersion[0] > 1 ||
                    (runtimeVersion[0] == 1 && runtimeVersion[1] >= 6);
                const std::uintptr_t callSite = target.address() +
                    (isAE ? 0x280 : 0x289);

                ThrownPickupPromptUI::SendHUDMessageHook::func =
                    trampoline.write_call<5>(
                        callSite,
                        ThrownPickupPromptUI::SendHUDMessageHook::thunk);

                installed = true;
                SKSE::log::info(
                    "[SaberThrow] installed per-reference thrown-item pickup prompt suppression hook.");
            }
            catch (const std::exception& e) {
                SKSE::log::error(
                    "[SaberThrow] failed to install thrown-item pickup prompt suppression hook: {}",
                    e.what());
            }
            catch (...) {
                SKSE::log::error(
                    "[SaberThrow] failed to install thrown-item pickup prompt suppression hook: unknown exception.");
            }
            });

        return installed;
    }

    inline void BlockThrowActivation(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return;
        }

        AddPickupBlock(ref);

        ref->SetActivationBlocked(true);
    }

    inline bool EnsureRefEnabled(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return false;
        }

        BlockThrowActivation(ref, caller ? caller : "EnsureReferenceEnabledMainThread");

        if (!ref->IsDisabled()) {
            return true;
        }

        ref->Enable(false);

        return !ref->IsDisabled();
    }

    inline bool EnsureRef3DLoaded(RE::TESObjectREFR* ref, const char* caller)
    {
        if (!ref) {
            return false;
        }

        if (Get3D(ref)) {
            return true;
        }

        Refresh3D(ref, caller ? caller : "EnsureReference3DLoadedMainThread");
        ForceKeyframedMotion(ref, caller ? caller : "EnsureReference3DLoadedMainThread");

        return Get3D(ref) != nullptr;
    }


    inline void MoveRaw(RE::TESObjectREFR* ref, float dx, float dy, float dz)
    {

        if (!ref) {
            return;
        }

        const RE::NiPoint3 cur = ref->GetPosition();

        const RE::NiPoint3 newPos{ cur.x + dx, cur.y + dy, cur.z + dz };
        ref->SetPosition(newPos);

        Refresh3D(ref, "MoveRaw");
        ForceKeyframedMotion(ref, "MoveRaw after Refresh3D");

    }

    inline void RotateRaw(RE::TESObjectREFR* ref, float pitch, float roll, float yaw, bool forceRotation = false)
    {

        if (!ref) {
            return;
        }


        RE::NiPoint3 newAngle{};
        if (forceRotation) {
            newAngle = RE::NiPoint3{ pitch, roll, yaw };
        }
        else {
            newAngle = RE::NiPoint3{ ref->data.angle.x + pitch, ref->data.angle.y + roll, ref->data.angle.z + yaw };
        }

        ref->data.angle = newAngle;


        auto* node3D = Get3D(ref);
        if (node3D) {
            node3D->local.rotate = EulerToMatrix(ref->data.angle.x, ref->data.angle.y, ref->data.angle.z);
        }

        Refresh3D(ref, "RotateRaw");
        ForceKeyframedMotion(ref, "RotateRaw after Refresh3D");

    }

    inline RE::NiPoint3 AddPoint(const RE::NiPoint3& a, const RE::NiPoint3& b)
    {
        return RE::NiPoint3{ a.x + b.x, a.y + b.y, a.z + b.z };
    }

    inline RE::NiPoint3 SubPoint(const RE::NiPoint3& a, const RE::NiPoint3& b)
    {
        return RE::NiPoint3{ a.x - b.x, a.y - b.y, a.z - b.z };
    }

    inline RE::NiPoint3 MulPoint(const RE::NiPoint3& p, float scale)
    {
        return RE::NiPoint3{ p.x * scale, p.y * scale, p.z * scale };
    }

    inline float LengthPoint(const RE::NiPoint3& p)
    {
        return std::sqrt((p.x * p.x) + (p.y * p.y) + (p.z * p.z));
    }

    inline bool NormalizePoint(RE::NiPoint3& p)
    {
        const float len = LengthPoint(p);
        if (len <= 1e-5f) {
            return false;
        }

        p.x /= len;
        p.y /= len;
        p.z /= len;
        return true;
    }


    inline float RadToDeg(float radians)
    {
        return radians * 57.29577951308232f;
    }

    inline const char* GetCompassName(float yawDeg)
    {
        yawDeg = std::fmod(yawDeg, 360.0f);
        if (yawDeg < 0.0f) {
            yawDeg += 360.0f;
        }

        if (yawDeg >= 337.5f || yawDeg < 22.5f) {
            return "N/+Y";
        }
        if (yawDeg < 67.5f) {
            return "NE";
        }
        if (yawDeg < 112.5f) {
            return "E/+X";
        }
        if (yawDeg < 157.5f) {
            return "SE";
        }
        if (yawDeg < 202.5f) {
            return "S/-Y";
        }
        if (yawDeg < 247.5f) {
            return "SW";
        }
        if (yawDeg < 292.5f) {
            return "W/-X";
        }
        return "NW";
    }



}

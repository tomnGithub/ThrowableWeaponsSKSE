#pragma once

#include "SaberThrow/SaberThrowCollision.hpp"

namespace SaberThrow
{

    inline TDM_API::IVTDM1* g_tdmInterface{ nullptr };
    inline bool g_tdmPostLoadDone{ false };
    inline bool g_tdmLateLogged{ false };

    struct TDMLockStatus
    {
        bool tdmAvailable{ false };
        bool tdmPostLoadDone{ false };
        bool targetLockActive{ false };
        bool validTarget{ false };
        RE::FormID targetFormID{ 0 };
    };

    inline TDM_API::IVTDM1* RequestTDM()
    {
        return static_cast<TDM_API::IVTDM1*>(
            TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V1));
    }

    inline void InitTDM()
    {
        g_tdmPostLoadDone = true;
        g_tdmInterface = RequestTDM();

        if (g_tdmInterface) {
            SKSE::log::info(
                "[SaberThrow] True Directional Movement API V1 detected. Target-lock throw support enabled.");
        }
        else {
            SKSE::log::info(
                "[SaberThrow] True Directional Movement API not detected at post-load. Will retry lazily at throw time.");
        }
    }

    inline void AddTDMListener()
    {
        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) {
            return;
        }

        messaging->RegisterListener([](SKSE::MessagingInterface::Message* message) {
            if (message && message->type == SKSE::MessagingInterface::kPostLoad) {
                InitTDM();
            }
            });
    }

    inline TDM_API::IVTDM1* GetTDM()
    {
        if (!g_tdmInterface) {
            g_tdmInterface = RequestTDM();

            if (g_tdmInterface && !g_tdmLateLogged) {
                g_tdmLateLogged = true;
                SKSE::log::info(
                    "[SaberThrow] True Directional Movement API V1 detected by lazy retry. Target-lock throw support enabled.");
            }
        }

        return g_tdmInterface;
    }

    inline RE::Actor* GetTDMTarget(RE::Actor* player, TDMLockStatus* outStatus = nullptr)
    {
        if (outStatus) {
            *outStatus = TDMLockStatus{};
        }

        if (!player) {
            return nullptr;
        }

        auto* tdm = GetTDM();
        if (outStatus) {
            outStatus->tdmPostLoadDone = g_tdmPostLoadDone;
        }

        if (!tdm) {
            return nullptr;
        }

        if (outStatus) {
            outStatus->tdmAvailable = true;
        }

        if (!tdm->GetTargetLockState()) {
            return nullptr;
        }

        if (outStatus) {
            outStatus->targetLockActive = true;
        }

        auto targetHandle = tdm->GetCurrentTarget();
        auto targetPtr = targetHandle.get();
        auto* target = targetPtr ? targetPtr.get() : nullptr;

        if (target && target != player) {
            if (outStatus) {
                outStatus->validTarget = true;
                outStatus->targetFormID = target->GetFormID();
            }
            return target;
        }

        return nullptr;
    }


    inline bool IsSneakingForAim(RE::Actor* actor)
    {
        return actor && actor->IsSneaking();
    }

    inline bool IsFPAim()
    {
        auto* camera = RE::PlayerCamera::GetSingleton();
        return camera && camera->IsInFirstPerson();
    }

    inline constexpr float kLockedAimOffset = 80.0f;

    inline float GetAimOffsetHeightThrow(
        RE::Actor* player,
        bool lockedOn,
        bool noReturnThrow)
    {
        const auto settings = ::SaberThrow::Settings::Get();

        if (lockedOn) {
            return noReturnThrow ?
                settings.noReturnLockAim +
                    kLockedAimOffset :
                settings.telekLockAim;
        }

        const bool firstPerson = IsFPAim();
        if (IsSneakingForAim(player)) {
            return firstPerson ?
                settings.sneakAimOffset :
                settings.thirdSneakAimOffset;
        }

        return firstPerson ?
            settings.aimOffset :
            settings.thirdAimOffset;
    }

    inline void ApplyAimHeight(
        RE::NiPoint3& destination,
        float aimOffset)
    {
        destination.z += aimOffset;
    }



    inline RE::BGSSoundDescriptorForm* GetSoundDescForm(
        RE::FormID formID,
        const std::string& pluginName)
    {
        if (formID == 0) {
            return nullptr;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        RE::BGSSoundDescriptorForm* soundForm = nullptr;

        if (dataHandler && !pluginName.empty()) {
            soundForm = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(
                formID,
                pluginName);
        }

        if (!soundForm) {
            if (auto* rawForm = RE::TESForm::LookupByID(formID)) {
                soundForm = rawForm->As<RE::BGSSoundDescriptorForm>();
            }
        }

        return soundForm;
    }

    inline bool PlaySoundAtPos(
        RE::BGSSoundDescriptorForm* soundForm,
        const RE::NiPoint3& position,
        const char* label,
        RE::NiAVObject* followObject = nullptr,
        bool player3DFallback = true)
    {
        if (!soundForm) {
            return false;
        }

        auto* audioManager = RE::BSAudioManager::GetSingleton();
        if (!audioManager) {
            SKSE::log::warn("[SaberThrow] {} sound failed: no BSAudioManager.", label ? label : "impact");
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

        bool attachedToObject = false;
        if (followObject) {
            handle.SetObjectToFollow(followObject);
            attachedToObject = true;
        }
        else if (player3DFallback) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (auto* player3D = player->Get3D(false)) {
                    handle.SetObjectToFollow(player3D);
                    attachedToObject = true;
                }
            }
        }

        handle.Play();

        SKSE::log::debug(
            "[SaberThrow] played {} sound via SNDR descriptor{}",
            label ? label : "impact",
            attachedToObject ? " attached to 3D" : " at position only");

        return true;
    }

    inline bool PlaySoundByFormAtPos(
        RE::FormID formID,
        const std::string& pluginName,
        const RE::NiPoint3& position,
        const char* label,
        RE::NiAVObject* followObject = nullptr,
        bool player3DFallback = true)
    {
        auto* soundForm = GetSoundDescForm(formID, pluginName);
        if (!soundForm) {
            if (formID != 0) {
                SKSE::log::warn(
                    "[SaberThrow] {} sound lookup failed: id=0x{:08X}, plugin='{}'",
                    label ? label : "impact",
                    formID,
                    pluginName);
            }
            return false;
        }

        return PlaySoundAtPos(
            soundForm,
            position,
            label,
            followObject,
            player3DFallback);
    }

    inline void PlayThrowTeleportFX(const RE::NiPoint3& position)
    {
        const auto settings = ::SaberThrow::Settings::Get();

        if (!settings.teleportFX) {
            return;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!dataHandler || !player) {
            return;
        }

        if (settings.teleportIMODID != 0 &&
            !settings.teleportIMODPlugin.empty()) {

            auto* imod = dataHandler->LookupForm<RE::TESImageSpaceModifier>(
                settings.teleportIMODID,
                settings.teleportIMODPlugin);

            if (imod) {
                auto* imodInstance = RE::ImageSpaceModifierInstanceForm::Trigger(
                    imod,
                    settings.teleportIMODStrength,
                    nullptr);

                if (imodInstance) {
                    imodInstance->Apply();
                }
            }
        }

        if (settings.teleportSoundID != 0) {
            RE::BGSSoundDescriptorForm* soundForm = nullptr;

            if (!settings.teleportSoundPlugin.empty()) {
                soundForm = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(
                    settings.teleportSoundID,
                    settings.teleportSoundPlugin);
            }

            if (!soundForm) {
                if (auto* rawForm = RE::TESForm::LookupByID(settings.teleportSoundID)) {
                    soundForm = rawForm->As<RE::BGSSoundDescriptorForm>();
                }
            }

            if (!soundForm) {
                SKSE::log::warn(
                    "[SaberThrow] Teleport sound lookup failed: id=0x{:08X}, plugin='{}'",
                    settings.teleportSoundID,
                    settings.teleportSoundPlugin);
                return;
            }

            auto* audioManager = RE::BSAudioManager::GetSingleton();
            if (!audioManager) {
                SKSE::log::warn("[SaberThrow] Teleport sound failed: no BSAudioManager.");
                return;
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

            if (auto* player3D = player->Get3D(false)) {
                handle.SetObjectToFollow(player3D);
            }

            handle.Play();

            SKSE::log::info(
                "[SaberThrow] Played teleport sound: id=0x{:08X}, plugin='{}'",
                settings.teleportSoundID,
                settings.teleportSoundPlugin);
        }
    }
    inline bool BlinkToThrow(
        RE::TESObjectREFR* thrownRef,
        const RE::NiPoint3* throwTravelDir = nullptr,
        float maxGroundSnap = 128.0f,
        const RE::NiPoint3* thrownTargetPos = nullptr)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !thrownRef) {
            return false;
        }

        constexpr float kPlayerRadius = 48.0f;
        constexpr float kPlayerHeight = kPlayerRadius * 3.0f;
        constexpr float kBackoffStep = 32.0f;
        constexpr float kFloorLift = 8.0f;
        constexpr std::uint32_t kMaxTries = 8;

        constexpr float kGroundProbeLift = 48.0f;
        constexpr float kGroundProbeExtra = 24.0f;
        constexpr float kGroundRadius = 32.0f;
        constexpr float kGroundDiag = kGroundRadius * 0.70710678f;

        const RE::NiPoint3 thrownPos = thrownTargetPos ? *thrownTargetPos : thrownRef->GetPosition();
        const RE::NiPoint3 playerPos = player->GetPosition();

        std::vector<RE::NiAVObject*> ignoreList = MakePlayerIgnoreList(player);
        AddIgnoreNode(ignoreList, thrownRef->Get3D(false));

        RE::NiPoint3 approachDir{};
        if (throwTravelDir) {
            approachDir = *throwTravelDir;
        }
        else {
            approachDir = RE::NiPoint3{
                thrownPos.x - playerPos.x,
                thrownPos.y - playerPos.y,
                thrownPos.z - playerPos.z
            };
        }

        approachDir.z = 0.0f;

        if (!NormalizePoint(approachDir)) {
            const float yaw = player->data.angle.z;
            approachDir = RE::NiPoint3{
                std::sin(yaw),
                std::cos(yaw),
                0.0f
            };

            if (!NormalizePoint(approachDir)) {
                approachDir = RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
            }
        }

        const float clearanceRadius = kPlayerRadius * 0.65f;
        const RE::NiPoint3 clearanceOffsets[] = {
            { 0.0f,             0.0f,             0.0f },
            {  clearanceRadius, 0.0f,             0.0f },
            { -clearanceRadius, 0.0f,             0.0f },
            { 0.0f,              clearanceRadius, 0.0f },
            { 0.0f,             -clearanceRadius, 0.0f }
        };

        const RE::NiPoint3 groundOffsets[] = {
            { 0.0f,                 0.0f,                 0.0f },
            {  kGroundRadius, 0.0f,                0.0f },
            { -kGroundRadius, 0.0f,                0.0f },
            { 0.0f,                  kGroundRadius, 0.0f },
            { 0.0f,                 -kGroundRadius, 0.0f },
            {  kGroundDiag,   kGroundDiag,   0.0f },
            {  kGroundDiag,  -kGroundDiag,   0.0f },
            { -kGroundDiag,   kGroundDiag,   0.0f },
            { -kGroundDiag,  -kGroundDiag,   0.0f }
        };

        for (std::uint32_t i = 0; i < kMaxTries; ++i) {
            const float backoff = kPlayerRadius + (static_cast<float>(i) * kBackoffStep);

            RE::NiPoint3 candidate{
                thrownPos.x - (approachDir.x * backoff),
                thrownPos.y - (approachDir.y * backoff),
                thrownPos.z
            };

            bool valid = true;

            for (const auto& offset : clearanceOffsets) {
                const RE::NiPoint3 heightFrom{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z + 4.0f
                };

                const RE::NiPoint3 heightTo{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z + kPlayerHeight
                };

                RE::Actor* heightHitActor = nullptr;
                RE::TESObjectREFR* heightHitRef = nullptr;
                RE::NiPoint3 heightHitPos{};

                if (!RayIsClear(
                    heightFrom,
                    heightTo,
                    player,
                    ignoreList,
                    &heightHitActor,
                    &heightHitRef,
                    &heightHitPos)) {

                    if (heightHitActor) {
                        valid = false;
                        break;
                    }

                    const float blockedHeight = heightHitPos.z - heightFrom.z;
                    const float moveDown = kPlayerHeight - blockedHeight;

                    if (moveDown <= 0.0f) {
                        continue;
                    }

                    const RE::NiPoint3 downTo{
                        candidate.x,
                        candidate.y,
                        candidate.z - moveDown
                    };

                    if (!RayIsClear(candidate, downTo, player, ignoreList)) {
                        valid = false;
                        break;
                    }

                    candidate.z -= moveDown;
                }
            }

            if (!valid) {
                continue;
            }

            if (maxGroundSnap > 0.0f) {
                bool foundGround = false;
                float bestGroundZ = -1000000000.0f;

                for (const auto& offset : groundOffsets) {
                    const RE::NiPoint3 groundFrom{
                        candidate.x + offset.x,
                        candidate.y + offset.y,
                        candidate.z + kGroundProbeLift
                    };

                    const RE::NiPoint3 groundTo{
                        candidate.x + offset.x,
                        candidate.y + offset.y,
                        candidate.z - maxGroundSnap - kGroundProbeExtra
                    };

                    RE::Actor* groundActor = nullptr;
                    RE::NiPoint3 groundHit{};

                    if (!RayIsClear(
                        groundFrom,
                        groundTo,
                        player,
                        ignoreList,
                        &groundActor,
                        nullptr,
                        &groundHit)) {

                        if (!groundActor) {
                            foundGround = true;
                            bestGroundZ = std::max(bestGroundZ, groundHit.z);
                        }
                    }
                }

                if (foundGround) {
                    candidate.z = bestGroundZ + kFloorLift;
                }
            }

            for (const auto& offset : clearanceOffsets) {
                const RE::NiPoint3 heightFrom{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z + 4.0f
                };

                const RE::NiPoint3 heightTo{
                    candidate.x + offset.x,
                    candidate.y + offset.y,
                    candidate.z + kPlayerHeight
                };

                if (!RayIsClear(heightFrom, heightTo, player, ignoreList)) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                continue;
            }

            player->SetPosition(candidate, true);
            player->Update3DPosition(true);

            PlayThrowTeleportFX(candidate);

            return true;
        }

        return false;
    }

    inline bool RaySolidHit(
        const RE::NiPoint3& from,
        const RE::NiPoint3& to,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore,
        RE::NiPoint3* outHitPos = nullptr)
    {
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

        RE::NiAVObject* hitObj = nullptr;
        {
            auto* collidable = pickData.rayOutput.rootCollidable;
            if (collidable) {
                typedef RE::NiAVObject* (*_GetAV)(const RE::hkpCollidable*);
                static auto getAV = REL::Relocation<_GetAV>(RELOCATION_ID(76160, 77988));
                hitObj = getAV(collidable);
            }
        }

        for (auto* skip : ignore) {
            if (skip && skip == hitObj) {
                return false;
            }
        }

        if (hitObj) {
            const std::uint64_t mask = GetRayMask();
            if (!LayerInMask(hitObj, mask)) {
                return false;
            }
        }

        if (outHitPos) {
            const float f = pickData.rayOutput.hitFraction;
            outHitPos->x = from.x + ((to.x - from.x) * f);
            outHitPos->y = from.y + ((to.y - from.y) * f);
            outHitPos->z = from.z + ((to.z - from.z) * f);
        }

        return true;
    }

    inline RE::NiPoint3 GetBlinkHitPoint(
        RE::NiAVObject* cameraNode,
        RE::Actor* caster,
        const std::vector<RE::NiAVObject*>& ignore,
        const RE::NiPoint3& longAimVector)
    {
        const RE::NiPoint3 from = cameraNode->world.translate;
        const RE::NiPoint3 to = TransformPoint(cameraNode->world, longAimVector);

        RE::NiPoint3 hitPos{};
        if (RaySolidHit(from, to, caster, ignore, &hitPos)) {
            return hitPos;
        }

        return to;
    }

    inline RE::NiAVObject* Find3DObject(RE::NiAVObject* root3D, const char* nodeName)
    {
        if (!root3D || !nodeName || nodeName[0] == '\0') {
            return nullptr;
        }

        auto* rootNode = root3D->AsNode();
        if (!rootNode) {
            return nullptr;
        }

        return rootNode->GetObjectByName(RE::BSFixedString(nodeName));
    }

    inline RE::NiAVObject* GetFPHandNode(RE::Actor* player)
    {
        if (!player) {
            return nullptr;
        }

        auto* firstPerson3D = player->Get3D(true);
        if (!firstPerson3D) {
            return nullptr;
        }

        constexpr const char* kFirstPersonNodes[] = {
            "NPC Spine2 [Spn2]",
            "NPC Spine1 [Spn1]",
            "NPC Spine [Spn0]",
            "NPC Pelvis [Pelv]",
            "NPC Root [Root]",
            "NPC R UpperArm [RUar]",
            "NPC R Forearm [RLar]",
            "NPC R Hand [RHnd]",
            "Weapon",
            "WEAPON"
        };

        for (const auto* nodeName : kFirstPersonNodes) {
            if (auto* node = Find3DObject(firstPerson3D, nodeName)) {
                return node;
            }
        }

        return nullptr;
    }

    inline RE::NiAVObject* GetTPHandNode(RE::Actor* player)
    {
        if (!player) {
            return nullptr;
        }

        constexpr const char* kThirdPersonNodes[] = {
            "NPC Spine2 [Spn2]",
            "NPC Spine1 [Spn1]",
            "NPC Spine [Spn0]",
            "NPC Pelvis [Pelv]",
            "NPC Root [Root]",
            "NPC R Hand [RHnd]",
            "Weapon",
            "WEAPON"
        };

        for (const auto* nodeName : kThirdPersonNodes) {
            if (auto* node = player->GetNodeByName(nodeName)) {
                return node;
            }
        }

        return nullptr;
    }

    inline RE::NiAVObject* GetThrowHandNode(RE::Actor* player, bool firstPerson)
    {
        if (!player) {
            return nullptr;
        }

        if (firstPerson) {
            if (auto* firstPersonNode = GetFPHandNode(player)) {
                return firstPersonNode;
            }
        }

        return GetTPHandNode(player);
    }

    inline RE::NiPoint3 GetCenterThrowSource(RE::Actor* player)
    {
        if (!player) {
            return RE::NiPoint3{};
        }

        const RE::NiPoint3 playerPos = player->GetPosition();
        return RE::NiPoint3{ playerPos.x, playerPos.y, playerPos.z + 100.0f };
    }

    inline RE::NiPoint3 GetHandThrowSource(RE::Actor* player, bool firstPerson)
    {
        if (auto* handNode = GetThrowHandNode(player, firstPerson)) {
            return handNode->world.translate;
        }

        return GetCenterThrowSource(player);
    }

    inline bool GetBlinkThrowPoints(
        RE::Actor* player,
        float snapForwardDist,
        float totalTravel,
        RE::NiPoint3& outSnapPos,
        RE::NiPoint3& outFarPos,
        TDMLockStatus* outTDMStatus = nullptr)
    {

        if (!player) {
            return false;
        }

        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return false;
        }

        auto* cameraNode = static_cast<RE::NiAVObject*>(camera->cameraRoot.get());
        if (!cameraNode) {
            return false;
        }


        const float aimRayDist = std::fmax(100.0f, totalTravel + 2000.0f);
        const RE::NiPoint3 aimVector{ 0.0f, aimRayDist, 0.0f };
        const RE::NiPoint3 aimVectorDoubled{ 0.0f, aimRayDist + 2000.0f, 0.0f };

        const bool firstPerson = camera->IsInFirstPerson();

        const RE::NiPoint3 source = GetHandThrowSource(player, firstPerson);
        RE::NiPoint3 target{};

        if (auto* lockedTarget = GetTDMTarget(player, outTDMStatus)) {
            target = lockedTarget->GetPosition();
            target.z += 50.0f;
        }
        else if (firstPerson) {
            target = TransformPoint(cameraNode->world, aimVector);
        }
        else {
            const auto ignore = MakePlayerIgnoreList(player);
            const RE::NiPoint3 cameraCollision = GetBlinkHitPoint(cameraNode, player, ignore, aimVectorDoubled);

            RE::NiPoint3 cameraDir = SubPoint(cameraCollision, cameraNode->world.translate);
            if (!NormalizePoint(cameraDir)) {
                return false;
            }

            target = AddPoint(cameraNode->world.translate, MulPoint(cameraDir, aimRayDist));
        }

        RE::NiPoint3 aimDir = SubPoint(target, source);
        if (!NormalizePoint(aimDir)) {
            return false;
        }

        (void)snapForwardDist;

        outSnapPos = source;
        outFarPos = AddPoint(outSnapPos, MulPoint(aimDir, totalTravel));

        return true;
    }

    inline bool GetBlinkFarPosFromView(
        RE::Actor* player,
        const RE::NiPoint3& source,
        float totalTravel,
        RE::NiPoint3& outFarPos,
        TDMLockStatus* outTDMStatus = nullptr)
    {
        if (!player) {
            return false;
        }

        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return false;
        }

        auto* cameraNode = static_cast<RE::NiAVObject*>(camera->cameraRoot.get());
        if (!cameraNode) {
            return false;
        }

        const float aimRayDist = std::fmax(100.0f, totalTravel + 2000.0f);
        const RE::NiPoint3 aimVector{ 0.0f, aimRayDist, 0.0f };
        const RE::NiPoint3 aimVectorDoubled{ 0.0f, aimRayDist + 2000.0f, 0.0f };

        RE::NiPoint3 target{};
        if (auto* lockedTarget = GetTDMTarget(player, outTDMStatus)) {
            target = lockedTarget->GetPosition();
            target.z += 50.0f;
        }
        else if (camera->IsInFirstPerson()) {
            target = TransformPoint(cameraNode->world, aimVector);
        }
        else {
            const auto ignore = MakePlayerIgnoreList(player);
            const RE::NiPoint3 cameraCollision = GetBlinkHitPoint(
                cameraNode,
                player,
                ignore,
                aimVectorDoubled);

            RE::NiPoint3 cameraDir = SubPoint(cameraCollision, cameraNode->world.translate);
            if (!NormalizePoint(cameraDir)) {
                return false;
            }

            target = AddPoint(cameraNode->world.translate, MulPoint(cameraDir, aimRayDist));
        }

        RE::NiPoint3 aimDir = SubPoint(target, source);
        if (!NormalizePoint(aimDir)) {
            return false;
        }

        outFarPos = AddPoint(source, MulPoint(aimDir, totalTravel));
        return true;
    }

    inline bool GetFallbackFarPosYaw(
        RE::Actor* player,
        const RE::NiPoint3& source,
        float totalTravel,
        RE::NiPoint3& outFarPos)
    {
        if (!player) {
            return false;
        }

        const float yaw = player->data.angle.z;
        RE::NiPoint3 dir{ std::sin(yaw), std::cos(yaw), 0.0f };
        if (!NormalizePoint(dir)) {
            return false;
        }

        outFarPos = AddPoint(source, MulPoint(dir, totalTravel));
        return true;
    }

    inline bool GetFallbackPoints(
        RE::Actor* player,
        float snapForwardDist,
        float totalTravel,
        RE::NiPoint3& outSnapPos,
        RE::NiPoint3& outFarPos)
    {
        if (!player) {
            return false;
        }

        const RE::NiPoint3 p = player->GetPosition();
        const float yaw = player->data.angle.z;
        const float fx = std::sin(yaw);
        const float fy = std::cos(yaw);

        const RE::NiPoint3 source{ p.x, p.y, p.z + 100.0f };
        const RE::NiPoint3 dir{ fx, fy, 0.0f };


        outSnapPos = AddPoint(source, MulPoint(dir, snapForwardDist));
        outFarPos = AddPoint(outSnapPos, MulPoint(dir, totalTravel));
        return true;
    }


}

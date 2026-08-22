#pragma once

#include "SaberThrow/SaberThrowDamage.hpp"

namespace SaberThrow
{
    inline constexpr bool kSpearDiag = false;

    inline float SpearRadToDeg(float radians)
    {
        return radians * (180.0f / kPiRad);
    }

    inline RE::NiPoint3 GetSpearMatrixCol(const RE::NiMatrix3& matrix, std::uint32_t column)
    {
        return RE::NiPoint3{
            matrix.entry[0][column],
            matrix.entry[1][column],
            matrix.entry[2][column]
        };
    }

    inline float SpearAxisErrorDeg(RE::NiPoint3 expected, RE::NiPoint3 actual)
    {
        if (!NormalizePoint(expected) || !NormalizePoint(actual)) {
            return -1.0f;
        }

        const float dot = std::clamp(
            (expected.x * actual.x) +
            (expected.y * actual.y) +
            (expected.z * actual.z),
            -1.0f,
            1.0f);
        return SpearRadToDeg(std::acos(dot));
    }

    inline float SpearMatrixError(
        const RE::NiMatrix3& expected,
        const RE::NiMatrix3& actual)
    {
        float maxError = 0.0f;
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t column = 0; column < 3; ++column) {
                maxError = std::max(
                    maxError,
                    std::fabs(expected.entry[row][column] - actual.entry[row][column]));
            }
        }
        return maxError;
    }

    inline float SpearMatrixDet(const RE::NiMatrix3& matrix)
    {
        return
            matrix.entry[0][0] *
                ((matrix.entry[1][1] * matrix.entry[2][2]) -
                 (matrix.entry[1][2] * matrix.entry[2][1])) -
            matrix.entry[0][1] *
                ((matrix.entry[1][0] * matrix.entry[2][2]) -
                 (matrix.entry[1][2] * matrix.entry[2][0])) +
            matrix.entry[0][2] *
                ((matrix.entry[1][0] * matrix.entry[2][1]) -
                 (matrix.entry[1][1] * matrix.entry[2][0]));
    }

    inline bool ShouldLogSpearOrient()
    {
        if constexpr (!kSpearDiag) {
            return false;
        }

        const std::uint32_t tick = g_state.spinTickCount;
        if (g_state.diagLoggedTick == tick) {
            return false;
        }

        const bool shouldLog =
            tick <= 10 ||
            (tick <= 90 && (tick % 15) == 0) ||
            (tick > 90 && (tick % 120) == 0);

        if (shouldLog) {
            g_state.diagLoggedTick = tick;
        }
        return shouldLog;
    }

    inline void LogSpearMatrixCompare(
        RE::TESObjectREFR* ref,
        const char* phase,
        const RE::NiMatrix3& expected,
        const RE::NiMatrix3& actual,
        float expectedSpinRad)
    {
        if constexpr (!kSpearDiag) {
            return;
        }

        const RE::NiPoint3 expectedX = GetSpearMatrixCol(expected, 0);
        const RE::NiPoint3 expectedTip = GetSpearMatrixCol(expected, 1);
        const RE::NiPoint3 expectedZ = GetSpearMatrixCol(expected, 2);
        const RE::NiPoint3 actualX = GetSpearMatrixCol(actual, 0);
        const RE::NiPoint3 actualTip = GetSpearMatrixCol(actual, 1);
        const RE::NiPoint3 actualZ = GetSpearMatrixCol(actual, 2);

        logger::info(
            "[SPEAR_DIAG][{}] session={} ref=0x{:08X} spinTick={} spinDeg={:.3f} "
            "tipErrDeg={:.4f} localXErrDeg={:.4f} localZErrDeg={:.4f} "
            "maxAbsErr={:.7f} expectedDet={:.7f} actualDet={:.7f}",
            phase ? phase : "UNKNOWN",
            g_state.session,
            ref ? ref->GetFormID() : 0,
            g_state.spinTickCount,
            SpearRadToDeg(expectedSpinRad),
            SpearAxisErrorDeg(expectedTip, actualTip),
            SpearAxisErrorDeg(expectedX, actualX),
            SpearAxisErrorDeg(expectedZ, actualZ),
            SpearMatrixError(expected, actual),
            SpearMatrixDet(expected),
            SpearMatrixDet(actual));

        logger::info(
            "[SPEAR_DIAG][{}][AXES] expectedX=({:.7f},{:.7f},{:.7f}) "
            "expectedTipY=({:.7f},{:.7f},{:.7f}) expectedZ=({:.7f},{:.7f},{:.7f}) "
            "actualX=({:.7f},{:.7f},{:.7f}) actualTipY=({:.7f},{:.7f},{:.7f}) "
            "actualZ=({:.7f},{:.7f},{:.7f})",
            phase ? phase : "UNKNOWN",
            expectedX.x, expectedX.y, expectedX.z,
            expectedTip.x, expectedTip.y, expectedTip.z,
            expectedZ.x, expectedZ.y, expectedZ.z,
            actualX.x, actualX.y, actualX.z,
            actualTip.x, actualTip.y, actualTip.z,
            actualZ.x, actualZ.y, actualZ.z);

        logger::info(
            "[SPEAR_DIAG][{}][MATRIX] expected=[{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f}] "
            "actual=[{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f}]",
            phase ? phase : "UNKNOWN",
            expected.entry[0][0], expected.entry[0][1], expected.entry[0][2],
            expected.entry[1][0], expected.entry[1][1], expected.entry[1][2],
            expected.entry[2][0], expected.entry[2][1], expected.entry[2][2],
            actual.entry[0][0], actual.entry[0][1], actual.entry[0][2],
            actual.entry[1][0], actual.entry[1][1], actual.entry[1][2],
            actual.entry[2][0], actual.entry[2][1], actual.entry[2][2]);
    }

    inline void LogSpearNodeCompare(
        RE::TESObjectREFR* ref,
        const char* phasePrefix,
        const RE::NiMatrix3& expected,
        float expectedSpinRad)
    {
        if constexpr (!kSpearDiag) {
            return;
        }

        auto* node3D = Get3D(ref);
        if (!node3D) {
            logger::info(
                "[SPEAR_DIAG][{}_NO3D] session={} ref=0x{:08X} spinTick={} spinDeg={:.3f}",
                phasePrefix ? phasePrefix : "UNKNOWN",
                g_state.session,
                ref ? ref->GetFormID() : 0,
                g_state.spinTickCount,
                SpearRadToDeg(expectedSpinRad));
            return;
        }

        const std::string localPhase = fmt::format("{}_LOCAL", phasePrefix ? phasePrefix : "UNKNOWN");
        const std::string worldPhase = fmt::format("{}_WORLD", phasePrefix ? phasePrefix : "UNKNOWN");
        LogSpearMatrixCompare(
            ref,
            localPhase.c_str(),
            expected,
            node3D->local.rotate,
            expectedSpinRad);
        LogSpearMatrixCompare(
            ref,
            worldPhase.c_str(),
            expected,
            node3D->world.rotate,
            expectedSpinRad);
    }

    inline void CacheSpearFixedPose()
    {
        g_state.spearFixedValid = false;
        g_state.spearEulerValid = false;
        g_state.spearFixedMatrix = RE::NiMatrix3{};
        g_state.spearFixedEuler = RE::NiPoint3{};
        g_state.spearEuler = RE::NiPoint3{};
        g_state.diagExpectedMatrix = RE::NiMatrix3{};
        g_state.diagExpectedSpin = 0.0f;
        g_state.diagLoggedTick = static_cast<std::uint32_t>(-1);
        g_state.diagExpectedValid = false;

        RE::NiPoint3 throwDir = SubPoint(g_state.farPos, g_state.snapPos);
        if (!NormalizePoint(throwDir)) {
            throwDir = GetThrowDir(g_state.snapPos, g_state.farPos);
        }

        g_state.spearFixedMatrix = MakeSpearMatrix(throwDir, 0.0f);
        g_state.spearFixedEuler = MatrixToEulerCont(
            g_state.spearFixedMatrix,
            GetSpearEngineEuler(g_state.throwPitchRad, g_state.throwYawRad));
        g_state.spearEuler = g_state.spearFixedEuler;
        g_state.spearFixedValid = true;
        g_state.spearEulerValid = true;

        g_state.visualPitchRad = g_state.spearFixedEuler.x;
        g_state.visualRollRad = g_state.spearFixedEuler.y;
        g_state.baseYawRad = g_state.spearFixedEuler.z;

        if constexpr (kSpearDiag) {
            const RE::NiPoint3 oldFormulaEuler = GetSpearEngineEuler(
                g_state.throwPitchRad,
                g_state.throwYawRad);
            const RE::NiPoint3 expectedTip = GetSpearMatrixCol(
                g_state.spearFixedMatrix,
                1);

            logger::info(
                "[SPEAR_DIAG][BEGIN] session={} spinTick={} dir=({:.7f},{:.7f},{:.7f}) "
                "expectedTipY=({:.7f},{:.7f},{:.7f}) throwPitchDeg={:.4f} throwYawDeg={:.4f} "
                "nativeEulerDeg=({:.4f},{:.4f},{:.4f}) oldFormulaEulerDeg=({:.4f},{:.4f},{:.4f})",
                g_state.session,
                g_state.spinTickCount,
                throwDir.x, throwDir.y, throwDir.z,
                expectedTip.x, expectedTip.y, expectedTip.z,
                SpearRadToDeg(g_state.throwPitchRad),
                SpearRadToDeg(g_state.throwYawRad),
                SpearRadToDeg(g_state.spearFixedEuler.x),
                SpearRadToDeg(g_state.spearFixedEuler.y),
                SpearRadToDeg(g_state.spearFixedEuler.z),
                SpearRadToDeg(oldFormulaEuler.x),
                SpearRadToDeg(oldFormulaEuler.y),
                SpearRadToDeg(oldFormulaEuler.z));

            logger::info(
                "[SPEAR_DIAG][BEGIN_MATRIX] session={} unrolledExpected=[{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f};{:.7f},{:.7f},{:.7f}]",
                g_state.session,
                g_state.spearFixedMatrix.entry[0][0],
                g_state.spearFixedMatrix.entry[0][1],
                g_state.spearFixedMatrix.entry[0][2],
                g_state.spearFixedMatrix.entry[1][0],
                g_state.spearFixedMatrix.entry[1][1],
                g_state.spearFixedMatrix.entry[1][2],
                g_state.spearFixedMatrix.entry[2][0],
                g_state.spearFixedMatrix.entry[2][1],
                g_state.spearFixedMatrix.entry[2][2]);
        }
    }

    inline RE::NiPoint3 GetSpearPersistEuler(
        const RE::NiMatrix3& spearMatrix)
    {
        if (!g_state.spearFixedValid) {
            CacheSpearFixedPose();
        }

        const RE::NiPoint3 previousEuler = g_state.spearEulerValid ?
            g_state.spearEuler :
            g_state.spearFixedEuler;

        g_state.spearEuler = MatrixToEulerCont(
            spearMatrix,
            previousEuler);
        g_state.spearEulerValid = true;
        return g_state.spearEuler;
    }

    inline void ApplySpearPose(RE::TESObjectREFR* ref)
    {
        if (!ref) {
            return;
        }

        if (!g_state.spearFixedValid) {
            CacheSpearFixedPose();
        }

        RE::NiPoint3 throwDir = SubPoint(g_state.farPos, g_state.snapPos);
        if (!NormalizePoint(throwDir)) {
            throwDir = GetThrowDir(g_state.snapPos, g_state.farPos);
        }

        const RE::NiMatrix3 spearVisualMatrix =
            MakeSpearMatrix(throwDir, g_state.spinZ);

        const RE::NiPoint3 spearEngineEuler =
            GetSpearPersistEuler(spearVisualMatrix);

        g_state.visualPitchRad = g_state.spearFixedEuler.x;
        g_state.visualRollRad = g_state.spearFixedEuler.y;
        g_state.baseYawRad = g_state.spearFixedEuler.z;

        const bool logSample = ShouldLogSpearOrient();

        if (logSample && g_state.diagExpectedValid) {
            LogSpearNodeCompare(
                ref,
                "PREVIOUS_TICK",
                g_state.diagExpectedMatrix,
                g_state.diagExpectedSpin);
        }

        if (logSample) {
            const RE::NiPoint3 actualAngleBefore = ref->data.angle;
            logger::info(
                "[SPEAR_DIAG][INPUT] session={} ref=0x{:08X} spinTick={} "
                "dir=({:.7f},{:.7f},{:.7f}) spinDeg={:.4f} "
                "persistEulerDeg=({:.4f},{:.4f},{:.4f}) dataAngleBeforeDeg=({:.4f},{:.4f},{:.4f})",
                g_state.session,
                ref->GetFormID(),
                g_state.spinTickCount,
                throwDir.x, throwDir.y, throwDir.z,
                SpearRadToDeg(g_state.spinZ),
                SpearRadToDeg(spearEngineEuler.x),
                SpearRadToDeg(spearEngineEuler.y),
                SpearRadToDeg(spearEngineEuler.z),
                SpearRadToDeg(actualAngleBefore.x),
                SpearRadToDeg(actualAngleBefore.y),
                SpearRadToDeg(actualAngleBefore.z));
        }

        ref->data.angle = spearEngineEuler;

        auto applyExactMatrix = [&]() {
            if (auto* node3D = Get3D(ref)) {
                node3D->local.rotate = spearVisualMatrix;
                node3D->world.rotate = spearVisualMatrix;
            }
        };

        applyExactMatrix();
        if (logSample) {
            LogSpearNodeCompare(
                ref,
                "PRIME_EXACT",
                spearVisualMatrix,
                g_state.spinZ);
        }

        Refresh3D(ref, "ApplyNoReturnSpearPoseMainThread diagnostic Refresh3D");
        if (logSample) {
            const RE::NiPoint3 angleAfterRefresh = ref->data.angle;
            logger::info(
                "[SPEAR_DIAG][ANGLE_AFTER_REFRESH] session={} ref=0x{:08X} spinTick={} dataAngleDeg=({:.4f},{:.4f},{:.4f})",
                g_state.session,
                ref->GetFormID(),
                g_state.spinTickCount,
                SpearRadToDeg(angleAfterRefresh.x),
                SpearRadToDeg(angleAfterRefresh.y),
                SpearRadToDeg(angleAfterRefresh.z));

            LogSpearNodeCompare(
                ref,
                "POST_REFRESH_VS_UNROLLED",
                g_state.spearFixedMatrix,
                0.0f);
            LogSpearNodeCompare(
                ref,
                "POST_REFRESH_VS_ROLLED",
                spearVisualMatrix,
                g_state.spinZ);
        }

        ForceKeyframedMotion(
            ref,
            "ApplyNoReturnSpearPoseMainThread diagnostic after Refresh3D");
        if (logSample) {
            const RE::NiPoint3 angleAfterHavok = ref->data.angle;
            logger::info(
                "[SPEAR_DIAG][ANGLE_AFTER_HAVOK] session={} ref=0x{:08X} spinTick={} dataAngleDeg=({:.4f},{:.4f},{:.4f})",
                g_state.session,
                ref->GetFormID(),
                g_state.spinTickCount,
                SpearRadToDeg(angleAfterHavok.x),
                SpearRadToDeg(angleAfterHavok.y),
                SpearRadToDeg(angleAfterHavok.z));

            LogSpearNodeCompare(
                ref,
                "POST_HAVOK_VS_ROLLED",
                spearVisualMatrix,
                g_state.spinZ);
        }

        applyExactMatrix();
        if (logSample) {
            LogSpearNodeCompare(
                ref,
                "FINAL_EXACT",
                spearVisualMatrix,
                g_state.spinZ);
            logger::info(
                "[SPEAR_DIAG][END_SAMPLE] session={} ref=0x{:08X} spinTick={}",
                g_state.session,
                ref->GetFormID(),
                g_state.spinTickCount);
        }

        g_state.diagExpectedMatrix = spearVisualMatrix;
        g_state.diagExpectedSpin = g_state.spinZ;
        g_state.diagExpectedValid = true;
    }

    inline void MoveAndApplySpearPose(
        RE::TESObjectREFR* ref,
        const RE::NiPoint3& newPos)
    {
        if (!ref) {
            return;
        }

        const RE::NiPoint3 cur = ref->GetPosition();
        MoveRaw(ref, newPos.x - cur.x, newPos.y - cur.y, newPos.z - cur.z);
        ApplySpearPose(ref);
    }

    inline void ApplyNoReturnPose(RE::TESObjectREFR* ref)
    {
        if (!ref) {
            return;
        }

        RE::NiPoint3 throwDir = SubPoint(g_state.farPos, g_state.snapPos);
        if (!NormalizePoint(throwDir)) {
            throwDir = GetThrowDir(g_state.snapPos, g_state.farPos);
        }

        if (IsThrownShield(ref)) {
            return;
        }

        const RE::NiMatrix3 visualMatrix = MakeNoReturnMatrix(
            throwDir,
            g_state.throwYawRad,
            g_state.spinZ);

        RotateMatrix(ref, visualMatrix);
    }


}

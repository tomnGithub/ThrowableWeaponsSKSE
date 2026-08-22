#pragma once

#include "Common/PCH.h"

namespace Papyrus
{
    inline constexpr std::string_view script = "SaberThrowScript";

    using VM = RE::BSScript::IVirtualMachine;

#define STATIC_ARGS RE::StaticFunctionTag*

#define BIND(func, ...) \
    a_vm.RegisterFunction(#func, script, func, ##__VA_ARGS__)

    bool RegisterFunctions(VM* a_vm);
}

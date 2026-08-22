#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#include "Plugin.h"

#define DLLEXPORT __declspec(dllexport)

#ifndef NDEBUG
#define LOG_DEBUG(msg, ...) logger::debug(msg, ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...)
#endif

namespace logger = SKSE::log;

using namespace std::literals;

namespace util
{
    using SKSE::stl::report_and_fail;
}

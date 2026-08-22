#include "Papyrus/papyrus.h"
#include "SaberThrow/SaberThrow_SKSE.hpp"
#include "SaberThrow/Settings.h"
#include "SaberThrow/SaberThrowMenu.h"

namespace
{
	constexpr REL::Version MIN_RUNTIME{ 1, 6, 1130, 0 };

	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

#ifndef NDEBUG
		const auto level = spdlog::level::debug;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%^%l%$] %v"s);
	}
}

SKSEPluginVersion = []()
	{
		SKSE::PluginVersionData v{};

		v.PluginVersion(REL::Version{ 1, 0, 0, 0 });
		v.PluginName("SaberThrow"sv);
		v.AuthorName("MadAborModding"sv);
		v.UsesAddressLibrary();
		v.UsesUpdatedStructs();

		return v;
	}();

static void MessageEventCallback(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		logger::info("Data loaded - initializing SaberThrow runtime."sv);
		logger::info("Loading SaberThrow MCM settings..."sv);
		::SaberThrow::Settings::LoadMCMSettings();
		logger::info("Registering Throwable Weapons SKSE Menu Framework pages..."sv);
		::SaberThrow::Menu::Register();
		logger::info("Installing SaberThrow load-safety listeners..."sv);
		::SaberThrow::AddLoopSoundListeners();
		logger::info("Installing SaberThrow spell-cast listener..."sv);
		::SaberThrow::AddThrowSpellSink();
		logger::info("Installing SaberThrow hotkey input listener..."sv);
		::SaberThrow::AddThrowHotkeySink();
		logger::info("Installing SaberThrow thrown-item pickup prompt suppression..."sv);
		if (!::SaberThrow::InstallPickupBlockHook()) {
			logger::warn("Failed to install SaberThrow thrown-item pickup prompt suppression hook."sv);
		}
		logger::info("Startup tasks finished, enjoy your game!"sv);
		break;
	default:
		break;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("================================================="sv);
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());
	logger::info("Author: MadAborModding"sv);
	logger::info("================================================="sv);

	SKSE::Init(a_skse);

	const auto ver = a_skse->RuntimeVersion();
	if (ver < MIN_RUNTIME) {
		logger::critical("Unsupported runtime version: {}"sv, ver.string());
		return false;
	}

	auto* papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(Papyrus::RegisterFunctions)) {
		logger::critical("Failed to register SaberThrow Papyrus functions."sv);
		return false;
	}

	SKSE::GetMessagingInterface()->RegisterListener(&MessageEventCallback);

	logger::info("Plugin loaded successfully."sv);
	return true;
}
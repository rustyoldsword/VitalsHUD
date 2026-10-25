#include "Diseases.h"
#include "Feed.h"
#include "Hud.h"
#include "Input.h"
#include "Menu.h"
#include "Meters.h"
#include "Sls.h"
#include "Settings.h"

namespace
{
	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "VitalsHUD.log";

		auto log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		using namespace Vitals;
		switch (a_message->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			Settings::Load();
			Menu::Register();
			Hud::Register();
			Feed::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			// every source plugin (WetFunction NG, OSL Aroused) has loaded its DLL by now
			Meters::Init();
			Sls::Init();
			Diseases::LoadDefinitions();
			Feed::Init();
			Meters::Start();
			Input::Register();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Meters::SetReady(false);
			Diseases::Clear();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Meters::OnGameLoaded();
			Diseases::OnGameLoaded();
			Meters::SetReady(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse);

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::stl::report_and_fail("Unable to register the SKSE message listener.");
	}

	logger::info("{} loaded", plugin->GetName());
	return true;
}

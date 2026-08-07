#include "Diagnostics.h"
#include "MapLODFix.h"
#include "Settings.h"

namespace
{
	class LoadWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static LoadWatcher& Get()
		{
			static LoadWatcher singleton;
			return singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event || a_event->menuName != RE::LoadingMenu::MENU_NAME) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (a_event->opening) {
				LODFix::MapLODFix::Get().OnLoadingScreenOpened();
			} else {
				LODFix::MapLODFix::Get().OnLoadingScreenClosed();
				LODFix::Diagnostics::LogState("worldspace-entered");
			}

			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		LoadWatcher() = default;
	};

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&LoadWatcher::Get());
			}
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			LODFix::MapLODFix::Get().OnGameLoaded();
			LODFix::Diagnostics::LogState("game-loaded");
			break;

		default:
			break;
		}
	}
}

SKSEPluginInfo(
	.Version = REL::Version{ SKLF_VERSION_MAJOR, SKLF_VERSION_MINOR, SKLF_VERSION_PATCH },
	.Name = "SkyrimLODFixes"sv,
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	logger::info("SkyrimLODFixes {}.{}.{}",
		SKLF_VERSION_MAJOR, SKLF_VERSION_MINOR, SKLF_VERSION_PATCH);

	LODFix::Settings::Get().Load();

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	return true;
}

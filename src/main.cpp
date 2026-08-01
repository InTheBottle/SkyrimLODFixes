#include "Diagnostics.h"
#include "MapLODFix.h"
#include "Settings.h"

namespace
{
	bool RuntimeSupported()
	{
		if (REL::Module::IsAE()) {
			return true;
		}

		const auto* which = REL::Module::IsVR() ? "VR" : "SE";
		logger::warn(
			"This build targets Skyrim AE (1.6.x) only; detected {}. No fixes will be "
			"applied. The internals it relies on were mapped from the AE binary and its "
			"address-library IDs do not carry over.",
			which);
		return false;
	}

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

	if (!RuntimeSupported()) {
		// Loaded, inert, and honest about it.
		return true;
	}

	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
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
	});

	return true;
}

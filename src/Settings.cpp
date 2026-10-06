#include "Settings.h"

namespace LODFix
{
	namespace
	{
		constexpr auto kIniPath = L"Data/SKSE/Plugins/SkyrimLODFixes.ini";
	}

	Settings& Settings::Get()
	{
		static Settings singleton;
		return singleton;
	}

	void Settings::Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();

		if (const auto result = ini.LoadFile(kIniPath); result != SI_OK) {
			logger::info("No config at Data/SKSE/Plugins/SkyrimLODFixes.ini; using defaults");
			return;
		}

		enableLODReset = ini.GetBoolValue("General", "bEnableLODReset", enableLODReset);
		enableInstanceGroupLock =
			ini.GetBoolValue("General", "bEnableInstanceGroupLock", enableInstanceGroupLock);
		logDiagnostics = ini.GetBoolValue("General", "bLogDiagnostics", logDiagnostics);

		logger::info("Config: lodReset={} instanceGroupLock={} diagnostics={}", enableLODReset,
			enableInstanceGroupLock, logDiagnostics);
	}
}

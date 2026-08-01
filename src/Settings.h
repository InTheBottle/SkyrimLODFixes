#pragma once

namespace LODFix
{
	/** Plugin configuration, read from SKSE/Plugins/SkyrimLODFixes.ini. */
	struct Settings
	{
		bool enableLODReset = true;

		/** One state line on game load and after each loading screen. Off by default. */
		bool logDiagnostics = false;

		static Settings& Get();

		/** Loads the INI, keeping defaults for anything absent or malformed. */
		void Load();
	};
}

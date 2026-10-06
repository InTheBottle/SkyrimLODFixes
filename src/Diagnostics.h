#pragma once

namespace LODFix::Diagnostics
{
	void LogState(std::string_view a_reason);

	/** What one manager still holds and draws; used on managers the player has left. */
	void LogManager(std::string_view a_reason, std::uintptr_t a_manager);
}

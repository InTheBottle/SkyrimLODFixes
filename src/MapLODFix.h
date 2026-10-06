#pragma once

namespace LODFix
{
	class MapLODFix
	{
	public:
		static MapLODFix& Get();

		/** Makes Update's own manager switch use the complete teardown; call at load. */
		void Install();

		void OnGameLoaded();

		void OnLoadingScreenOpened();

		void OnLoadingScreenClosed();

	private:
		MapLODFix() = default;

		// Where the player was, and which manager served it, at a transition boundary.
		struct Waypoint
		{
			std::uintptr_t manager = 0;
			std::uint32_t worldspace = 0;
			std::int32_t cellX = 0;
			std::int32_t cellY = 0;
			bool valid = false;
		};

		static bool Sample(Waypoint& a_out);

		void RunDetach(const Waypoint& a_from, const Waypoint& a_to, std::int32_t a_movedCells,
			const char* a_reason);

		Waypoint _departure;

		// Manager left on the last manager switch; reported again at the next loading screen.
		std::uintptr_t _departedManager = 0;
	};
}

#include "MapLODFix.h"

#include "Diagnostics.h"
#include "NodeGeometry.h"
#include "Offsets.h"
#include "Settings.h"

#include <optional>

namespace LODFix
{
	namespace
	{
		std::uintptr_t ActiveTerrainManager()
		{
			static REL::Relocation<std::uintptr_t*> slot{ REL::VariantID(
				Offsets::kActiveTerrainManagerSE, Offsets::kActiveTerrainManager,
				Offsets::kActiveTerrainManagerVR) };
			auto* p = slot.get();
			return p ? *p : 0;
		}

		template <typename T>
		T Read(std::uintptr_t a_base, std::ptrdiff_t a_offset)
		{
			return *reinterpret_cast<T*>(a_base + a_offset);
		}

		// Arg 2 clears the initialised flag; arg 3 widens the release mask from 0x2F to 0x3F,
		// which adds the map handle pair. That is the engine's own complete teardown.
		void DetachManager(std::uintptr_t a_manager)
		{
			using Fn = void (*)(std::uintptr_t, char, char);
			static REL::Relocation<Fn> detach{ REL::VariantID(
				Offsets::kDetachManagerSE, Offsets::kDetachManager,
				Offsets::kDetachManagerVR) };
			detach(a_manager, 1, 1);
		}

		// Update's manager switch detaches the outgoing (still active) manager with arg 3 = 0.
		struct SwitchDetach
		{
			static void Thunk(std::uintptr_t a_manager, char a_clearInitialised, char)
			{
				Diagnostics::LogManager("switch-before", a_manager);
				original(a_manager, a_clearInitialised, 1);
				Diagnostics::LogManager("switch-after", a_manager);
				logger::info("[lod] manager switch: released manager {:#x} with Detach(mgr, 1, 1)",
					a_manager);
			}

			static inline REL::Relocation<decltype(Thunk)> original;
		};

		// The one call to Detach with a zeroed third argument followed by a store to the active-manager global.
		std::optional<std::uintptr_t> FindSwitchDetachCall(std::uintptr_t a_detach, std::uintptr_t a_active)
		{
			constexpr std::uintptr_t kWindow = 0x10000;
			std::optional<std::uintptr_t> found;
			for (auto site = a_detach - kWindow; site < a_detach + kWindow; ++site) {
				const auto* code = reinterpret_cast<const std::uint8_t*>(site);
				if (code[0] != 0xE8 ||
					site + 5 + static_cast<std::intptr_t>(*reinterpret_cast<const std::int32_t*>(code + 1)) != a_detach) {
					continue;
				}

				bool zeroed = false;
				for (auto k = site - 12; k < site; ++k) {
					const auto* prior = reinterpret_cast<const std::uint8_t*>(k);
					zeroed = zeroed || (prior[0] == 0x45 && prior[1] == 0x33 && prior[2] == 0xC0);
				}

				bool storesActive = false;
				for (auto j = site + 5; j < site + 0x25; ++j) {
					const auto* next = reinterpret_cast<const std::uint8_t*>(j);
					if ((next[0] == 0x48 || next[0] == 0x4C) && next[1] == 0x89 && (next[2] & 0xC7) == 0x05) {
						const auto disp = *reinterpret_cast<const std::int32_t*>(next + 3);
						storesActive = storesActive || j + 7 + static_cast<std::intptr_t>(disp) == a_active;
					}
				}

				if (zeroed && storesActive) {
					if (found) {
						return std::nullopt;
					}
					found = site;
				}
			}
			return found;
		}

		// Mirrors the engine's accessor: climb while the worldspace borrows its parent's LOD.
		std::uintptr_t ResolveTerrainManager(std::uintptr_t a_worldSpace)
		{
			using namespace Offsets::WorldSpace;
			for (int hops = 0; a_worldSpace != 0 && hops < 16; ++hops) {
				const auto parent = Read<std::uintptr_t>(a_worldSpace, kParentWorld);
				if (parent == 0 ||
					(Read<std::uint16_t>(a_worldSpace, kParentUseFlags) & kUseLODData) == 0) {
					break;
				}
				a_worldSpace = parent;
			}
			return a_worldSpace != 0 ? Read<std::uintptr_t>(a_worldSpace, kTerrainManager) : 0;
		}

		// Cell index of a world coordinate; cells go negative, so this floors, not truncates.
		std::int32_t CellOf(float a_world)
		{
			const auto quotient = a_world / NodeGeometry::kCellSize;
			auto cell = static_cast<std::int32_t>(quotient);
			if (quotient < 0.0f && static_cast<float>(cell) != quotient) {
				--cell;
			}
			return cell;
		}

		// Cells inside this radius are fully loaded, so moving within it strands nothing.
		std::int32_t GridsToLoad()
		{
			static REL::Relocation<std::uint32_t*> slot{ REL::VariantID(
				Offsets::kCachedGridsToLoadSE, Offsets::kCachedGridsToLoad,
				Offsets::kCachedGridsToLoadVR) };
			auto* p = slot.get();
			const auto value = p ? *p : 0;
			return value != 0 ? static_cast<std::int32_t>(value) : 5;
		}
	}

	MapLODFix& MapLODFix::Get()
	{
		static MapLODFix singleton;
		return singleton;
	}

	void MapLODFix::Install()
	{
		if (!Settings::Get().enableLODReset) {
			return;
		}

		const REL::Relocation<std::uintptr_t> detach{ REL::VariantID(
			Offsets::kDetachManagerSE, Offsets::kDetachManager, Offsets::kDetachManagerVR) };
		const REL::Relocation<std::uintptr_t> active{ REL::VariantID(
			Offsets::kActiveTerrainManagerSE, Offsets::kActiveTerrainManager,
			Offsets::kActiveTerrainManagerVR) };

		const auto site = FindSwitchDetachCall(detach.address(), active.address());
		if (!site) {
			logger::warn("[lod] could not find the manager-switch Detach call in "
						 "BGSTerrainManager::Update; worldspace switches keep the engine's own reset");
			return;
		}

		SwitchDetach::original = SKSE::GetTrampoline().write_call<5>(*site, SwitchDetach::Thunk);
		logger::info("[lod] manager switches release the old manager completely (call at {:#x})",
			*site - REL::Module::get().base());
	}

	bool MapLODFix::Sample(Waypoint& a_out)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}
		auto* ws = player->GetWorldspace();
		if (!ws) {
			return false;
		}

		const auto manager = ResolveTerrainManager(reinterpret_cast<std::uintptr_t>(ws));
		if (manager == 0) {
			return false;
		}

		const auto pos = player->GetPosition();
		a_out.manager = manager;
		a_out.worldspace = ws->GetFormID();
		a_out.cellX = CellOf(pos.x);
		a_out.cellY = CellOf(pos.y);
		a_out.valid = true;
		return true;
	}

	void MapLODFix::RunDetach(const Waypoint& a_from, const Waypoint& a_to,
		std::int32_t a_movedCells, const char* a_reason)
	{
		const auto mgr = ActiveTerrainManager();
		if (mgr == 0) {
			return;
		}

		using namespace Offsets::TerrainManager;
		if (!Read<bool>(mgr, kHasLOD) || Read<std::uintptr_t>(mgr, kRootNode) == 0) {
			logger::info("[lod] {}: active manager {:#x} has no LOD quadtree; nothing to reset",
				a_reason, mgr);
			return;
		}

		DetachManager(mgr);

		// Redundant -- Detach already cleared it -- but it keeps Update's own two statements.
		*reinterpret_cast<char*>(mgr + kInitialised) = 0;

		logger::info("[lod] {}: worldspace {:#x} -> {:#x}, cell ({},{}) -> ({},{}), moved {} "
					 "cell(s) with the manager unchanged at {:#x}; ran Detach(mgr, 1, 1), because "
					 "Update's own reset cannot fire",
			a_reason, a_from.worldspace, a_to.worldspace, a_from.cellX, a_from.cellY, a_to.cellX,
			a_to.cellY, a_movedCells, mgr);
	}

	void MapLODFix::OnGameLoaded()
	{
		// Re-seed, so the first transition of a session compares against where we actually are.
		_departure = {};
		Waypoint here;
		if (Sample(here)) {
			_departure = here;
		}
	}

	void MapLODFix::OnLoadingScreenOpened()
	{
		if (!Settings::Get().enableLODReset) {
			return;
		}

		if (_departedManager != 0) {
			Diagnostics::LogManager("left-earlier", _departedManager);
			_departedManager = 0;
		}

		// Best estimate of where we are leaving from; interiors keep the previous waypoint.
		Waypoint here;
		if (Sample(here)) {
			_departure = here;
		}
	}

	void MapLODFix::OnLoadingScreenClosed()
	{
		if (!Settings::Get().enableLODReset) {
			return;
		}

		Waypoint arrival;
		if (!Sample(arrival)) {
			return;
		}

		const auto departure = _departure;
		_departure = arrival;

		if (!departure.valid) {
			// First exterior of the session: nothing stale to reset.
			return;
		}

		if (arrival.manager != departure.manager) {
			// The manager changed, so Update's switch resets it (completely, with Install's hook).
			logger::info("[lod] worldspace {:#x} -> {:#x}: manager {:#x} -> {:#x}, reset by "
						 "Update's manager switch",
				departure.worldspace, arrival.worldspace, departure.manager, arrival.manager);
			Diagnostics::LogManager("left", departure.manager);
			Diagnostics::LogManager("arrived", arrival.manager);
			_departedManager = departure.manager;
			return;
		}

		const auto dx = arrival.cellX - departure.cellX;
		const auto dy = arrival.cellY - departure.cellY;
		const auto spanX = dx < 0 ? -dx : dx;
		const auto spanY = dy < 0 ? -dy : dy;
		const auto moved = spanX > spanY ? spanX : spanY;

		const char* reason = nullptr;
		if (arrival.worldspace != departure.worldspace) {
			// Same manager, different cell grid -- every node holds the wrong worldspace's LOD.
			reason = "worldspace changed";
		} else if (moved > GridsToLoad()) {
			// The walk declines at the subtree we left, stranding everything loaded below it.
			reason = "teleport";
		}

		if (!reason) {
			if (Settings::Get().logDiagnostics) {
				logger::info("[lod] worldspace {:#x}, moved {} cell(s) within uGridsToLoad {}; "
							 "no reset needed",
					arrival.worldspace, moved, GridsToLoad());
			}
			return;
		}

		auto* task = SKSE::GetTaskInterface();
		if (!task) {
			return;
		}
		task->AddTask([this, departure, arrival, moved, reason] {
			RunDetach(departure, arrival, moved, reason);
		});
	}
}

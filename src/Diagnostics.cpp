#include "Diagnostics.h"

#include "MapLODFix.h"
#include "NodeGeometry.h"
#include "Offsets.h"
#include "Settings.h"

namespace LODFix::Diagnostics
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

		std::uint32_t LockedLevel(REL::VariantID a_id)
		{
			REL::Relocation<std::uint32_t*> slot{ a_id };
			auto* p = slot.get();
			return p ? *p : 0;
		}

		// Mirrors the engine's accessor: climb while the worldspace borrows its parent's LOD.
		std::uintptr_t ResolveTerrainManager(std::uintptr_t a_worldSpace)
		{
			using namespace Offsets::WorldSpace;
			for (int hops = 0; a_worldSpace != 0 && hops < 16; ++hops) {
				const auto parent = Read<std::uintptr_t>(a_worldSpace, kParentWorld);
				if (parent == 0 || (Read<std::uint16_t>(a_worldSpace, kParentUseFlags) &
									   kUseLODData) == 0) {
					break;
				}
				a_worldSpace = parent;
			}
			return a_worldSpace != 0 ? Read<std::uintptr_t>(a_worldSpace, kTerrainManager) : 0;
		}
	}

	void LogState(std::string_view a_reason)
	{
		if (!Settings::Get().logDiagnostics) {
			return;
		}

		const auto mgr = ActiveTerrainManager();
		if (mgr == 0) {
			logger::info("[lod:{}] no active terrain manager", a_reason);
			return;
		}

		using namespace Offsets::TerrainManager;
		const auto rootNode = Read<std::uintptr_t>(mgr, kRootNode);
		const auto poolBegin = Read<std::uintptr_t>(mgr, kNodePoolBegin);
		const auto poolEnd = Read<std::uintptr_t>(mgr, kNodePoolEnd);

		std::size_t nodes = 0;
		std::size_t pinned = 0;
		std::size_t drawn = 0;
		std::uint32_t pinnedSpan = 0;

		if (poolBegin != 0 && poolEnd > poolBegin) {
			for (auto node = poolBegin; node + Offsets::kNodeSize <= poolEnd;
				 node += Offsets::kNodeSize) {
				++nodes;
				const auto mapChunk =
					Read<std::uintptr_t>(node, Offsets::TerrainNode::kMapChunkHandle);
				const auto mapBlock =
					Read<std::uintptr_t>(node, Offsets::TerrainNode::kMapBlockHandle);
				if (mapChunk == 0 && mapBlock == 0) {
					continue;
				}
				++pinned;
				if (NodeGeometry::MapHandleDrawn(mapBlock)) {
					++drawn;
				}
				if (pinnedSpan == 0) {
					pinnedSpan = NodeGeometry::SpanOf(
						Read<std::uint32_t>(node, Offsets::TerrainNode::kNodeState));
				}
			}
		}

		const char* wsName = "?";
		std::uint32_t wsID = 0;
		std::uintptr_t ownMgr = 0;
		std::uintptr_t resolvedMgr = 0;
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			if (auto* ws = player->GetWorldspace()) {
				wsName = ws->GetFormEditorID();
				wsID = ws->GetFormID();
				const auto wsAddr = reinterpret_cast<std::uintptr_t>(ws);
				ownMgr = Read<std::uintptr_t>(wsAddr, Offsets::WorldSpace::kTerrainManager);
				resolvedMgr = ResolveTerrainManager(wsAddr);
			}
		}

		logger::info(
			"[lod:{}] worldspace={} ({:#x}) manager[active={:#x} own={:#x} resolved={:#x}] | "
			"hasLOD={} quadtree={} nodes={} levels[min={} max={} root={}] queue[size={} at={}] | "
			"locked[terrain={} object={}] | map handles held={} blocksDrawn={} (span {})",
			a_reason,
			wsName ? wsName : "?",
			wsID,
			mgr,
			ownMgr,
			resolvedMgr,
			Read<bool>(mgr, kHasLOD),
			rootNode != 0 ? "yes" : "NULL",
			nodes,
			Read<std::uint32_t>(mgr, kMinLevel),
			Read<std::uint32_t>(mgr, kMaxLevel),
			Read<std::uint32_t>(mgr, kRootLevel),
			Read<std::uint32_t>(mgr, kUpdateNodesSize),
			Read<std::uint32_t>(mgr, kNextUpdateNode),
			LockedLevel(REL::VariantID(
				Offsets::kLockedTerrainLODSE, Offsets::kLockedTerrainLOD,
				Offsets::kLockedTerrainLODVR)),
			LockedLevel(REL::VariantID(
				Offsets::kLockedObjectMapLODSE, Offsets::kLockedObjectMapLOD,
				Offsets::kLockedObjectMapLODVR)),
			pinned,
			drawn,
			pinnedSpan);

		if (rootNode == 0) {
			logger::warn(
				"Active worldspace has no terrain LOD quadtree. Its "
				"Data/LODSettings/<worldspace>.LOD is missing or unreadable, which disables "
				"terrain LOD for this worldspace completely.");
		}
	}

	void LogManager(std::string_view a_reason, std::uintptr_t a_manager)
	{
		if (!Settings::Get().logDiagnostics || a_manager == 0) {
			return;
		}

		using namespace Offsets::TerrainManager;
		using namespace Offsets::TerrainNode;
		constexpr std::array kHandles{ kChunkHandle, kBlockHandle, kTreeHandle, kMapChunkHandle,
			kMapBlockHandle };

		std::array<std::size_t, kHandles.size()> held{};
		std::size_t blocksDrawn = 0;
		std::size_t mapBlocksDrawn = 0;

		const auto poolBegin = Read<std::uintptr_t>(a_manager, kNodePoolBegin);
		const auto poolEnd = Read<std::uintptr_t>(a_manager, kNodePoolEnd);
		if (poolBegin != 0 && poolEnd > poolBegin) {
			for (auto node = poolBegin; node + Offsets::kNodeSize <= poolEnd;
				 node += Offsets::kNodeSize) {
				for (std::size_t i = 0; i < kHandles.size(); ++i) {
					if (Read<std::uintptr_t>(node, kHandles[i]) != 0) {
						++held[i];
					}
				}
				if (NodeGeometry::MapHandleDrawn(Read<std::uintptr_t>(node, kBlockHandle))) {
					++blocksDrawn;
				}
				if (NodeGeometry::MapHandleDrawn(Read<std::uintptr_t>(node, kMapBlockHandle))) {
					++mapBlocksDrawn;
				}
			}
		}

		const auto* ws = Read<RE::TESWorldSpace*>(a_manager, kWorldSpace);
		const auto* wsName = ws ? ws->GetFormEditorID() : nullptr;

		logger::info(
			"[lod:{}] manager {:#x} of {} ({}) initialised={} hasLOD={} | held chunk={} block={} "
			"tree={} mapChunk={} mapBlock={} | drawn block={} mapBlock={}",
			a_reason,
			a_manager,
			wsName && *wsName ? wsName : "?",
			a_manager == ActiveTerrainManager() ? "active" : "inactive",
			Read<bool>(a_manager, kInitialised),
			Read<bool>(a_manager, kHasLOD),
			held[0], held[1], held[2], held[3], held[4],
			blocksDrawn,
			mapBlocksDrawn);
	}
}

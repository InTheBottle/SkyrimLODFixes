#pragma once

namespace LODFix::Offsets
{
	/** BGSTerrainManager*. The manager the update path currently considers active. */
	inline constexpr std::uint64_t kActiveTerrainManager = 402262;

	inline constexpr std::uint64_t kDetachManager = 31812;

	inline constexpr std::uint64_t kLockedTerrainLOD = 402337;
	inline constexpr std::uint64_t kLockedObjectMapLOD = 402338;

	/** Cached `uGridsToLoad`, re-derived at the top of every terrain update. */
	inline constexpr std::uint64_t kCachedGridsToLoad = 402339;

	/** sizeof(BGSTerrainNode); the quadtree is one pooled allocation at this stride. */
	inline constexpr std::ptrdiff_t kNodeSize = 0x50;

	/** Offsets inside BGSTerrainManager (verified against CommonLibSSE + decompilation). */
	namespace TerrainManager
	{
		/** Non-zero while the manager is serving the world map rather than the world. */
		inline constexpr std::ptrdiff_t kMapMode = 0x00;
		inline constexpr std::ptrdiff_t kRootNode = 0x10;
		inline constexpr std::ptrdiff_t kMaxLevel = 0x1C;
		/** The descent floor: every LOD walk recurses while `span > minLevel`. */
		inline constexpr std::ptrdiff_t kMinLevel = 0x20;
		/** The span at which nodes are enrolled in the per-frame refresh queue. */
		inline constexpr std::ptrdiff_t kRootLevel = 0x24;
		inline constexpr std::ptrdiff_t kHasLOD = 0x36;
		inline constexpr std::ptrdiff_t kInitialised = 0x58;
		/** updateNodes is {data 0x60, capacity 0x68, size 0x70}; the cursor follows it. */
		inline constexpr std::ptrdiff_t kUpdateNodesSize = 0x70;
		inline constexpr std::ptrdiff_t kNextUpdateNode = 0x78;
		/** Base and end of the single pooled allocation holding every node. */
		inline constexpr std::ptrdiff_t kNodePoolBegin = 0xB0;
		inline constexpr std::ptrdiff_t kNodePoolEnd = 0xC0;
	}

	/** A worldspace resolves to its parent's manager while it borrows the parent's LOD. */
	namespace WorldSpace
	{
		inline constexpr std::ptrdiff_t kTerrainManager = 0x90;
		inline constexpr std::ptrdiff_t kParentUseFlags = 0xA2;
		inline constexpr std::ptrdiff_t kParentWorld = 0x158;

		inline constexpr std::uint16_t kUseLODData = 1 << 1;
	}

	namespace TerrainNode
	{
		inline constexpr std::ptrdiff_t kNodeState = 0x40;
		inline constexpr std::ptrdiff_t kBaseCellX = 0x48;
		inline constexpr std::ptrdiff_t kBaseCellY = 0x4A;

		/** The world map's own LOD instances -- the handles that leak. */
		inline constexpr std::ptrdiff_t kMapChunkHandle = 0x20;
		inline constexpr std::ptrdiff_t kMapBlockHandle = 0x28;
	}
}

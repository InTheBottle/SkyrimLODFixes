#pragma once

namespace LODFix::NodeGeometry
{
	inline constexpr float kCellSize = 4096.0f;

	[[nodiscard]] inline constexpr std::uint32_t SpanOf(std::uint32_t a_nodeState) noexcept
	{
		return (a_nodeState >> 21) & 0x3FCu;
	}

	[[nodiscard]] inline bool MapHandleDrawn(std::uintptr_t a_handle) noexcept
	{
		if (a_handle == 0) {
			return false;
		}
		const auto resource = *reinterpret_cast<std::uintptr_t*>(a_handle + 0x28);
		if (resource == 0) {
			return false;
		}
		const auto sceneNode = *reinterpret_cast<std::uintptr_t*>(resource + 0x08);
		if (sceneNode == 0) {
			return false;
		}
		return (*reinterpret_cast<std::uint32_t*>(sceneNode + 0xF4) & 1u) == 0;
	}
}

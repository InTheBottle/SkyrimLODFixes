#pragma once

namespace LODFix::InstanceGroupFix
{
	/**
	 * Makes BSMultiStreamInstanceTriShape::OnVisible hold the engine's instance-group lock
	 * while it walks its groups. The terrain and grass updates add and remove tree LOD and
	 * grass groups under that lock, but culling reads the same array without it, so a
	 * culling pass that overlaps an update can read a freed group array or group. Call once
	 * at plugin load.
	 */
	void Install();
}

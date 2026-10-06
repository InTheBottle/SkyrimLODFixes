#include "InstanceGroupFix.h"

#include "Settings.h"

#include <optional>
#include <thread>

namespace LODFix::InstanceGroupFix
{
	namespace
	{
		// BSMultiStreamInstanceTriShape vtable slots on SE/AE. VR has one extra virtual ahead
		// of them; every address derived from these is validated before anything is patched.
		constexpr std::size_t kOnVisibleSlot = 0x34;
		constexpr std::size_t kAddGroupSlot = 0x3C;
		constexpr std::size_t kRemoveGroupSlot = 0x3D;

		constexpr std::size_t kMaxScan = 0x400;

		// The engine's lock is a bare flag: `lock cmpxchg 0 -> 1`, Sleep(0) while busy, and a
		// plain store of 0 to release. It is not recursive.
		std::uint32_t*    g_lock = nullptr;
		thread_local bool t_held = false;

		void Acquire()
		{
			std::atomic_ref<std::uint32_t> lock(*g_lock);
			for (std::uint32_t expected = 0; !lock.compare_exchange_weak(expected, 1); expected = 0) {
				std::this_thread::yield();
			}
			t_held = true;
		}

		void Release()
		{
			if (t_held) {
				t_held = false;
				std::atomic_ref<std::uint32_t>(*g_lock).store(0);
			}
		}

		std::uintptr_t VFunc(std::uintptr_t a_vtbl, std::size_t a_slot)
		{
			return reinterpret_cast<const std::uintptr_t*>(a_vtbl)[a_slot];
		}

		// Up to the first `ret` followed by int3 padding, capped at kMaxScan.
		std::size_t FunctionLength(std::uintptr_t a_func)
		{
			const auto* code = reinterpret_cast<const std::uint8_t*>(a_func);
			for (std::size_t i = 0; i + 2 < kMaxScan; ++i) {
				if (code[i] == 0xC3 && code[i + 1] == 0xCC && code[i + 2] == 0xCC) {
					return i + 1;
				}
			}
			return kMaxScan;
		}

		// The first `lock cmpxchg dword ptr [rip + disp32], r32` in the function.
		std::uint32_t* FindLockWord(std::uintptr_t a_func)
		{
			const auto  length = FunctionLength(a_func);
			const auto* code = reinterpret_cast<const std::uint8_t*>(a_func);
			for (std::size_t i = 0; i + 8 <= length; ++i) {
				if (code[i] != 0xF0) {
					continue;
				}
				auto op = i + 1;
				if ((code[op] & 0xF0) == 0x40) {
					++op;
				}
				if (op + 7 > length || code[op] != 0x0F || code[op + 1] != 0xB1 || (code[op + 2] & 0xC7) != 0x05) {
					continue;
				}
				const auto disp = *reinterpret_cast<const std::int32_t*>(code + op + 3);
				return reinterpret_cast<std::uint32_t*>(a_func + op + 7 + static_cast<std::intptr_t>(disp));
			}
			return nullptr;
		}

		// The one `call rel32` to a_target in the function; nothing if absent or ambiguous.
		std::optional<std::uintptr_t> FindCall(std::uintptr_t a_func, std::uintptr_t a_target)
		{
			const auto length = FunctionLength(a_func);
			std::optional<std::uintptr_t> found;
			for (std::size_t i = 0; i + 5 <= length; ++i) {
				const auto site = a_func + i;
				if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
					continue;
				}
				const auto rel = *reinterpret_cast<const std::int32_t*>(site + 1);
				if (site + 5 + static_cast<std::intptr_t>(rel) != a_target) {
					continue;
				}
				if (found) {
					return std::nullopt;
				}
				found = site;
			}
			return found;
		}

		struct OnVisible
		{
			static void Thunk(void* a_this, void* a_process, std::int32_t a_alphaGroup)
			{
				Acquire();
				original(a_this, a_process, a_alphaGroup);
				Release();
			}

			static inline REL::Relocation<decltype(Thunk)> original;
		};

		// The base OnVisible only registers the shape with the accumulator. The groups have
		// been tested by then, so drop the lock rather than hold it across registration.
		struct BaseOnVisible
		{
			static void Thunk(void* a_this, void* a_process, std::int32_t a_alphaGroup)
			{
				Release();
				original(a_this, a_process, a_alphaGroup);
			}

			static inline REL::Relocation<decltype(Thunk)> original;
		};
	}

	void Install()
	{
		if (!Settings::Get().enableInstanceGroupLock) {
			return;
		}

		const auto base = REL::Module::get().base();
		const auto shift = REL::Module::IsVR() ? std::size_t{ 1 } : std::size_t{ 0 };

		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_BSMultiStreamInstanceTriShape[0] };
		REL::Relocation<std::uintptr_t> baseVtbl{ RE::VTABLE_BSInstanceTriShape[0] };

		auto* addLock = FindLockWord(VFunc(vtbl.address(), kAddGroupSlot + shift));
		auto* removeLock = FindLockWord(VFunc(vtbl.address(), kRemoveGroupSlot + shift));
		const auto lockAddress = reinterpret_cast<std::uintptr_t>(addLock);
		if (!addLock || addLock != removeLock || lockAddress % alignof(std::uint32_t) != 0) {
			logger::warn("[instance] AddGroup and RemoveGroup do not share a recognisable lock "
						 "({:#x} / {:#x}); culling lock not installed",
				lockAddress, reinterpret_cast<std::uintptr_t>(removeLock));
			return;
		}

		const auto onVisible = VFunc(vtbl.address(), kOnVisibleSlot + shift);
		const auto baseOnVisible = VFunc(baseVtbl.address(), kOnVisibleSlot + shift);
		const auto site = FindCall(onVisible, baseOnVisible);
		if (!site) {
			logger::warn("[instance] OnVisible at {:#x} has no single call to the base OnVisible "
						 "at {:#x} (already hooked by another plugin?); culling lock not installed",
				onVisible - base, baseOnVisible - base);
			return;
		}

		g_lock = addLock;
		SKSE::AllocTrampoline(1 << 6);
		BaseOnVisible::original = SKSE::GetTrampoline().write_call<5>(*site, BaseOnVisible::Thunk);
		OnVisible::original = vtbl.write_vfunc(kOnVisibleSlot + shift, OnVisible::Thunk);

		logger::info("[instance] culling lock installed: lock at {:#x}, released before the base "
					 "OnVisible call at {:#x}",
			lockAddress - base, *site - base);
	}
}

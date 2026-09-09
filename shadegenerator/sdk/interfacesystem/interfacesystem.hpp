#pragma once
#include <Windows.h>
#include <stdexcept>
#include <string_view>
#include <cstdint>

#include "utils/debug.hpp"

namespace CInterfaceSystem {
	#define RESOLVE_RIP_EX(type, addr, offset, size) reinterpret_cast<type*>(addr + *((int32_t*)(addr + offset)) + size)
	#define RESOLVE_RIP(type, addr) RESOLVE_RIP_EX(type, addr, 3, 7)

	template<typename T>
	[[nodiscard]] static T* Get(std::string_view module_name, std::string_view interface_name){
		HMODULE hModule = GetModuleHandleA(module_name.data());

		if (!hModule)
		{
			return nullptr;
		}

		uintptr_t create_interface = reinterpret_cast<uintptr_t>(GetProcAddress(hModule, "CreateInterface"));

		if (!create_interface)
		{
			return nullptr;
		}

		using interface_callback_fn = void* (__cdecl*)();

		typedef struct _interface_reg_t
		{
			interface_callback_fn callback;
			const char* name;
			_interface_reg_t* flink;
		} interface_reg_t;

		interface_reg_t* interface_list = *reinterpret_cast<interface_reg_t**>(RESOLVE_RIP(std::uintptr_t, create_interface));

		if (!interface_list) {
			return nullptr;
		}

		for (interface_reg_t* it = interface_list; it; it = it->flink)
		{
			if (!strcmp(it->name, interface_name.data()))
			{
				lg::Success("interface", "successfully found {} at {:p}", interface_name, it->callback());
				return reinterpret_cast<T*>(it->callback());
			}
		}

		throw std::runtime_error(std::format("failed to initialize {} in {}", interface_name, module_name));
	}
}
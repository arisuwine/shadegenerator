#pragma once
#include <chrono>
#include <format>
#include <print>
#include <string_view>
#include <Windows.h>

namespace lg {
	namespace colors {
		constexpr std::string_view red    = "\033[38;2;251;118;118m";
		constexpr std::string_view green  = "\033[38;2;145;251;118m";
		constexpr std::string_view yellow = "\033[33m";
		constexpr std::string_view bold   = "\033[1m";
		constexpr std::string_view reset  = "\033[0m";
		constexpr std::string_view blue   = "\033[38;2;129;149;249m";
		constexpr std::string_view orange = "\033[38;2;249;171;129m";
		constexpr std::string_view dim    = "\033[90m";
	} // namespace colors

	inline void PrintPrefix() {
		using namespace std::chrono;

		const auto tpNow   = system_clock::now();
		const auto tpLocal = current_zone()->to_local(tpNow);
		const auto tpMs    = floor<milliseconds>(tpLocal);

		std::print("{}[{:%H:%M:%S}]{} ", colors::dim, tpMs, colors::reset);
	}

	template <typename... Args>
	inline void Success(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
		PrintPrefix();
		std::print("[{}ok{}]", colors::green, colors::reset);
		if (!tag.empty())
			std::print("[{}{}{}{}] ", colors::bold, colors::green, tag, colors::reset);
		else
			std::print(" ");

		std::println(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void Info(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
		PrintPrefix();
		std::print("[{}info{}]", colors::blue, colors::reset);
		if (!tag.empty())
			std::print("[{}{}{}{}] ", colors::bold, colors::blue, tag, colors::reset);
		else
			std::print(" ");

		std::println(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void Error(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
		PrintPrefix();
		std::print("[{}error{}]", colors::red, colors::reset);
		if (!tag.empty())
			std::print("[{}{}{}{}] ", colors::bold, colors::red, tag, colors::reset);
		else
			std::print(" ");

		std::println(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void Warn(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
		PrintPrefix();
		std::print("[{}warning{}]", colors::orange, colors::reset);
		if (!tag.empty())
			std::print("[{}{}{}{}] ", colors::bold, colors::orange, tag, colors::reset);
		else
			std::print(" ");

		std::println(fmt, std::forward<Args>(args)...);
	}
} // namespace lg
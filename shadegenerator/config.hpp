#pragma once
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

namespace shade {
	/** @brief Selects the syntax emitted for the generated SDK. */
	enum EEmitType : std::uint8_t {
		CPP, ///< Portable C++ headers and a CMake interface target.
		IDA_C, ///< IDA-compatible C declarations without includes.
		IDA_CPP ///< IDA-compatible C++ declarations without includes.
	};

	/** @brief Selects how generated declarations are distributed across files. */
	enum class ESplitMode : std::uint8_t {
		PER_FILE, ///< One file per schema type.
		MODULE, ///< One file per schema module.
		DUAL, ///< Both per-type and per-module files in one tree.
		SINGLE ///< One file for the whole SDK.
	};

	/** @brief Returns the split mode used when none is requested explicitly. */
	[[nodiscard]] constexpr ESplitMode GetDefaultSplitMode(EEmitType type) noexcept {
		return type == EEmitType::CPP ? ESplitMode::PER_FILE : ESplitMode::SINGLE;
	}

	/** @brief Controls the output format and destination of one generation run. */
	struct GeneratorConfig_t {
		EEmitType  m_eEmitType; ///< Syntax to emit.
		ESplitMode m_eSplitMode = ESplitMode::PER_FILE; ///< File layout of the generated SDK.
		fs::path   m_OutputPath; ///< Directory in which the generated SDK is created.
	};

} // namespace shade

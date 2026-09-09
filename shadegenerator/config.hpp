#pragma once
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

namespace shade {
	/** @brief Selects the syntax emitted for the generated SDK. */
	enum EEmitType : std::uint8_t {
		CPP, ///< Portable C++ headers and a CMake interface target.
		IDA_C, ///< A single IDA-compatible C declaration file.
		IDA_CPP ///< A single IDA-compatible C++ declaration file.
	};

	/** @brief Controls the output format and destination of one generation run. */
	struct GeneratorConfig_t {
		EEmitType m_eEmitType; ///< Syntax to emit.
		fs::path  m_OutputPath; ///< Directory in which the generated SDK is created.
	};

} // namespace shade

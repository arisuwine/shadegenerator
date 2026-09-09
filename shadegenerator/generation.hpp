#pragma once
#include "config.hpp"

namespace shade {
	/**
	 * @brief Collects the loaded Source 2 schema and emits an SDK.
	 * @param config Output format and destination settings.
	 * @throws std::runtime_error if the schema system is unavailable or the emit type is unsupported.
	 */
	void GenerateSdk(shade::GeneratorConfig_t& config);
} // namespace shade

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace shade::schema {
	using SchemaDependencyMap_t = std::unordered_map<std::string, std::unordered_set<std::string>>;

	struct SchemaDependencies_t {
		SchemaDependencyMap_t m_Definitions;
		SchemaDependencyMap_t m_Declarations;
	};
} // namespace shade::schema

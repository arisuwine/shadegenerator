#pragma once
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace shade::schema {
	enum class ESchemaAtomicType : std::uint8_t {
		PLAIN,
		T,
		COLLECTION_OF_T,
		TT,
		I,
		INVALID
	};

	struct SchemaAtomicSpecializationRecord_t {
		SchemaAtomicParameters_t      m_Parameters;
		std::optional<SchemaLayout_t> m_LayoutOverride;
	};

	struct SchemaAtomicRecord_t {
		std::string       m_szName;
		int               m_nId   = -1;
		ESchemaAtomicType m_eType = ESchemaAtomicType::INVALID;

		std::optional<SchemaLayout_t>                   m_DefaultLayout;
		std::vector<SchemaAtomicSpecializationRecord_t> m_Specializations;
	};
} // namespace shade::schema

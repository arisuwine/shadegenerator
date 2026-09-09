#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace shade::schema {
	struct SchemaEnumValueRecord_t {
		std::string  m_szName;
		std::int64_t m_nValue = 0;
	};

	struct SchemaEnumRecord_t {
		SchemaTypeName_t m_Name;
		SchemaLayout_t   m_Layout;
		SchemaTypeRef_t  m_UnderlyingType = kInvalidSchemaTypeRef;

		std::vector<SchemaEnumValueRecord_t> m_Values;
	};
} // namespace shade::schema

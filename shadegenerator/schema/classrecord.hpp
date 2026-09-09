#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace shade::schema {
	// The fixed-width type mirrors the engine flag representation.
	enum ESchemaClassFlags : std::uint32_t { // NOLINT(performance-enum-size)
		SCHEMA_CLASS_NONE,
		SCHEMA_CLASS_HAS_VTABLE              = 1u << 0,
		SCHEMA_CLASS_IS_ABSTRACT             = 1u << 1,
		SCHEMA_CLASS_HAS_TRIVIAL_CONSTRUCTOR = 1u << 2,
		SCHEMA_CLASS_HAS_TRIVIAL_DESTRUCTOR  = 1u << 3,
		SCHEMA_CLASS_GLOBAL_TYPE_SCOPE       = 1u << 4,
		SCHEMA_CLASS_MODULE_LOCAL_TYPE_SCOPE = 1u << 5,
		SCHEMA_CLASS_CONSTRUCT_ALLOWED       = 1u << 6,
		SCHEMA_CLASS_CONSTRUCT_DISALLOWED    = 1u << 7
	};

	enum class ESchemaClassType : std::uint8_t {
		CLASS,
		STRUCT
	};

	struct SchemaBaseClassRecord_t {
		SchemaTypeName_t m_Name;
		std::size_t      m_nOffset = 0;
		std::size_t      m_nSize   = 0;
	};

	struct SchemaClassFieldRecord_t {
		SchemaTypeRef_t m_Type = kInvalidSchemaTypeRef;
		std::string     m_szName;
		std::size_t     m_nOffset   = 0;
		std::size_t     m_nSize     = 0;
		std::size_t     m_nBitWidth = 0;
	};

	struct SchemaDataMapFieldRecord_t {
		SchemaTypeRef_t m_Type = kInvalidSchemaTypeRef;
		std::string     m_szName;
		std::size_t     m_nOffset      = 0;
		std::size_t     m_nSize        = 0;
		std::size_t     m_nSizeInBytes = 0;
	};

	struct SchemaClassRecord_t {
		SchemaTypeName_t m_Name;
		SchemaLayout_t   m_Layout;
		ESchemaClassType m_eType  = ESchemaClassType::CLASS;
		std::uint32_t    m_eFlags = ESchemaClassFlags::SCHEMA_CLASS_NONE;

		std::vector<SchemaBaseClassRecord_t>    m_BaseClasses;
		std::vector<SchemaClassFieldRecord_t>   m_Fields;
		std::vector<SchemaDataMapFieldRecord_t> m_DataMapFields;
	};
} // namespace shade::schema

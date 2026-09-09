#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace shade::schema {
	using SchemaTypeRef_t = std::uint32_t;

	/// @brief Sentinel for an unresolved schema type reference.
	inline constexpr SchemaTypeRef_t kInvalidSchemaTypeRef = std::numeric_limits<SchemaTypeRef_t>::max();

	struct SchemaTypeName_t {
		std::string m_szModule;
		std::string m_szName;

		bool operator==(const SchemaTypeName_t&) const = default;
	};

	struct SchemaLayout_t {
		std::size_t m_nSize      = 0;
		std::size_t m_nAlignment = 0;

		bool operator==(const SchemaLayout_t&) const = default;
	};

	struct SchemaAtomicTypeParameter_t {
		SchemaTypeRef_t m_Type = kInvalidSchemaTypeRef;

		bool operator==(const SchemaAtomicTypeParameter_t&) const = default;
	};

	struct SchemaAtomicCollectionParameters_t {
		SchemaTypeRef_t m_ElementType       = kInvalidSchemaTypeRef;
		std::size_t     m_nElementSize      = 0;
		std::size_t     m_nFixedBufferCount = 0;

		bool operator==(const SchemaAtomicCollectionParameters_t&) const = default;
	};

	struct SchemaAtomicTwoTypeParameters_t {
		SchemaTypeRef_t m_FirstType  = kInvalidSchemaTypeRef;
		SchemaTypeRef_t m_SecondType = kInvalidSchemaTypeRef;

		bool operator==(const SchemaAtomicTwoTypeParameters_t&) const = default;
	};

	struct SchemaAtomicIntegerParameter_t {
		std::int64_t m_nValue = 0;

		bool operator==(const SchemaAtomicIntegerParameter_t&) const = default;
	};

	using SchemaAtomicParameters_t = std::variant<std::monostate, SchemaAtomicTypeParameter_t, SchemaAtomicCollectionParameters_t,
	                                              SchemaAtomicTwoTypeParameters_t, SchemaAtomicIntegerParameter_t>;

	enum class ESchemaBuiltinType : std::uint8_t {
		VOID,
		BOOL,
		CHAR,
		INT8,
		UINT8,
		INT16,
		UINT16,
		INT32,
		UINT32,
		INT64,
		UINT64,
		FLOAT32,
		FLOAT64
	};

	struct BuiltinType_t {
		ESchemaBuiltinType m_eType = ESchemaBuiltinType::VOID;
	};

	struct DeclaredClassType_t {
		SchemaTypeName_t m_Name;
	};

	struct DeclaredEnumType_t {
		SchemaTypeName_t m_Name;
	};

	struct PointerType_t {
		SchemaTypeRef_t m_PointeeType = kInvalidSchemaTypeRef;
	};

	struct FixedArrayType_t {
		SchemaTypeRef_t m_ElementType = kInvalidSchemaTypeRef;
		std::size_t     m_nCount      = 0;
	};

	struct AtomicType_t {
		std::string              m_szName;
		SchemaAtomicParameters_t m_Parameters;
	};

	struct InvalidType_t {
		std::string m_szName;
	};

	using SchemaType_t =
	    std::variant<BuiltinType_t, DeclaredClassType_t, DeclaredEnumType_t, PointerType_t, FixedArrayType_t, AtomicType_t, InvalidType_t>;
} // namespace shade::schema

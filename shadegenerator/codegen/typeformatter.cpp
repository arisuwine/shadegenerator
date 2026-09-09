#include "typeformatter.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace {
	using namespace shade;

	enum class ETypeFormat : std::uint8_t {
		CPP,
		IDA
	};

	using ActiveTypeRefs_t = std::unordered_set<schema::SchemaTypeRef_t>;

	[[nodiscard]] std::string FormatType(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ETypeFormat format,
	                                     ActiveTypeRefs_t& activeTypes);

	[[nodiscard]] std::optional<std::size_t> GetGeneratedCppSize(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type,
	                                                             ActiveTypeRefs_t& activeTypes);

	[[nodiscard]] bool IsInvalidByValue(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ActiveTypeRefs_t& activeTypes);

	/**
	 * @brief Checks whether atomic parameters contain a type that cannot be represented by value.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param parameters A `const schema::SchemaAtomicParameters_t&`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `bool`.
	 */
	[[nodiscard]] bool IsAtomicInvalidByValue(const schema::CSchemaModel& model, const schema::SchemaAtomicParameters_t& parameters,
	                                          ActiveTypeRefs_t& activeTypes) {
		return std::visit(
		    [&model, &activeTypes](const auto& value) {
			    using Value_t = std::remove_cvref_t<decltype(value)>;
			    if constexpr (std::same_as<Value_t, std::monostate> || std::same_as<Value_t, schema::SchemaAtomicIntegerParameter_t>) {
				    return false;
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicTypeParameter_t>) {
				    return IsInvalidByValue(model, value.m_Type, activeTypes);
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicCollectionParameters_t>) {
				    return IsInvalidByValue(model, value.m_ElementType, activeTypes);
			    } else {
				    return IsInvalidByValue(model, value.m_FirstType, activeTypes) || IsInvalidByValue(model, value.m_SecondType, activeTypes);
			    }
		    },
		    parameters);
	}

	/**
	 * @brief Checks whether a schema type is invalid when embedded by value and detects recursive value dependencies.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `bool`.
	 */
	[[nodiscard]] bool IsInvalidByValue(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ActiveTypeRefs_t& activeTypes) {
		if (!activeTypes.emplace(type).second)
			return true;

		const auto& schemaType = model.GetType(type);
		const bool  invalid    = [&] {
			if (std::holds_alternative<schema::InvalidType_t>(schemaType))
				return true;

			if (std::holds_alternative<schema::PointerType_t>(schemaType))
				return false;

			if (const auto* array = std::get_if<schema::FixedArrayType_t>(&schemaType))
				return array->m_nCount == 0 || IsInvalidByValue(model, array->m_ElementType, activeTypes);

			if (const auto* atomic = std::get_if<schema::AtomicType_t>(&schemaType))
				return IsAtomicInvalidByValue(model, atomic->m_Parameters, activeTypes);

			return false;
		}();
		activeTypes.erase(type);
		return invalid;
	}

	/**
	 * @brief Checks whether a schema type is invalid when embedded by value.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @return A `bool`.
	 */
	[[nodiscard]] bool IsInvalidByValue(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type) {
		ActiveTypeRefs_t activeTypes;
		return IsInvalidByValue(model, type, activeTypes);
	}

	/**
	 * @brief Checks whether a schema type can be used as an enum underlying type.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @return A `bool`.
	 */
	[[nodiscard]] bool IsValidEnumUnderlyingType(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type) {
		const auto* builtin = std::get_if<schema::BuiltinType_t>(&model.GetType(type));
		if (!builtin)
			return false;

		switch (builtin->m_eType) {
		case schema::ESchemaBuiltinType::BOOL:
		case schema::ESchemaBuiltinType::CHAR:
		case schema::ESchemaBuiltinType::INT8:
		case schema::ESchemaBuiltinType::UINT8:
		case schema::ESchemaBuiltinType::INT16:
		case schema::ESchemaBuiltinType::UINT16:
		case schema::ESchemaBuiltinType::INT32:
		case schema::ESchemaBuiltinType::UINT32:
		case schema::ESchemaBuiltinType::INT64:
		case schema::ESchemaBuiltinType::UINT64:
			return true;
		case schema::ESchemaBuiltinType::VOID:
		case schema::ESchemaBuiltinType::FLOAT32:
		case schema::ESchemaBuiltinType::FLOAT64:
			return false;
		}
		return false;
	}

	/**
	 * @brief Formats a schema builtin type for the selected output format.
	 *
	 * @param type A `schema::ESchemaBuiltinType`.
	 * @param format An `ETypeFormat`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatBuiltin(schema::ESchemaBuiltinType type, ETypeFormat format) {
		const bool cpp = format == ETypeFormat::CPP;
		switch (type) {
		case schema::ESchemaBuiltinType::VOID:
			return "void";
		case schema::ESchemaBuiltinType::BOOL:
			return "bool";
		case schema::ESchemaBuiltinType::CHAR:
			return "char";
		case schema::ESchemaBuiltinType::INT8:
			return cpp ? "std::int8_t" : "int8_t";
		case schema::ESchemaBuiltinType::UINT8:
			return cpp ? "std::uint8_t" : "uint8_t";
		case schema::ESchemaBuiltinType::INT16:
			return cpp ? "std::int16_t" : "int16_t";
		case schema::ESchemaBuiltinType::UINT16:
			return cpp ? "std::uint16_t" : "uint16_t";
		case schema::ESchemaBuiltinType::INT32:
			return cpp ? "std::int32_t" : "int32_t";
		case schema::ESchemaBuiltinType::UINT32:
			return cpp ? "std::uint32_t" : "uint32_t";
		case schema::ESchemaBuiltinType::INT64:
			return cpp ? "std::int64_t" : "int64_t";
		case schema::ESchemaBuiltinType::UINT64:
			return cpp ? "std::uint64_t" : "uint64_t";
		case schema::ESchemaBuiltinType::FLOAT32:
			return "float";
		case schema::ESchemaBuiltinType::FLOAT64:
			return "double";
		}

		return "void";
	}

	/**
	 * @brief Checks whether a byte is an ASCII letter or decimal digit.
	 *
	 * @param value An `unsigned char`.
	 * @return A `bool`.
	 */
	[[nodiscard]] bool IsAsciiAlphaNumeric(unsigned char value) {
		return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
	}

	/**
	 * @brief Formats a fully qualified schema type name for C++ output.
	 *
	 * @param name A `const schema::SchemaTypeName_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatCppName(const schema::SchemaTypeName_t& name) {
		return std::format("shade::sdk::{}::{}", name.m_szModule, name.m_szName);
	}

	/**
	 * @brief Formats a schema type name as a flattened IDA identifier.
	 *
	 * @param name A `const schema::SchemaTypeName_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatIdaName(const schema::SchemaTypeName_t& name) {
		return std::string(std::format("sdk_{}_{}", name.m_szModule, name.m_szName));
	}

	/**
	 * @brief Replaces unsupported characters and ensures that a value is a valid IDA identifier.
	 *
	 * @param value A `std::string_view`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string SanitizeIdaIdentifier(std::string_view value) {
		std::string result;
		result.reserve(value.size());
		for (const unsigned char character : value)
			result.push_back(IsAsciiAlphaNumeric(character) || character == '_' ? static_cast<char>(character) : '_');

		if (result.empty())
			return "_";
		if (result.front() >= '0' && result.front() <= '9')
			result.insert(result.begin(), '_');
		return result;
	}

	/**
	 * @brief Formats a readable IDA identifier from optional module and local type names.
	 *
	 * @param name A `const schema::SchemaTypeName_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatReadableIdaName(const schema::SchemaTypeName_t& name) {
		if (name.m_szModule.empty())
			return SanitizeIdaIdentifier(name.m_szName);
		return SanitizeIdaIdentifier(name.m_szModule) + '_' + SanitizeIdaIdentifier(name.m_szName);
	}

	[[nodiscard]] std::string FormatIdaAtomicType(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ActiveTypeRefs_t& activeTypes);

	/**
	 * @brief Encodes an atomic specialization as a valid IDA identifier.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param atomicName A `std::string_view`.
	 * @param parameters A `const schema::SchemaAtomicParameters_t&`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatIdaAtomic(const schema::CSchemaModel& model, std::string_view atomicName,
	                                          const schema::SchemaAtomicParameters_t& parameters, ActiveTypeRefs_t& activeTypes) {
		std::string name = SanitizeIdaIdentifier(atomicName);
		return std::visit(
		    [&model, &name, &activeTypes](const auto& value) {
			    using Value_t = std::remove_cvref_t<decltype(value)>;
			    if constexpr (std::same_as<Value_t, std::monostate>) {
				    return name;
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicTypeParameter_t>) {
				    return name + '_' + FormatIdaAtomicType(model, value.m_Type, activeTypes);
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicCollectionParameters_t>) {
				    std::string result = name + '_' + FormatIdaAtomicType(model, value.m_ElementType, activeTypes);
				    if (value.m_nFixedBufferCount != 0)
					    result += '_' + std::to_string(value.m_nFixedBufferCount);
				    return result;
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicTwoTypeParameters_t>) {
				    return name + '_' + FormatIdaAtomicType(model, value.m_FirstType, activeTypes) + '_' +
				           FormatIdaAtomicType(model, value.m_SecondType, activeTypes);
			    } else {
				    if (value.m_nValue < 0) {
					    // Compute the magnitude without negating INT64_MIN.
					    const auto magnitude = static_cast<std::uint64_t>(-(value.m_nValue + 1)) + 1;
					    return name + "_minus_" + std::to_string(magnitude);
				    }
				    return name + '_' + std::to_string(value.m_nValue);
			    }
		    },
		    parameters);
	}

	/**
	 * @brief Encodes a schema type for use as part of an IDA atomic identifier.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatIdaAtomicType(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ActiveTypeRefs_t& activeTypes) {
		if (!activeTypes.emplace(type).second)
			return "void";

		const auto&       schemaType = model.GetType(type);
		const std::string encoded    = [&] {
			if (const auto* builtin = std::get_if<schema::BuiltinType_t>(&schemaType))
				return FormatBuiltin(builtin->m_eType, ETypeFormat::IDA);
			if (const auto* declared = std::get_if<schema::DeclaredClassType_t>(&schemaType))
				return FormatReadableIdaName(declared->m_Name);
			if (const auto* declared = std::get_if<schema::DeclaredEnumType_t>(&schemaType))
				return FormatReadableIdaName(declared->m_Name);
			if (const auto* pointer = std::get_if<schema::PointerType_t>(&schemaType)) {
				if (IsInvalidByValue(model, pointer->m_PointeeType, activeTypes))
					return std::string("void_ptr");
				return FormatIdaAtomicType(model, pointer->m_PointeeType, activeTypes) + "_ptr";
			}
			if (const auto* array = std::get_if<schema::FixedArrayType_t>(&schemaType)) {
				return FormatIdaAtomicType(model, array->m_ElementType, activeTypes) + "_array_" + std::to_string(array->m_nCount);
			}
			if (const auto* atomic = std::get_if<schema::AtomicType_t>(&schemaType))
				return FormatIdaAtomic(model, atomic->m_szName, atomic->m_Parameters, activeTypes);
			return std::string("void");
		}();
		activeTypes.erase(type);
		return encoded;
	}

	/**
	 * @brief Formats an atomic type and its parameters as a C++ type expression.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param atomic A `const schema::AtomicType_t&`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatCppAtomic(const schema::CSchemaModel& model, const schema::AtomicType_t& atomic, ActiveTypeRefs_t& activeTypes) {
		return std::visit(
		    [&model, &atomic, &activeTypes](const auto& value) {
			    using Value_t = std::remove_cvref_t<decltype(value)>;
			    if constexpr (std::same_as<Value_t, std::monostate>) {
				    return atomic.m_szName;
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicTypeParameter_t>) {
				    return atomic.m_szName + '<' + FormatType(model, value.m_Type, ETypeFormat::CPP, activeTypes) + '>';
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicCollectionParameters_t>) {
				    std::string result = atomic.m_szName + '<' + FormatType(model, value.m_ElementType, ETypeFormat::CPP, activeTypes);
				    if (value.m_nFixedBufferCount != 0)
					    result += ", " + std::to_string(value.m_nFixedBufferCount);
				    result += '>';
				    return result;
			    } else if constexpr (std::same_as<Value_t, schema::SchemaAtomicTwoTypeParameters_t>) {
				    return atomic.m_szName + '<' + FormatType(model, value.m_FirstType, ETypeFormat::CPP, activeTypes) + ", " +
				           FormatType(model, value.m_SecondType, ETypeFormat::CPP, activeTypes) + '>';
			    } else {
				    return atomic.m_szName + '<' + std::to_string(value.m_nValue) + '>';
			    }
		    },
		    atomic.m_Parameters);
	}

	/**
	 * @brief Joins a base type with its accumulated declarator using C-style spacing rules.
	 *
	 * @param base A `std::string`.
	 * @param declarator A `std::string`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FinishDeclaration(std::string base, std::string declarator) {
		if (declarator.empty())
			return base;

		bool pointerOnly = true;
		for (const char character : declarator)
			pointerOnly = pointerOnly && character == '*';
		if (pointerOnly || declarator.front() == '[')
			return base + declarator;
		return base + ' ' + declarator;
	}

	/**
	 * @brief Recursively builds a declaration while preserving pointer and array binding rules.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @param declarator A `std::string`.
	 * @param format An `ETypeFormat`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatDeclarator(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, std::string declarator, ETypeFormat format,
	                                           ActiveTypeRefs_t& activeTypes) {
		if (!activeTypes.emplace(type).second)
			return FinishDeclaration("void", std::move(declarator));

		const auto&       schemaType  = model.GetType(type);
		const std::string declaration = [&] {
			if (const auto* pointer = std::get_if<schema::PointerType_t>(&schemaType)) {
				if (IsInvalidByValue(model, pointer->m_PointeeType, activeTypes)) {
					// Preserve a representable pointer even when its pointee cannot be named safely.
					return FinishDeclaration("void", '*' + declarator);
				}

				std::string pointerDeclarator = '*' + declarator;
				// Parentheses distinguish a pointer to an array from an array of pointers.
				if (std::holds_alternative<schema::FixedArrayType_t>(model.GetType(pointer->m_PointeeType)))
					pointerDeclarator = '(' + pointerDeclarator + ')';
				return FormatDeclarator(model, pointer->m_PointeeType, std::move(pointerDeclarator), format, activeTypes);
			}

			if (const auto* array = std::get_if<schema::FixedArrayType_t>(&schemaType)) {
				declarator += format == ETypeFormat::CPP ? std::format("[0x{:x}]", array->m_nCount) : std::format("[{}]", array->m_nCount);
				return FormatDeclarator(model, array->m_ElementType, std::move(declarator), format, activeTypes);
			}

			if (const auto* builtin = std::get_if<schema::BuiltinType_t>(&schemaType))
				return FinishDeclaration(FormatBuiltin(builtin->m_eType, format), std::move(declarator));
			if (const auto* declared = std::get_if<schema::DeclaredClassType_t>(&schemaType)) {
				const std::string name = format == ETypeFormat::CPP ? FormatCppName(declared->m_Name) : FormatIdaName(declared->m_Name);
				return FinishDeclaration(name, std::move(declarator));
			}
			if (const auto* declared = std::get_if<schema::DeclaredEnumType_t>(&schemaType)) {
				const std::string name = format == ETypeFormat::CPP ? FormatCppName(declared->m_Name) : FormatIdaName(declared->m_Name);
				return FinishDeclaration(name, std::move(declarator));
			}
			if (const auto* atomic = std::get_if<schema::AtomicType_t>(&schemaType)) {
				const std::string name = format == ETypeFormat::CPP ? FormatCppAtomic(model, *atomic, activeTypes) :
				                                                      FormatIdaAtomic(model, atomic->m_szName, atomic->m_Parameters, activeTypes);
				return FinishDeclaration(name, std::move(declarator));
			}
			return FinishDeclaration("void", std::move(declarator));
		}();
		activeTypes.erase(type);
		return declaration;
	}

	/**
	 * @brief Formats a validated schema type for the selected output format.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @param format An `ETypeFormat`.
	 * @param activeTypes An `ActiveTypeRefs_t&`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatType(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, ETypeFormat format,
	                                     ActiveTypeRefs_t& activeTypes) {
		if (IsInvalidByValue(model, type))
			return "void";
		return FormatDeclarator(model, type, {}, format, activeTypes);
	}

	[[nodiscard]] std::optional<std::size_t> GetBuiltinCppSize(schema::ESchemaBuiltinType type) {
		switch (type) {
		case schema::ESchemaBuiltinType::BOOL:
		case schema::ESchemaBuiltinType::CHAR:
		case schema::ESchemaBuiltinType::INT8:
		case schema::ESchemaBuiltinType::UINT8:
			return 1;
		case schema::ESchemaBuiltinType::INT16:
		case schema::ESchemaBuiltinType::UINT16:
			return 2;
		case schema::ESchemaBuiltinType::INT32:
		case schema::ESchemaBuiltinType::UINT32:
		case schema::ESchemaBuiltinType::FLOAT32:
			return 4;
		case schema::ESchemaBuiltinType::INT64:
		case schema::ESchemaBuiltinType::UINT64:
		case schema::ESchemaBuiltinType::FLOAT64:
			return 8;
		case schema::ESchemaBuiltinType::VOID:
			return std::nullopt;
		}

		return std::nullopt;
	}

	[[nodiscard]] std::optional<std::size_t> GetGeneratedCppSize(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type,
	                                                             ActiveTypeRefs_t& activeTypes) {
		if (!activeTypes.emplace(type).second)
			return std::nullopt;

		const auto& schemaType = model.GetType(type);
		const auto  size       = [&]() -> std::optional<std::size_t> {
			if (const auto* builtin = std::get_if<schema::BuiltinType_t>(&schemaType))
				return GetBuiltinCppSize(builtin->m_eType);

			if (std::holds_alternative<schema::PointerType_t>(schemaType))
				return 8;

			if (const auto* array = std::get_if<schema::FixedArrayType_t>(&schemaType)) {
				const auto elementSize = GetGeneratedCppSize(model, array->m_ElementType, activeTypes);
				if (!elementSize || *elementSize == 0 || array->m_nCount > (std::numeric_limits<std::size_t>::max)() / *elementSize)
					return std::nullopt;
				return *elementSize * array->m_nCount;
			}

			if (const auto* declared = std::get_if<schema::DeclaredClassType_t>(&schemaType)) {
				for (const auto& record : model.GetClasses()) {
					if (record.m_Name == declared->m_Name && record.m_Layout.m_nSize != 0)
						return record.m_Layout.m_nSize;
				}
				return std::nullopt;
			}

			if (const auto* declared = std::get_if<schema::DeclaredEnumType_t>(&schemaType)) {
				for (const auto& record : model.GetEnums()) {
					if (record.m_Name == declared->m_Name)
						return GetGeneratedCppSize(model, record.m_UnderlyingType, activeTypes);
				}
				return std::nullopt;
			}

			if (const auto* atomic = std::get_if<schema::AtomicType_t>(&schemaType)) {
				for (const auto& record : model.GetAtomics()) {
					if (record.m_szName != atomic->m_szName || !record.m_DefaultLayout)
						continue;

					for (const auto& specialization : record.m_Specializations) {
						if (specialization.m_Parameters == atomic->m_Parameters && specialization.m_LayoutOverride)
							return std::max<std::size_t>(specialization.m_LayoutOverride->m_nSize, 1);
					}
					return std::max<std::size_t>(record.m_DefaultLayout->m_nSize, 1);
				}
			}

			return std::nullopt;
		}();

		activeTypes.erase(type);
		return size;
	}

	/**
	 * @brief Formats storage for an invalid field using its known byte size.
	 *
	 * @param byteType A `std::string_view`.
	 * @param name A `std::string_view`.
	 * @param size A `std::size_t`.
	 * @param format An `ETypeFormat`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatFallback(std::string_view byteType, std::string_view name, std::size_t size, ETypeFormat format) {
		if (size == 0)
			return name.empty() ? "struct {}" : "struct {} " + std::string(name);

		std::string result(byteType);
		if (!name.empty()) {
			result += ' ';
			result += name;
		}
		result += format == ETypeFormat::CPP ? std::format("[0x{:x}]", size) : std::format("[{}]", size);
		return result;
	}

	/**
	 * @brief Formats a complete declaration and substitutes invalid by-value types with fallback storage.
	 *
	 * @param model A `const schema::CSchemaModel&`.
	 * @param type A `schema::SchemaTypeRef_t`.
	 * @param name A `std::string_view`.
	 * @param size A `std::size_t`.
	 * @param format An `ETypeFormat`.
	 * @return A `std::string`.
	 */
	[[nodiscard]] std::string FormatDeclaration(const schema::CSchemaModel& model, schema::SchemaTypeRef_t type, std::string_view name, std::size_t size,
	                                            ETypeFormat format) {
		if (IsInvalidByValue(model, type)) {
			const std::string_view byteType = format == ETypeFormat::CPP ? "std::uint8_t" : "uint8_t";
			return FormatFallback(byteType, name, size, format);
		}
		ActiveTypeRefs_t activeTypes;
		return FormatDeclarator(model, type, std::string(name), format, activeTypes);
	}

} // namespace

shade::codegen::CCppTypeFormatter::CCppTypeFormatter(const schema::CSchemaModel& model) noexcept : m_Model(model) {}

std::string shade::codegen::CCppTypeFormatter::FormatName(const schema::SchemaTypeName_t& name) const {
	return FormatCppName(name);
}

bool shade::codegen::CCppTypeFormatter::IsAtomicInvalidByValue(const schema::SchemaAtomicParameters_t& parameters) const {
	ActiveTypeRefs_t activeTypes;
	return ::IsAtomicInvalidByValue(m_Model, parameters, activeTypes);
}

bool shade::codegen::CCppTypeFormatter::IsValidEnumUnderlyingType(schema::SchemaTypeRef_t type) const {
	return ::IsValidEnumUnderlyingType(m_Model, type);
}

std::string shade::codegen::CCppTypeFormatter::FormatType(schema::SchemaTypeRef_t type) const {
	ActiveTypeRefs_t activeTypes;
	return ::FormatType(m_Model, type, ETypeFormat::CPP, activeTypes);
}

std::optional<std::size_t> shade::codegen::CCppTypeFormatter::GetGeneratedSize(schema::SchemaTypeRef_t type) const {
	if (IsInvalidByValue(m_Model, type))
		return std::nullopt;

	ActiveTypeRefs_t activeTypes;
	return GetGeneratedCppSize(m_Model, type, activeTypes);
}

std::string shade::codegen::CCppTypeFormatter::FormatDeclaration(schema::SchemaTypeRef_t type, std::string_view name, std::size_t size) const {
	return ::FormatDeclaration(m_Model, type, name, size, ETypeFormat::CPP);
}

shade::codegen::CIdaTypeFormatter::CIdaTypeFormatter(const schema::CSchemaModel& model) noexcept : m_Model(model) {}

std::string shade::codegen::CIdaTypeFormatter::FormatName(const schema::SchemaTypeName_t& name) const {
	return FormatIdaName(name);
}

std::string shade::codegen::CIdaTypeFormatter::FormatEnumeratorName(const schema::SchemaTypeName_t& enumName, std::string_view rawValueName) const {
	return std::format("{}_{}", enumName.m_szName, rawValueName);
}

std::string shade::codegen::CIdaTypeFormatter::FormatAtomicName(std::string_view name, const schema::SchemaAtomicParameters_t& parameters) const {
	ActiveTypeRefs_t validationTypes;
	if (IsAtomicInvalidByValue(m_Model, parameters, validationTypes))
		return "void";

	ActiveTypeRefs_t encodingTypes;
	return FormatIdaAtomic(m_Model, name, parameters, encodingTypes);
}

bool shade::codegen::CIdaTypeFormatter::IsValidEnumUnderlyingType(schema::SchemaTypeRef_t type) const {
	return ::IsValidEnumUnderlyingType(m_Model, type);
}

std::string shade::codegen::CIdaTypeFormatter::FormatType(schema::SchemaTypeRef_t type) const {
	ActiveTypeRefs_t activeTypes;
	return ::FormatType(m_Model, type, ETypeFormat::IDA, activeTypes);
}

std::string shade::codegen::CIdaTypeFormatter::FormatDeclaration(schema::SchemaTypeRef_t type, std::string_view name, std::size_t size) const {
	return ::FormatDeclaration(m_Model, type, name, size, ETypeFormat::IDA);
}

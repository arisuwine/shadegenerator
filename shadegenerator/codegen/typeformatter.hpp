#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "schema/model.hpp"

namespace shade::codegen {
	/** @brief Converts schema types into declarations for a target language or tool. */
	class ITypeFormatter {
	public:
		/** @brief Formats a module-qualified schema type name. */
		[[nodiscard]] virtual std::string FormatName(const schema::SchemaTypeName_t& name) const = 0;
		/** @brief Formats a schema type for use in a type expression. */
		[[nodiscard]] virtual std::string FormatType(schema::SchemaTypeRef_t type) const = 0;
		/** @brief Formats a complete named declaration, using size for any required fallback storage. */
		[[nodiscard]] virtual std::string FormatDeclaration(schema::SchemaTypeRef_t type, std::string_view name, std::size_t size) const = 0;

		/** @brief Enables safe destruction through the formatter interface. */
		virtual ~ITypeFormatter() = default;
	};

	/**
	 * @brief Formats schema types and declarations for generated C++ code.
	 */
	class CCppTypeFormatter final : public ITypeFormatter {
	private:
		const schema::CSchemaModel& m_Model;

	public:
		/**
		 * @brief Constructs a C++ type formatter for a schema model.
		 *
		 * @param model A `const schema::CSchemaModel&`.
		 */
		explicit CCppTypeFormatter(const schema::CSchemaModel& model) noexcept;

		/**
		 * @brief Formats a fully qualified schema type name for C++ output.
		 *
		 * @param name A `const schema::SchemaTypeName_t&`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatName(const schema::SchemaTypeName_t& name) const override;

		/**
		 * @brief Checks whether atomic parameters contain a type that cannot be represented by value.
		 *
		 * @param parameters A `const schema::SchemaAtomicParameters_t&`.
		 * @return A `bool`.
		 */
		[[nodiscard]] bool IsAtomicInvalidByValue(const schema::SchemaAtomicParameters_t& parameters) const;

		/**
		 * @brief Checks whether a schema type is a valid C++ enum underlying type.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @return A `bool`.
		 */
		[[nodiscard]] bool IsValidEnumUnderlyingType(schema::SchemaTypeRef_t type) const;

		/**
		 * @brief Formats a schema type for use in C++ type expressions.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatType(schema::SchemaTypeRef_t type) const override;

		/**
		 * @brief Returns the size of the C++ type emitted for a schema type when it can be determined.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @return The emitted C++ size, or `std::nullopt` when it cannot be determined safely.
		 */
		[[nodiscard]] std::optional<std::size_t> GetGeneratedSize(schema::SchemaTypeRef_t type) const;

		/**
		 * @brief Formats a complete C++ declaration and preserves invalid fields through a byte-array fallback.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @param name A `std::string_view`.
		 * @param size A `std::size_t`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatDeclaration(schema::SchemaTypeRef_t type, std::string_view name, std::size_t size) const override;
	};

	/**
	 * @brief Formats schema types and declarations for IDA-compatible output.
	 */
	class CIdaTypeFormatter final : public ITypeFormatter {
	private:
		const schema::CSchemaModel& m_Model;

	public:
		/**
		 * @brief Constructs an IDA type formatter for a schema model.
		 *
		 * @param model A `const schema::CSchemaModel&`.
		 */
		explicit CIdaTypeFormatter(const schema::CSchemaModel& model) noexcept;

		/**
		 * @brief Formats a schema type name as an IDA-compatible identifier.
		 *
		 * @param name A `const schema::SchemaTypeName_t&`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatName(const schema::SchemaTypeName_t& name) const override;

		/**
		 * @brief Formats an enumerator name using its parent enum name.
		 *
		 * @param enumName A `const schema::SchemaTypeName_t&`.
		 * @param rawValueName A `std::string_view`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatEnumeratorName(const schema::SchemaTypeName_t& enumName, std::string_view rawValueName) const;

		/**
		 * @brief Formats an atomic specialization as an IDA-compatible identifier.
		 *
		 * @param name A `std::string_view`.
		 * @param parameters A `const schema::SchemaAtomicParameters_t&`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatAtomicName(std::string_view name, const schema::SchemaAtomicParameters_t& parameters) const;

		/**
		 * @brief Checks whether a schema type is a valid IDA enum underlying type.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @return A `bool`.
		 */
		[[nodiscard]] bool IsValidEnumUnderlyingType(schema::SchemaTypeRef_t type) const;

		/**
		 * @brief Formats a schema type for use in IDA type expressions.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatType(schema::SchemaTypeRef_t type) const override;

		/**
		 * @brief Formats a complete IDA declaration and preserves invalid fields through a byte-array fallback.
		 *
		 * @param type A `schema::SchemaTypeRef_t`.
		 * @param name A `std::string_view`.
		 * @param size A `std::size_t`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatDeclaration(schema::SchemaTypeRef_t type, std::string_view name, std::size_t size) const override;
	};
} // namespace shade::codegen

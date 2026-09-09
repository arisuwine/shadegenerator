#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "iemitter.hpp"

#include "schema/dependencies.hpp"

namespace shade::codegen {
	class CGenerator;
	class CCppTypeFormatter;

	/**
	 * @brief Emits schema declarations as standard C++ code.
	 */
	class CCppEmitter final : public IEmitter {
	private:
		CGenerator&              m_Generator;
		const CCppTypeFormatter& m_Formatter;

		/**
		 * @brief Emits a comment describing the schema class layout and flags.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 */
		void ClassParameters(const schema::SchemaClassRecord_t& record);

		/**
		 * @brief Emits data map fields as an informational comment.
		 *
		 * @param fields A `std::span<const schema::SchemaDataMapFieldRecord_t>`.
		 */
		void DataMapFields(std::span<const schema::SchemaDataMapFieldRecord_t> fields);

		/**
		 * @brief Emits class fields and padding from the specified layout cursor.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 * @param nCursor A `std::size_t`.
		 * @return A `std::size_t`.
		 */
		[[nodiscard]] std::size_t Fields(const schema::SchemaClassRecord_t& record, std::size_t nCursor);

		/**
		 * @brief Validates direct base-class ranges and computes the packed inheritance cursor.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 * @return A `std::size_t`.
		 */
		[[nodiscard]] std::size_t BaseCursor(const schema::SchemaClassRecord_t& record) const;

		/**
		 * @brief Formats the name of an atomic specialization for C++ output.
		 *
		 * @param record A `const schema::SchemaAtomicRecord_t&`.
		 * @param parameters A `const schema::SchemaAtomicParameters_t&`.
		 * @return A `std::string`.
		 */
		[[nodiscard]] std::string FormatAtomicName(const schema::SchemaAtomicRecord_t& record, const schema::SchemaAtomicParameters_t& parameters) const;

		/**
		 * @brief Emits the template parameter list required by an atomic type.
		 *
		 * @param record A `const schema::SchemaAtomicRecord_t&`.
		 * @param includeDefaultArgument A `bool`.
		 */
		void AtomicTemplate(const schema::SchemaAtomicRecord_t& record, bool includeDefaultArgument);

	public:
		/**
		 * @brief Constructs a C++ emitter using a generator and type formatter.
		 *
		 * @param generator A `CGenerator&`.
		 * @param formatter A `const CCppTypeFormatter&`.
		 */
		CCppEmitter(CGenerator& generator, const CCppTypeFormatter& formatter) noexcept : m_Generator(generator), m_Formatter(formatter) {}

		/**
		 * @brief Emits the generated file preamble and required standard includes.
		 */
		void Prologue() override;

		/**
		 * @brief Emits a schema class or structure declaration.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 */
		void Class(const schema::SchemaClassRecord_t& record) override;

		/**
		 * @brief Emits a scoped schema enumeration declaration.
		 *
		 * @param record A `const schema::SchemaEnumRecord_t&`.
		 */
		void Enum(const schema::SchemaEnumRecord_t& record) override;

		/**
		 * @brief Emits an atomic type and its layout specializations.
		 *
		 * @param record A `const schema::SchemaAtomicRecord_t&`.
		 */
		void Atomic(const schema::SchemaAtomicRecord_t& record) override;

		/**
		 * @brief Emits includes and forward declarations for schema dependencies.
		 *
		 * @param dependencies A `const schema::SchemaDependencies_t&`.
		 */
		void Dependencies(const schema::SchemaDependencies_t& dependencies);

		/**
		 * @brief Emits forward declarations for valid atomic types.
		 *
		 * @param records A `std::span<const schema::SchemaAtomicRecord_t>`.
		 */
		void AtomicForwardDeclarations(std::span<const schema::SchemaAtomicRecord_t> records);

		[[nodiscard]] std::string_view GetExtension() const noexcept override {
			return ".hpp";
		}
	};
} // namespace shade::codegen

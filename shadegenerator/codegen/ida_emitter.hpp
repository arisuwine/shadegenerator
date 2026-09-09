#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "iemitter.hpp"
#include "schema/dependencies.hpp"

namespace shade::codegen {
	class CGenerator;
	class CIdaTypeFormatter;

	/**
	 * @brief Specifies the source syntax accepted by the target IDA version.
	 */
	enum class EIdaSyntax : std::uint8_t {
		CPP,
		C
	};

	/**
	 * @brief Emits one-file, flattened schema declarations for IDA.
	 */
	class CIdaEmitter final : public IEmitter {
	private:
		CGenerator&              m_Generator;
		const CIdaTypeFormatter& m_Formatter;
		EIdaSyntax               m_Syntax;

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
		 * @brief Validates C++ base-class layout and returns the end of its schema ranges.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 * @return A `std::size_t`.
		 */
		[[nodiscard]] std::size_t CppBaseCursor(const schema::SchemaClassRecord_t& record);

		/**
		 * @brief Emits C-compatible base-class fields and returns their layout cursor.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 * @return A `std::size_t`.
		 */
		[[nodiscard]] std::size_t CBaseFields(const schema::SchemaClassRecord_t& record);

		/**
		 * @brief Emits class fields and padding from the specified layout cursor.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 * @param nCursor A `std::size_t`.
		 * @return A `std::size_t`.
		 */
		std::size_t Fields(const schema::SchemaClassRecord_t& record, std::size_t nCursor);

	public:
		/**
		 * @brief Constructs an IDA emitter for the selected source syntax.
		 *
		 * @param generator A `CGenerator&`.
		 * @param formatter A `const CIdaTypeFormatter&`.
		 * @param syntax An `EIdaSyntax`.
		 */
		CIdaEmitter(CGenerator& generator, const CIdaTypeFormatter& formatter, EIdaSyntax syntax) noexcept
		    : m_Generator(generator), m_Formatter(formatter), m_Syntax(syntax) {}

		/**
		 * @brief Emits the generated file preamble.
		 */
		void Prologue() override;

		/**
		 * @brief Emits a schema class using the selected IDA source syntax.
		 *
		 * @param record A `const schema::SchemaClassRecord_t&`.
		 */
		void Class(const schema::SchemaClassRecord_t& record) override;

		/**
		 * @brief Emits an unscoped schema enumeration declaration.
		 *
		 * @param record A `const schema::SchemaEnumRecord_t&`.
		 */
		void Enum(const schema::SchemaEnumRecord_t& record) override;

		/**
		 * @brief Emits flattened declarations for an atomic type and its specializations.
		 *
		 * @param record A `const schema::SchemaAtomicRecord_t&`.
		 */
		void Atomic(const schema::SchemaAtomicRecord_t& record) override;

		/**
		 * @brief Emits flattened forward declarations for schema dependencies.
		 *
		 * @param dependencies A `const schema::SchemaDependencies_t&`.
		 */
		void ForwardDeclarations(const schema::SchemaDependencies_t& dependencies);

		[[nodiscard]] std::string_view GetExtension() const noexcept override {
			return ".hpp";
		}
	};
} // namespace shade::codegen

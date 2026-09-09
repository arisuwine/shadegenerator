#pragma once
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "outputfile.hpp"

namespace shade {
	namespace codegen {
		/**
		 * @brief Specifies how an include directive is emitted.
		 */
		enum class EIncludeType : uint8_t {
			Local,
			System
		};

		/**
		 * @brief Specifies whether an enum is scoped.
		 */
		enum class EEnumStyle : uint8_t {
			Scoped,
			Unscoped
		};

		/**
		 * @brief Specifies the type of template parameter.
		 */
		enum class ETemplateType : uint8_t {
			T,
			Integer,
			IntegerDefaultZero
		};

		/**
		 * @brief Generates C++ source code and writes it to an output file.
		 *
		 * Maintains indentation, line state, and block-comment state while emitting
		 * declarations, fields, includes, templates, and namespaces.
		 *
		 * @see codegen::COutputFile
		 */
		class CGenerator {
		public:
			using Self = std::add_lvalue_reference_t<CGenerator>;

		private:
			COutputFile& m_File;

			size_t m_nIndentLevel;
			bool   m_bAtLineStart;
			bool   m_bIsBlockComment;

		private:
			Self Write(std::string_view szText);

			Self BeginBlock(std::string_view szText);
			Self EndBlock(std::string_view szEnd = "");

			void Indent() {
				m_nIndentLevel++;
			}

			void Outdent() {
				if (m_nIndentLevel > 0)
					m_nIndentLevel--;
			}

		public:
			/** @brief Creates a generator that writes to an already opened output file. */
			explicit CGenerator(COutputFile& file) : m_File(file), m_nIndentLevel(0), m_bAtLineStart(false), m_bIsBlockComment(false) {}

			/** @brief Starts a new output line. */
			Self NewLine();

			/** @brief Starts a generated block comment. */
			Self StartBlockComment();
			/** @brief Ends the current generated block comment. */
			Self EndBlockComment();
			/** @brief Writes a comment line. */
			Self Comment(std::string_view szText);

			/** @brief Writes a local or system include directive. */
			Self Include(std::string_view szText, EIncludeType includeType);
			/** @brief Writes a size assertion for a generated type. */
			Self StaticAssert(std::string_view szName, size_t nExpectedSize);
			/** @brief Writes a pragma directive. */
			Self Pragma(std::string_view szDirective);
			/** @brief Writes a using-namespace directive. */
			Self UsingNamespace(std::string_view szName);

			/** @brief Starts a template declaration with the requested parameter kinds. */
			Self Template(std::initializer_list<ETemplateType> types);
			/** @brief Writes an explicit-template-specialization prefix. */
			Self TemplateSpecialization();

			/** @brief Writes a declaration and its schema layout comment. */
			Self Declaration(std::string_view szDeclaration, size_t nOffset, size_t nSize, size_t nBitWidth = 0);
			/** @brief Writes a named value field and its schema layout comment. */
			Self Field(std::string_view szTypeName, std::string_view szFieldName, size_t nOffset, size_t nSize);
			/** @brief Writes a pointer field and its schema layout comment. */
			Self PointerField(std::string_view szTypeName, std::string_view szFieldName, size_t nOffset, size_t nSize);
			/** @brief Writes a multidimensional array field and its schema layout comment. */
			Self ArrayField(std::string_view szTypeName, std::string_view szFieldName, std::span<const size_t> Dimensions, size_t nOffset, size_t nSize);
			/** @brief Writes a bit-field declaration. */
			Self Bitfield(std::string_view szTypeName, std::string_view szFieldName, size_t nBits);
			/** @brief Writes explicitly sized padding storage. */
			Self Pad(std::string_view szByteType, size_t nOffset, size_t nSize);
			/** @brief Writes one enumerator. */
			Self EnumField(std::string_view szName, std::int64_t nValue, bool bIsLast = false);

			/** @brief Writes a struct forward declaration. */
			Self StructForwardDeclaration(std::string_view szName);
			/** @brief Writes a class forward declaration. */
			Self ClassForwardDeclaration(std::string_view szName);
			/** @brief Writes an enum forward declaration. */
			Self EnumForwardDeclaration(std::string_view szName, std::string_view szType, EEnumStyle style = EEnumStyle::Scoped);

			/** @brief Writes an access qualifier. */
			Self AccessQualifier(std::string_view szAccess);

			/** @brief Writes a class and invokes the callback for its body. */
			template <typename fn>
			Self Class(std::string_view szName, std::span<const std::string> baseClasses, fn&& fnCallback);
			/** @brief Writes a struct and invokes the callback for its body. */
			template <typename fn>
			Self Struct(std::string_view szName, std::span<const std::string> baseStructs, fn&& fnCallback);
			/** @brief Writes a namespace and invokes the callback for its body. */
			template <typename fn>
			Self Namespace(std::string_view szName, fn&& fnCallback);
			/** @brief Writes an enum and invokes the callback for its body. */
			template <typename fn>
			Self Enum(std::string_view szName, std::string_view szType, EEnumStyle style, fn&& fnCallback);
		};
	} // namespace codegen
} // namespace shade

template <typename fn>
shade::codegen::CGenerator::Self shade::codegen::CGenerator::Namespace(std::string_view szName, fn&& fnCallback) {
	BeginBlock(std::format("namespace {}", szName));

	std::invoke(std::forward<fn>(fnCallback));

	return EndBlock("\n");
}

template <typename fn>
shade::codegen::CGenerator::Self shade::codegen::CGenerator::Class(std::string_view szName, std::span<const std::string> baseClasses, fn&& fnCallback) {
	std::string szBlock = std::format("class {}", szName);
	for (std::size_t index = 0; index < baseClasses.size(); ++index)
		szBlock += std::format("{}public {}", index == 0 ? " : " : ", ", baseClasses[index]);

	BeginBlock(szBlock);

	AccessQualifier("public");

	std::invoke(std::forward<fn>(fnCallback));

	return EndBlock(";");
}

template <typename fn>
shade::codegen::CGenerator::Self shade::codegen::CGenerator::Struct(std::string_view szName, std::span<const std::string> baseStructs, fn&& fnCallback) {
	std::string szBlock = std::format("struct {}", szName);
	for (std::size_t index = 0; index < baseStructs.size(); ++index)
		szBlock += std::format("{}public {}", index == 0 ? " : " : ", ", baseStructs[index]);

	BeginBlock(szBlock);

	std::invoke(std::forward<fn>(fnCallback));

	return EndBlock(";");
}

template <typename fn>
shade::codegen::CGenerator::Self shade::codegen::CGenerator::Enum(std::string_view szName, std::string_view szType, EEnumStyle style, fn&& fnCallback) {
	std::string declaration = std::format("enum {}{}", style == EEnumStyle::Scoped ? "class " : "", szName);
	if (!szType.empty())
		declaration += std::format(" : {}", szType);
	BeginBlock(declaration);

	std::invoke(std::forward<fn>(fnCallback));

	return EndBlock(";");
}

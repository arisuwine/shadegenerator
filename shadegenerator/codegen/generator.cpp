#include "generator.hpp"

#include <limits>

using namespace shade;

codegen::CGenerator::Self codegen::CGenerator::Write(std::string_view szText) {
	for (char ch : szText) {
		if (m_bAtLineStart && ch != '\n') {
			m_File << std::string(m_nIndentLevel * 4, ' ');
			m_bAtLineStart = false;
		}

		m_File.GetStream().put(ch);
		if (ch == '\n')
			m_bAtLineStart = true;
	}

	return *this;
}

codegen::CGenerator::Self codegen::CGenerator::StartBlockComment() {
	m_bIsBlockComment = true;

	return Write("/*");
}

codegen::CGenerator::Self codegen::CGenerator::EndBlockComment() {
	m_bIsBlockComment = false;

	return Write("*/");
}

codegen::CGenerator::Self codegen::CGenerator::Comment(std::string_view szText) {
	return Write(std::format("{}{}", m_bIsBlockComment ? "" : "// ", szText));
}

codegen::CGenerator::Self codegen::CGenerator::Include(std::string_view szText, codegen::EIncludeType includeType) {
	Write("#include ");
	std::string szName =
	    std::format("{}{}{}\n", includeType == EIncludeType::System ? "<" : "\"", szText, includeType == EIncludeType::System ? ">" : "\"");
	return Write(szName);
}

codegen::CGenerator::Self codegen::CGenerator::NewLine() {
	return Write("\n");
}

codegen::CGenerator::Self codegen::CGenerator::BeginBlock(std::string_view szText) {
	Write(std::format("{} {{\n", szText));
	Indent();

	return *this;
}

codegen::CGenerator::Self codegen::CGenerator::EndBlock(std::string_view szEnd) {
	if (!m_bAtLineStart)
		NewLine();

	Outdent();
	Write("}");
	if (!szEnd.empty())
		Write(szEnd);

	return *this;
}

codegen::CGenerator::Self codegen::CGenerator::StaticAssert(std::string_view szName, size_t nExpectedSize) {
	return Write(std::format("static_assert(sizeof({}) == 0x{:X}, \"{} size mismatch\");\n", szName, nExpectedSize, szName));
}

codegen::CGenerator::Self codegen::CGenerator::Pragma(std::string_view szDirective) {
	return Write(std::format("#pragma {}\n", szDirective));
}

codegen::CGenerator::Self codegen::CGenerator::UsingNamespace(std::string_view szName) {
	return Write(std::format("using namespace {};\n", szName));
}

codegen::CGenerator::Self codegen::CGenerator::Declaration(std::string_view szDeclaration, size_t nOffset, size_t nSize, size_t nBitWidth) {
	Write(szDeclaration);
	if (nBitWidth != 0)
		Write(std::format(" : {}", nBitWidth));
	Write("; ");
	return Comment(std::format("0x{:04x}, 0x{:x} bytes\n", nOffset, nSize));
}

codegen::CGenerator::Self codegen::CGenerator::Field(std::string_view szTypeName, std::string_view szFieldName, size_t nOffset, size_t nSize) {
	Write(std::format("{} {}; ", szTypeName, szFieldName));
	return Comment(std::format("0x{:04x}, 0x{:x} bytes\n", nOffset, nSize));
}

codegen::CGenerator::Self codegen::CGenerator::PointerField(std::string_view szTypeName, std::string_view szFieldName, size_t nOffset, size_t nSize) {
	Write(std::format("{}* {}; ", szTypeName, szFieldName));
	return Comment(std::format("0x{:04x}, 0x{:x} bytes\n", nOffset, nSize));
}

codegen::CGenerator::Self codegen::CGenerator::ArrayField(std::string_view szTypeName, std::string_view szFieldName, std::span<const size_t> Dimensions,
                                                          size_t nOffset, size_t nSize) {
	Write(std::format("{} {}", szTypeName, szFieldName));
	for (const size_t nDimension : Dimensions)
		Write(std::format("[{}]", nDimension));
	Write("; ");
	return Comment(std::format("0x{:04x}, 0x{:x} bytes\n", nOffset, nSize));
}

codegen::CGenerator::Self codegen::CGenerator::Pad(std::string_view szByteType, size_t nOffset, size_t nSize) {
	Write(std::format("{} pad_{:04x}[0x{:x}]; ", szByteType, nOffset, nSize));
	return Comment(std::format("0x{:04x}, 0x{:x} bytes\n", nOffset, nSize));
}

codegen::CGenerator::Self codegen::CGenerator::EnumField(std::string_view szName, std::int64_t nValue, bool bIsLast) {
	if (nValue == (std::numeric_limits<std::int64_t>::min)()) {
		Write(std::format("{} = (-9223372036854775807LL - 1)", szName));
	} else if (nValue < 0) {
		const auto magnitude = std::uint64_t{ 0 } - static_cast<std::uint64_t>(nValue);
		Write(std::format("{} = -0x{:x}", szName, magnitude));
	} else {
		Write(std::format("{} = 0x{:x}", szName, static_cast<std::uint64_t>(nValue)));
	}
	if (!bIsLast)
		Write(",");

	return NewLine();
}

codegen::CGenerator::Self codegen::CGenerator::Bitfield(std::string_view szTypeName, std::string_view szFieldName, size_t nBits) {
	Write(std::format("{} {} : {}; ", szTypeName, szFieldName, nBits));
	return Comment("bitfield\n");
}

codegen::CGenerator::Self codegen::CGenerator::StructForwardDeclaration(std::string_view szName) {
	return Write(std::format("struct {};\n", szName));
}

codegen::CGenerator::Self codegen::CGenerator::ClassForwardDeclaration(std::string_view szName) {
	return Write(std::format("class {};\n", szName));
}

codegen::CGenerator::Self codegen::CGenerator::EnumForwardDeclaration(std::string_view szName, std::string_view szType, EEnumStyle style) {
	return Write(std::format("enum {}{} : {};\n", style == EEnumStyle::Scoped ? "class " : "", szName, szType));
}

codegen::CGenerator::Self codegen::CGenerator::AccessQualifier(std::string_view szAccess) {
	Outdent();
	Write(std::format("{}:\n", szAccess));
	Indent();

	return *this;
}

codegen::CGenerator::Self codegen::CGenerator::Template(std::initializer_list<ETemplateType> types) {
	size_t I = 0;
	size_t T = 0;

	size_t i = 0;

	std::string szFormat = "template <";

	for (const auto type : types) {
		if (i++ != 0)
			szFormat += ", ";

		switch (type) {
		case ETemplateType::Integer:
			szFormat += std::format("int N{}", I++);
			break;
		case ETemplateType::IntegerDefaultZero:
			szFormat += std::format("int N{} = 0", I++);
			break;
		case ETemplateType::T:
			szFormat += std::format("typename T{}", T++);
			break;
		}
	}

	szFormat += ">\n";

	return Write(szFormat);
}

codegen::CGenerator::Self codegen::CGenerator::TemplateSpecialization() {
	return Write("template<>\n");
}

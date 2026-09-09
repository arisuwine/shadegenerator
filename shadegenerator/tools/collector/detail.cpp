#include "detail.hpp"

#include <cctype>
#include <string>
#include <string_view>

#include "schema/types.hpp"

#include "sdk/schemasystem/schematypes.hpp"

namespace shade::tools::collector {
	using schema::ESchemaBuiltinType;
	using schema::SchemaTypeName_t;

	[[nodiscard]] std::string GetUnsupportedEnumTypeName(const CSchemaEnumInfo& info) {
		const SchemaTypeName_t typeName = { .m_szModule = std::string(info.GetModuleName()), .m_szName = info.GetName() };
		std::string            szQualifiedName;

		if (!typeName.m_szModule.empty())
			szQualifiedName = typeName.m_szModule + "::";

		szQualifiedName += typeName.m_szName.empty() ? "<unnamed>" : typeName.m_szName;

		return "enum " + szQualifiedName + " (" + std::to_string(static_cast<std::size_t>(info.m_nSize) * 8) + "-bit underlying type)";
	}

	[[nodiscard]] std::size_t NonNegativeSize(int nValue) {
		return nValue > 0 ? static_cast<std::size_t>(nValue) : 0;
	}

	[[nodiscard]] std::optional<ESchemaBuiltinType> GetIntegerType(std::size_t nSize, bool bIsSigned) {
		switch (nSize) {
		case 1:
			return bIsSigned ? ESchemaBuiltinType::INT8 : ESchemaBuiltinType::UINT8;
		case 2:
			return bIsSigned ? ESchemaBuiltinType::INT16 : ESchemaBuiltinType::UINT16;
		case 4:
			return bIsSigned ? ESchemaBuiltinType::INT32 : ESchemaBuiltinType::UINT32;
		case 8:
			return bIsSigned ? ESchemaBuiltinType::INT64 : ESchemaBuiltinType::UINT64;
		default:
			return std::nullopt;
		}
	}

	[[nodiscard]] std::string GetAtomicName(const CSchemaType_Atomic* pAtomic) {
		std::string_view szName = pAtomic->GetTypeName();

		if (const auto templateStart = szName.find('<'); templateStart != std::string_view::npos)
			szName = szName.substr(0, templateStart);

		while (!szName.empty() && std::isspace(static_cast<unsigned char>(szName.back())))
			szName.remove_suffix(1);

		std::string name(szName);
		if (szName.starts_with("std::"))
			return name;

		for (std::size_t separator = name.find("::"); separator != std::string::npos; separator = name.find("::", separator + 2))
			name.replace(separator, 2, "__");

		return name;
	}
} // namespace shade::tools::collector

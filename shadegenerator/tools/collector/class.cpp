#include "collector.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "detail.hpp"

#include "tools/bitfield.hpp"

#include "schema/classrecord.hpp"

#include "sdk/public/datamap.hpp"
#include "sdk/schemasystem/schematypes.hpp"

#include "utils/debug.hpp"

namespace {
	using shade::tools::collector::GetIntegerType;
	using shade::tools::collector::GetUnsupportedEnumTypeName;
	using shade::tools::collector::NonNegativeSize;

	[[nodiscard]] std::size_t GetTypeSize(const CSchemaType* pType) {
		if (!pType)
			return 0;

		switch (pType->m_eTypeCategory) {
		case SCHEMA_TYPE_BUILTIN:
			return pType->Cast<CSchemaType_Builtin>()->GetSize();

		case SCHEMA_TYPE_POINTER:
			return sizeof(void*);

		case SCHEMA_TYPE_FIXED_ARRAY: {
			const auto pArray = pType->Cast<CSchemaType_FixedArray>();
			if (pArray->m_nElementCount <= 0)
				return 0;

			return static_cast<std::size_t>(pArray->m_nElementSize) * static_cast<std::size_t>(pArray->m_nElementCount);
		}

		case SCHEMA_TYPE_ATOMIC:
			return pType->Cast<CSchemaType_Atomic>()->m_nSize;

		case SCHEMA_TYPE_DECLARED_CLASS: {
			const auto pDeclaredClass = pType->Cast<CSchemaType_DeclaredClass>();
			return pDeclaredClass->m_pClassInfo ? NonNegativeSize(pDeclaredClass->m_pClassInfo->m_nSize) : 0;
		}

		case SCHEMA_TYPE_DECLARED_ENUM: {
			const auto* pDeclaredEnum = pType->Cast<CSchemaType_DeclaredEnum>();
			return pDeclaredEnum->m_pEnumInfo ? pDeclaredEnum->m_pEnumInfo->m_nSize : 0;
		}

		case SCHEMA_TYPE_BITFIELD:
		case SCHEMA_TYPE_INVALID:
		default:
			return 0;
		}
	}

	[[nodiscard]] std::uint32_t GetClassFlags(const CSchemaClassInfo& info) {
		std::uint32_t result = shade::schema::ESchemaClassFlags::SCHEMA_CLASS_NONE;
		const auto    flags  = info.GetFlags();

		if (flags & SchemaClassFlags_t::SCHEMA_CF1_HAS_VIRTUAL_MEMBERS)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_HAS_VTABLE;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_IS_ABSTRACT)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_IS_ABSTRACT;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_HAS_TRIVIAL_CONSTRUCTOR)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_HAS_TRIVIAL_CONSTRUCTOR;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_HAS_TRIVIAL_DESTRUCTOR)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_HAS_TRIVIAL_DESTRUCTOR;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_CONSTRUCT_ALLOWED)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_CONSTRUCT_ALLOWED;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_CONSTRUCT_DISALLOWED)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_CONSTRUCT_DISALLOWED;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_MODULE_LOCAL_TYPE_SCOPE)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_MODULE_LOCAL_TYPE_SCOPE;
		if (flags & SchemaClassFlags_t::SCHEMA_CF1_GLOBAL_TYPE_SCOPE)
			result |= shade::schema::ESchemaClassFlags::SCHEMA_CLASS_GLOBAL_TYPE_SCOPE;

		return result;
	}

	[[nodiscard]] shade::schema::ESchemaClassType GetClassType(std::string_view szName) {
		return szName.ends_with("_t") ? shade::schema::ESchemaClassType::STRUCT : shade::schema::ESchemaClassType::CLASS;
	}

} // namespace

shade::schema::SchemaClassRecord_t shade::tools::CSchemaCollector::CollectClass(const CSchemaClassInfo& info) {
	schema::SchemaClassRecord_t result{
		.m_Name = { .m_szModule = std::string(info.GetModuleName()), .m_szName = info.GetName() },
		.m_Layout = {
			.m_nSize = NonNegativeSize(info.m_nSize),
			.m_nAlignment = info.m_nAlignment,
		},
		.m_eType = GetClassType(info.GetName()),
		.m_eFlags = GetClassFlags(info),
		.m_BaseClasses = {},
		.m_Fields = {},
		.m_DataMapFields = {},
	};

	std::size_t nCursor = 0;
	CollectBaseClasses(info, result, nCursor);
	CollectClassFields(info, result, nCursor);
	CollectDataMapFields(info, result);

	return result;
}

void shade::tools::CSchemaCollector::CollectBaseClasses(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record, std::size_t& nCursor) {
	for (const auto& base : info.GetBaseClasses()) {
		if (!base.m_pClass)
			continue;

		const std::size_t nBaseSize   = NonNegativeSize(base.m_pClass->m_nSize);
		const std::size_t nBaseOffset = static_cast<std::size_t>(base.m_nOffset);
		if (nBaseOffset > record.m_Layout.m_nSize || nBaseSize > record.m_Layout.m_nSize - std::min(nBaseOffset, record.m_Layout.m_nSize))
			lg::Warn("collector", "base '{}' has a range outside class '{}'", std::string(base.m_pClass->GetName()), record.m_Name.m_szName);

		for (const auto& previous : record.m_BaseClasses) {
			const auto begin = std::max(nBaseOffset, previous.m_nOffset);
			const auto end   = std::min(nBaseOffset + nBaseSize, previous.m_nOffset + previous.m_nSize);

			if (begin < end) {
				lg::Warn("collector", "direct bases '{}' and '{}' overlap in class '{}'", previous.m_Name.m_szName, std::string(base.m_pClass->GetName()),
				         record.m_Name.m_szName);

				break;
			}
		}

		record.m_BaseClasses.push_back({
		    .m_Name    = { .m_szModule = std::string(base.m_pClass->GetModuleName()), .m_szName = base.m_pClass->GetName() },
		    .m_nOffset = nBaseOffset,
		    .m_nSize   = nBaseSize,
		});

		nCursor = std::max(nCursor, nBaseOffset + nBaseSize);
	}
}

void shade::tools::CSchemaCollector::CollectBitfieldRun(const CSchemaClassInfo& info, std::size_t nBegin, std::size_t nEnd,
                                                        schema::SchemaClassRecord_t& record, std::size_t& nCursor) {
	const auto&       fields         = info.GetFields();
	const auto&       field          = fields[nBegin];
	const std::size_t nSchemaOffset  = field.m_nSingleInheritanceOffset < 0 ? nCursor : static_cast<std::size_t>(field.m_nSingleInheritanceOffset);
	const std::size_t nBitfieldStart = std::max(nCursor, nSchemaOffset);
	const std::size_t nBoundary      = nEnd < fields.size() && fields[nEnd].m_nSingleInheritanceOffset >= 0 ?
	                                       static_cast<std::size_t>(fields[nEnd].m_nSingleInheritanceOffset) :
	                                       record.m_Layout.m_nSize;
	const auto        bitfields      = fields.subspan(nBegin, nEnd - nBegin);

	std::optional<BitfieldLayout_t> layout;
	if (nBoundary >= nBitfieldStart)
		layout = ResolveBitfieldLayout(bitfields, nBoundary - nBitfieldStart);

	if (!layout) {
		lg::Warn("collector", "could not resolve bitfield layout in class '{}' at offset {}", record.m_Name.m_szName, nBitfieldStart);

		for (const auto& bitfield : bitfields) {
			record.m_Fields.push_back({
			    .m_Type    = AddInvalidType(bitfield.m_pType ? std::string(bitfield.m_pType->GetTypeName()) : "<null bitfield>"),
			    .m_szName  = std::string(bitfield.GetName()),
			    .m_nOffset = nBitfieldStart,
			    .m_nSize   = 0,
			    .m_nBitWidth =
			        bitfield.m_pType ? static_cast<std::size_t>(std::max(0, bitfield.m_pType->Cast<CSchemaType_Bitfield>()->m_nBitfieldCount)) : 0,
			});
		}

		nCursor = std::max(nCursor, nBoundary);
		return;
	}

	for (std::size_t nPlacementIndex = 0; nPlacementIndex < bitfields.size(); ++nPlacementIndex) {
		const auto& bitfield    = bitfields[nPlacementIndex];
		const auto& placement   = layout->m_Placements[nPlacementIndex];
		const auto  storageType = GetIntegerType(placement.m_nStorageBits / 8, false);

		record.m_Fields.push_back({
		    .m_Type      = storageType ? AddBuiltinType(*storageType) : AddInvalidType("bitfield storage"),
		    .m_szName    = std::string(bitfield.GetName()),
		    .m_nOffset   = nBitfieldStart + placement.m_nByteOffset,
		    .m_nSize     = placement.m_nStorageBits / 8,
		    .m_nBitWidth = static_cast<std::size_t>(bitfield.m_pType->Cast<CSchemaType_Bitfield>()->m_nBitfieldCount),
		});
	}

	nCursor = std::max(nCursor, nBitfieldStart + layout->m_nSize);
	nCursor = std::max(nCursor, nBoundary);
}

void shade::tools::CSchemaCollector::CollectClassFields(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record, std::size_t& nCursor) {
	const auto fields = info.GetFields();
	for (std::size_t i = 0; i < fields.size();) {
		const auto& field = fields[i];
		if (IsBitfield(field)) {
			std::size_t end = i;
			while (end < fields.size() && IsBitfield(fields[end]))
				++end;

			CollectBitfieldRun(info, i, end, record, nCursor);
			i = end;
			continue;
		}

		const std::size_t nOffset = field.m_nSingleInheritanceOffset < 0 ? nCursor : static_cast<std::size_t>(field.m_nSingleInheritanceOffset);
		std::size_t       nSize   = GetTypeSize(field.m_pType);

		if (nSize == 0) {
			std::size_t nBoundary = record.m_Layout.m_nSize;

			for (std::size_t next = i + 1; next < fields.size(); ++next) {
				if (fields[next].m_nSingleInheritanceOffset >= 0 && static_cast<std::size_t>(fields[next].m_nSingleInheritanceOffset) > nOffset) {
					nBoundary = static_cast<std::size_t>(fields[next].m_nSingleInheritanceOffset);
					break;
				}
			}

			if (nBoundary > nOffset)
				nSize = nBoundary - nOffset;
		}

		record.m_Fields.push_back({
		    .m_Type      = CollectType(field.m_pType),
		    .m_szName    = std::string(field.GetName()),
		    .m_nOffset   = nOffset,
		    .m_nSize     = nSize,
		    .m_nBitWidth = 0,
		});

		nCursor = std::max(nCursor, nOffset + nSize);
		++i;
	}
}

void shade::tools::CSchemaCollector::CollectDataMapFields(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record) {
	std::unordered_set<std::string> fieldNames;
	for (const auto& field : record.m_Fields)
		fieldNames.emplace(field.m_szName);

	if (const auto pDataMap = info.m_pDataDescMap; pDataMap && pDataMap->dataDesc && pDataMap->dataNumFields > 0) {
		for (int i = 0; i < pDataMap->dataNumFields; ++i) {
			const auto&       field  = pDataMap->dataDesc[i];
			const std::string szName = std::string(field.GetFieldName());

			if (!fieldNames.emplace(szName).second)
				continue;

			record.m_DataMapFields.push_back({
			    .m_Type         = CollectDataMapType(field.fieldType),
			    .m_szName       = szName,
			    .m_nOffset      = NonNegativeSize(field.fieldOffset),
			    .m_nSize        = field.fieldSize,
			    .m_nSizeInBytes = NonNegativeSize(field.fieldSizeInBytes),
			});
		}
	}
}

shade::schema::SchemaEnumRecord_t shade::tools::CSchemaCollector::CollectEnum(const CSchemaEnumInfo& info) {
	schema::SchemaEnumRecord_t result{
		.m_Name = { .m_szModule = std::string(info.GetModuleName()), .m_szName = info.GetName() },
		.m_Layout = {
			.m_nSize = info.m_nSize,
			.m_nAlignment = info.m_nAlignment,
		},
		.m_UnderlyingType = schema::kInvalidSchemaTypeRef,
		.m_Values = {},
	};

	if (const auto underlying = GetIntegerType(info.m_nSize, info.m_nMinEnumeratorValue < 0))
		result.m_UnderlyingType = AddBuiltinType(*underlying);
	else
		result.m_UnderlyingType = AddInvalidType(GetUnsupportedEnumTypeName(info));

	for (const auto& value : info.GetEnumerators()) {
		result.m_Values.push_back({
		    .m_szName = std::string(value.GetName()),
		    .m_nValue = value.m_nValue,
		});
	}

	return result;
}

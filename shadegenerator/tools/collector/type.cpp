#include "collector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "detail.hpp"

#include "schema/types.hpp"

#include "sdk/public/datamap.hpp"
#include "sdk/schemasystem/schematypes.hpp"

namespace {
	using shade::schema::ESchemaBuiltinType;
	using shade::tools::collector::GetAtomicName;
	using shade::tools::collector::GetIntegerType;
	using shade::tools::collector::GetUnsupportedEnumTypeName;

	inline constexpr auto kBuiltinVoid = static_cast<ESchemaBuiltinType>(0);

	[[nodiscard]] std::optional<ESchemaBuiltinType> GetBuiltinType(SchemaBuiltinType_t type) {
		switch (type) {
		case SCHEMA_BUILTIN_TYPE_VOID:
			return kBuiltinVoid;
		case SCHEMA_BUILTIN_TYPE_BOOL:
			return ESchemaBuiltinType::BOOL;
		case SCHEMA_BUILTIN_TYPE_CHAR:
			return ESchemaBuiltinType::CHAR;
		case SCHEMA_BUILTIN_TYPE_INT8:
			return ESchemaBuiltinType::INT8;
		case SCHEMA_BUILTIN_TYPE_UINT8:
			return ESchemaBuiltinType::UINT8;
		case SCHEMA_BUILTIN_TYPE_INT16:
			return ESchemaBuiltinType::INT16;
		case SCHEMA_BUILTIN_TYPE_UINT16:
			return ESchemaBuiltinType::UINT16;
		case SCHEMA_BUILTIN_TYPE_INT32:
			return ESchemaBuiltinType::INT32;
		case SCHEMA_BUILTIN_TYPE_UINT32:
			return ESchemaBuiltinType::UINT32;
		case SCHEMA_BUILTIN_TYPE_INT64:
			return ESchemaBuiltinType::INT64;
		case SCHEMA_BUILTIN_TYPE_UINT64:
			return ESchemaBuiltinType::UINT64;
		case SCHEMA_BUILTIN_TYPE_FLOAT32:
			return ESchemaBuiltinType::FLOAT32;
		case SCHEMA_BUILTIN_TYPE_FLOAT64:
			return ESchemaBuiltinType::FLOAT64;
		case SCHEMA_BUILTIN_TYPE_INVALID:
		case SCHEMA_BUILTIN_TYPE_COUNT:
		default:
			return std::nullopt;
		}
	}

	[[nodiscard]] std::optional<fieldtype_t> GetDataMapFieldType(int nType) {
#ifdef SHADE_GAME_CS2
		// CS2 datamaps use a compact field type enum that is distinct from the
		// schema-exposed fieldtype_t. Keep this translation at the ABI boundary.
		static constexpr std::array kFieldTypes{
			FIELD_VOID,
			FIELD_FLOAT32,
			FIELD_STRING,
			FIELD_VECTOR,
			FIELD_QUATERNION,
			FIELD_INT32,
			FIELD_BOOLEAN,
			FIELD_INT16,
			FIELD_CHARACTER,
			FIELD_COLOR32,
			FIELD_EMBEDDED,
			FIELD_EHANDLE,
			FIELD_POSITION_VECTOR,
			FIELD_TIME,
			FIELD_TICK,
			FIELD_SOUNDNAME,
			FIELD_INTERVAL,
			FIELD_INT64,
			FIELD_VECTOR4D,
			FIELD_UINT64,
			FIELD_UINT32,
			FIELD_UTLSTRINGTOKEN,
			FIELD_QANGLE,
			FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_VECTOR,
			FIELD_HMATERIAL,
			FIELD_HMODEL,
			FIELD_NETWORK_QUANTIZED_VECTOR,
			FIELD_NETWORK_QUANTIZED_FLOAT,
			FIELD_DIRECTION_VECTOR_WORLDSPACE,
			FIELD_QANGLE_WORLDSPACE,
			FIELD_QUATERNION_WORLDSPACE,
			FIELD_UTLSTRING,
			FIELD_HRENDERTEXTURE,
			FIELD_HPARTICLESYSTEMDEFINITION,
			FIELD_UINT8,
			FIELD_UINT16,
			FIELD_HPOSTPROCESSING,
			FIELD_MATRIX3X4,
			FIELD_SHIM,
			FIELD_ATTACHMENT_HANDLE,
			FIELD_GLOBALSYMBOL,
			FIELD_HNMGRAPHDEFINITION,
			FIELD_NETWORK_QUANTIZED_VECTORWS,
			FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_VECTORWS,
		};

		if (nType < 0 || static_cast<std::size_t>(nType) >= kFieldTypes.size())
			return std::nullopt;

		return kFieldTypes[static_cast<std::size_t>(nType)];
#else
		return static_cast<fieldtype_t>(nType);
#endif
	}
} // namespace

std::optional<shade::schema::SchemaAtomicParameters_t> shade::tools::CSchemaCollector::CollectAtomicParameters(const CSchemaType_Atomic& atomic) {
	switch (atomic.m_eAtomicCategory) {
	case SCHEMA_ATOMIC_PLAIN:
		return std::monostate{};

	case SCHEMA_ATOMIC_T: {
		const auto* atomicT = atomic.Cast<CSchemaType_Atomic_T>();
		return schema::SchemaAtomicTypeParameter_t{ CollectType(atomicT->m_pTemplateType) };
	}

	case SCHEMA_ATOMIC_COLLECTION_OF_T: {
		const auto* collection = atomic.Cast<CSchemaType_Atomic_CollectionOfT>();
		return schema::SchemaAtomicCollectionParameters_t{
			.m_ElementType       = CollectType(collection->m_pTemplateType),
			.m_nElementSize      = collection->m_nElementSize,
			.m_nFixedBufferCount = static_cast<std::size_t>(collection->m_nFixedBufferCount),
		};
	}

	case SCHEMA_ATOMIC_TT: {
		const auto* atomicTT = atomic.Cast<CSchemaType_Atomic_TT>();
		return schema::SchemaAtomicTwoTypeParameters_t{
			.m_FirstType  = CollectType(atomicTT->m_pTemplateType),
			.m_SecondType = CollectType(atomicTT->m_pTemplateType2),
		};
	}

	case SCHEMA_ATOMIC_I:
		return schema::SchemaAtomicIntegerParameter_t{
			.m_nValue = atomic.Cast<CSchemaType_Atomic_I>()->m_nInteger,
		};

	case SCHEMA_ATOMIC_INVALID:
	default:
		return std::nullopt;
	}
}

shade::schema::SchemaTypeRef_t shade::tools::CSchemaCollector::CollectType(const CSchemaType* type) {
	if (!type)
		return AddInvalidType("<null>");

	if (const auto it = m_CollectedTypes.find(type); it != m_CollectedTypes.end())
		return it->second;

	schema::SchemaTypeRef_t result = schema::kInvalidSchemaTypeRef;
	switch (type->m_eTypeCategory) {
	case SCHEMA_TYPE_BUILTIN: {
		const auto buildInType = GetBuiltinType(type->Cast<CSchemaType_Builtin>()->m_eBuiltinType);
		result                 = buildInType ? AddBuiltinType(*buildInType) : AddInvalidType(std::string(type->GetTypeName()));

		break;
	}
	case SCHEMA_TYPE_POINTER:
		result = m_Model.AddType(schema::PointerType_t{
		    .m_PointeeType = CollectType(type->Cast<CSchemaType_Ptr>()->m_pObjectType),
		});

		break;
	case SCHEMA_TYPE_FIXED_ARRAY: {
		const auto pArray = type->Cast<CSchemaType_FixedArray>();
		result            = m_Model.AddType(schema::FixedArrayType_t{
		    .m_ElementType = CollectType(pArray->m_pElementType),
		    .m_nCount      = pArray->m_nElementCount > 0 ? static_cast<std::size_t>(pArray->m_nElementCount) : 0,
		});

		break;
	}
	case SCHEMA_TYPE_ATOMIC: {
		const auto pAtomic    = type->Cast<CSchemaType_Atomic>();
		auto       parameters = CollectAtomicParameters(*pAtomic);
		if (parameters) {
			result = m_Model.AddType(schema::AtomicType_t{
			    .m_szName     = GetAtomicName(pAtomic),
			    .m_Parameters = *parameters,
			});
		} else
			result = AddInvalidType(std::string(type->GetTypeName()));

		break;
	}
	case SCHEMA_TYPE_DECLARED_CLASS: {
		const auto pDeclared = type->Cast<CSchemaType_DeclaredClass>();
		result = pDeclared->m_pClassInfo ?
		             m_Model.AddType(schema::DeclaredClassType_t{
		                 { .m_szModule = std::string(pDeclared->m_pClassInfo->GetModuleName()), .m_szName = pDeclared->m_pClassInfo->GetName() } }) :
		             AddInvalidType(std::string(type->GetTypeName()));

		break;
	}
	case SCHEMA_TYPE_DECLARED_ENUM: {
		const auto pDeclared = type->Cast<CSchemaType_DeclaredEnum>();
		if (!pDeclared->m_pEnumInfo)
			result = AddInvalidType(std::string(type->GetTypeName()));
		else if (!GetIntegerType(pDeclared->m_pEnumInfo->m_nSize, false))
			result = AddInvalidType(GetUnsupportedEnumTypeName(*pDeclared->m_pEnumInfo));
		else
			result = m_Model.AddType(schema::DeclaredEnumType_t{
			    .m_Name = { .m_szModule = std::string(pDeclared->m_pEnumInfo->GetModuleName()), .m_szName = pDeclared->m_pEnumInfo->GetName() } });

		break;
	}
	case SCHEMA_TYPE_BITFIELD:
	case SCHEMA_TYPE_INVALID:
	default:
		result = AddInvalidType(std::string(type->GetTypeName()));
		break;
	}

	m_CollectedTypes.emplace(type, result);
	return result;
}

shade::schema::SchemaTypeRef_t shade::tools::CSchemaCollector::CollectDataMapType(int nType) {
	auto AddAtomic = [this](std::string name) {
		return m_Model.AddType(schema::AtomicType_t{
		    .m_szName     = std::move(name),
		    .m_Parameters = std::monostate{},
		});
	};

	const auto type = GetDataMapFieldType(nType);
	if (!type)
		return AddInvalidType("datamap field type " + std::to_string(nType));

	/// @author neverlosecc/source2gen
	switch (*type) {
	case FIELD_VOID:
	case FIELD_CUSTOM:
		return AddBuiltinType(kBuiltinVoid);
	case FIELD_FLOAT32:
	case FIELD_ENGINE_TIME:
	case FIELD_NETWORK_QUANTIZED_FLOAT:
		return AddBuiltinType(schema::ESchemaBuiltinType::FLOAT32);
	case FIELD_FLOAT64:
		return AddBuiltinType(schema::ESchemaBuiltinType::FLOAT64);
	case FIELD_INT16:
		return AddBuiltinType(schema::ESchemaBuiltinType::INT16);
	case FIELD_INT32:
	case FIELD_TICK:
	case FIELD_ENGINE_TICK:
		return AddBuiltinType(schema::ESchemaBuiltinType::INT32);
	case FIELD_INT64:
		return AddBuiltinType(schema::ESchemaBuiltinType::INT64);
	case FIELD_UINT8:
		return AddBuiltinType(schema::ESchemaBuiltinType::UINT8);
	case FIELD_UINT16:
		return AddBuiltinType(schema::ESchemaBuiltinType::UINT16);
	case FIELD_UINT32:
		return AddBuiltinType(schema::ESchemaBuiltinType::UINT32);
	case FIELD_UINT64:
		return AddBuiltinType(schema::ESchemaBuiltinType::UINT64);
	case FIELD_BOOLEAN:
		return AddBuiltinType(schema::ESchemaBuiltinType::BOOL);
	case FIELD_CHARACTER:
		return AddBuiltinType(schema::ESchemaBuiltinType::CHAR);
	case FIELD_CSTRING:
		return m_Model.AddType(schema::PointerType_t{
		    .m_PointeeType = AddBuiltinType(schema::ESchemaBuiltinType::CHAR),
		});
	case FIELD_SOUNDNAME:
		return AddBuiltinType(schema::ESchemaBuiltinType::INT32);
	case FIELD_FUNCTION:
		return m_Model.AddType(schema::PointerType_t{
		    .m_PointeeType = AddBuiltinType(kBuiltinVoid),
		});
	case FIELD_TIME:
		return AddAtomic("GameTime_t");
	case FIELD_STRING:
		return AddAtomic("CUtlSymbolLarge");
	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	case FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_VECTOR:
	case FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_POSITION_VECTOR:
	case FIELD_DIRECTION_VECTOR_WORLDSPACE:
	case FIELD_NETWORK_QUANTIZED_VECTOR:
	case FIELD_NETWORK_QUANTIZED_VECTORWS:
	case FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_VECTORWS:
		return AddAtomic("Vector");
	case FIELD_VECTOR2D:
		return AddAtomic("Vector2D");
	case FIELD_VECTOR4D:
		return AddAtomic("Vector4D");
	case FIELD_QANGLE:
	case FIELD_QANGLE_WORLDSPACE:
		return AddAtomic("QAngle");
	case FIELD_QUATERNION:
	case FIELD_QUATERNION_WORLDSPACE:
		return AddAtomic("Quaternion");
	case FIELD_UTLSTRING:
		return AddAtomic("CUtlString");
	case FIELD_UTLSTRINGTOKEN:
		return AddAtomic("CUtlStringToken");
	case FIELD_COLOR32:
		return AddAtomic("Color");
	case FIELD_WORLD_GROUP_ID:
		return AddAtomic("WorldGroupId_t");
	case FIELD_ROTATION_VECTOR:
	case FIELD_ROTATION_VECTOR_WORLDSPACE:
		return AddAtomic("RotationVector");
	case FIELD_CTRANSFORM:
	case FIELD_CTRANSFORM_WORLDSPACE:
		return AddAtomic("CTransform");
	case FIELD_SHIM:
		return AddAtomic("SHIM");
	case FIELD_ATTACHMENT_HANDLE:
		return AddAtomic("AttachmentHandle_t");
	case FIELD_GLOBALSYMBOL:
		return AddAtomic("CGlobalSymbol");
	case FIELD_EHANDLE:
		return AddAtomic("CHandle<CBaseEntity>");
	case FIELD_HMODEL:
		return AddAtomic("CStrongHandle<InfoForResourceTypeCModel>");
	case FIELD_HMATERIAL:
		return AddAtomic("CStrongHandle<InfoForResourceTypeIMaterial2>");
	case FIELD_HRENDERTEXTURE:
		return AddAtomic("CStrongHandle<InfoForResourceTypeCTextureBase>");
	case FIELD_HPARTICLESYSTEMDEFINITION:
		return AddAtomic("CStrongHandle<InfoForResourceTypeIParticleSystemDefinition>");
	case FIELD_HPOSTPROCESSING:
		return AddAtomic("CStrongHandle<InfoForResourceTypeCPostProcessingResource>");
	case FIELD_HNMGRAPHDEFINITION:
		return AddAtomic("CStrongHandle<InfoForResourceTypeCNmGraphDefinition>");
	case FIELD_MATRIX3X4_WORLDSPACE:
	case FIELD_MATRIX3X4:
		return AddAtomic("matrix3x4_t");
	case FIELD_HSCRIPT:
		return AddAtomic("HSCRIPT");
	default:
		return AddInvalidType("datamap field type " + std::to_string(nType));
	}
}

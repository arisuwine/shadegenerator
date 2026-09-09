#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "tier0/threadtools.hpp"
#include "tier1/utlmap.hpp"

#include "public/datamap.hpp"

class CSchemaClassInfo;
class CSchemaEnumInfo;
class CSchemaSystemTypeScope;
class CSchemaType;
class CSchemaType_DeclaredClass;

template <typename K, typename V>
class CSchemaPtrMap {
public:
	CUtlMap<K, V, uint16_t> m_Map;
	CThreadFastMutex        m_Mutex;
};

static_assert(sizeof(CSchemaPtrMap<int, void*>) == 0x30, "CSchemaPtrMap layout must match SchemaSystem");

// enums

enum SchemaCollectionManipulatorAction_t {
	// Returns count of the collection, index1 & index2 is unused
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_COUNT = 0,

	// Returns element from the collection at index1, index2 is unused
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT_CONST,
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT,

	// Swaps elements in a collection, index1 & index2 is first and second elements to swap
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_SWAP_ELEMENTS,
	// Inserts elements to a collection at index1 where index2 is how much elements to insert
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_INSERT_BEFORE,
	// Removes elements from a collection at index1 where index2 is how much elements to remove
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_REMOVE_MULTIPLE,

	// Sets the count of a collection of size index1, index2 is unused
	SCHEMA_COLLECTION_MANIPULATOR_ACTION_SET_COUNT,
};

enum SchemaTypeCategory_t : uint8_t {
	SCHEMA_TYPE_BUILTIN = 0,
	SCHEMA_TYPE_POINTER,
	SCHEMA_TYPE_BITFIELD,
	SCHEMA_TYPE_FIXED_ARRAY,
	SCHEMA_TYPE_ATOMIC,
	SCHEMA_TYPE_DECLARED_CLASS,
	SCHEMA_TYPE_DECLARED_ENUM,
	SCHEMA_TYPE_INVALID,
};

enum SchemaAtomicCategory_t : uint8_t {
	SCHEMA_ATOMIC_PLAIN = 0,
	SCHEMA_ATOMIC_T,
	SCHEMA_ATOMIC_COLLECTION_OF_T,
	SCHEMA_ATOMIC_TT,
	SCHEMA_ATOMIC_I,
	SCHEMA_ATOMIC_INVALID,
};

enum SchemaBuiltinType_t {
	SCHEMA_BUILTIN_TYPE_INVALID = 0,
	SCHEMA_BUILTIN_TYPE_VOID,
	SCHEMA_BUILTIN_TYPE_CHAR,
	SCHEMA_BUILTIN_TYPE_INT8,
	SCHEMA_BUILTIN_TYPE_UINT8,
	SCHEMA_BUILTIN_TYPE_INT16,
	SCHEMA_BUILTIN_TYPE_UINT16,
	SCHEMA_BUILTIN_TYPE_INT32,
	SCHEMA_BUILTIN_TYPE_UINT32,
	SCHEMA_BUILTIN_TYPE_INT64,
	SCHEMA_BUILTIN_TYPE_UINT64,
	SCHEMA_BUILTIN_TYPE_FLOAT32,
	SCHEMA_BUILTIN_TYPE_FLOAT64,
	SCHEMA_BUILTIN_TYPE_BOOL,
	SCHEMA_BUILTIN_TYPE_COUNT,
};

enum SchemaClassFlags_t : uint32_t {
	SCHEMA_CF1_HAS_VIRTUAL_MEMBERS                          = (1 << 0),
	SCHEMA_CF1_IS_ABSTRACT                                  = (1 << 1),
	SCHEMA_CF1_HAS_TRIVIAL_CONSTRUCTOR                      = (1 << 2),
	SCHEMA_CF1_HAS_TRIVIAL_DESTRUCTOR                       = (1 << 3),
	SCHEMA_CF1_LIMITED_METADATA                             = (1 << 4),
	SCHEMA_CF1_INHERITANCE_DEPTH_CALCULATED                 = (1 << 5),
	SCHEMA_CF1_MODULE_LOCAL_TYPE_SCOPE                      = (1 << 6),
	SCHEMA_CF1_GLOBAL_TYPE_SCOPE                            = (1 << 7),
	SCHEMA_CF1_CONSTRUCT_ALLOWED                            = (1 << 8),
	SCHEMA_CF1_CONSTRUCT_DISALLOWED                         = (1 << 9),
	SCHEMA_CF1_INFO_TAG_MNetworkAssumeNotNetworkable        = (1 << 10),
	SCHEMA_CF1_INFO_TAG_MNetworkNoBase                      = (1 << 11),
	SCHEMA_CF1_INFO_TAG_MIgnoreTypeScopeMetaChecks          = (1 << 12),
	SCHEMA_CF1_INFO_TAG_MDisableDataDescValidation          = (1 << 13),
	SCHEMA_CF1_INFO_TAG_MClassHasEntityLimitedDataDesc      = (1 << 14),
	SCHEMA_CF1_INFO_TAG_MClassHasCustomAlignedNewDelete     = (1 << 15),
	SCHEMA_CF1_UNK016                                       = (1 << 16),
	SCHEMA_CF1_INFO_TAG_MConstructibleClassBase             = (1 << 17),
	SCHEMA_CF1_INFO_TAG_MHasKV3TransferPolymorphicClassname = (1 << 18),
};

enum SchemaEnumFlags_t : uint8_t {
	SCHEMA_EF_IS_REGISTERED           = (1 << 0),
	SCHEMA_EF_MODULE_LOCAL_TYPE_SCOPE = (1 << 1),
	SCHEMA_EF_GLOBAL_TYPE_SCOPE       = (1 << 2),
};

enum SchemaClassManipulatorAction_t {
	// Registers pObject in a schemasystem
	SCHEMA_CLASS_MANIPULATOR_ACTION_REGISTER = 0,
	SCHEMA_CLASS_MANIPULATOR_ACTION_REGISTER_PRE,

	// Allocates object on the heap and constructs it in place, pObject is unused
	SCHEMA_CLASS_MANIPULATOR_ACTION_ALLOCATE,
	// Deallocates pObject
	SCHEMA_CLASS_MANIPULATOR_ACTION_DEALLOCATE,

	// Constructs pObject in place
	SCHEMA_CLASS_MANIPULATOR_ACTION_CONSTRUCT_IN_PLACE,
	// Destructs pObject in place
	SCHEMA_CLASS_MANIPULATOR_ACTION_DESCTRUCT_IN_PLACE,

	// Returns schema binding of pObject
	SCHEMA_CLASS_MANIPULATOR_ACTION_GET_SCHEMA_BINDING,
};

// typedefs

typedef int LoggingChannelID_t;
typedef void* (*SchemaCollectionManipulatorFn_t)(SchemaCollectionManipulatorAction_t eAction, void* pCollection, int index1, int index2);
typedef void* (*SchemaClassManipulatorFn_t)(SchemaClassManipulatorAction_t eAction, void* pObject);

// structs

struct SchemaMetadataEntryData_t {
	const char* m_pszName;
	void*       m_pData;
};

struct SchemaAtomicTypeInfo_t {
	const char* m_pszName;
	const char* m_pszTokenName;

	int m_nAtomicID;

	int                        m_nStaticMetadataCount;
	SchemaMetadataEntryData_t* m_pStaticMetadata;
};

struct SchemaBaseClassInfoData_t {
	uint32_t          m_nOffset;
	CSchemaClassInfo* m_pClass;
};

struct SchemaEnumeratorInfoData_t {
	const char* m_pszName;

	int64_t m_nValue;

	int                        m_nStaticMetadataCount;
	SchemaMetadataEntryData_t* m_pStaticMetadata;

	[[nodiscard]] std::string_view GetName() const noexcept {
		return m_pszName ? m_pszName : "";
	}
};

template <typename T>
struct SchemaMetaInfoHandle_t {
	SchemaMetaInfoHandle_t() : m_pObj(nullptr) {}
	SchemaMetaInfoHandle_t(T* obj) : m_pObj(obj) {}

	[[nodiscard]] inline T* Get() const {
		return m_pObj;
	}
	bool operator<(const SchemaMetaInfoHandle_t& rhs) const {
		return m_pObj < rhs.m_pObj;
	}
	bool operator==(const SchemaMetaInfoHandle_t& rhs) const {
		return m_pObj == rhs.m_pObj;
	}
	bool operator!=(const SchemaMetaInfoHandle_t& rhs) const {
		return m_pObj != rhs.m_pObj;
	}
	T& operator*() const {
		return *m_pObj;
	};
	T* operator->() const {
		return m_pObj;
	};

	T* m_pObj;
};

struct AtomicTypeInfo_T_t {
	int                             m_nAtomicID;
	CSchemaType*                    m_pTemplateType;
	SchemaCollectionManipulatorFn_t m_pfnManipulator;

	bool operator<(const AtomicTypeInfo_T_t& rhs) const {
		if (m_nAtomicID != rhs.m_nAtomicID)
			return m_nAtomicID < rhs.m_nAtomicID;

		if (m_pTemplateType != rhs.m_pTemplateType)
			return m_pTemplateType < rhs.m_pTemplateType;

		return (void*)m_pfnManipulator < (void*)rhs.m_pfnManipulator;
	}
};

struct AtomicTypeInfo_CollectionOfT_t {
	int                             m_nAtomicID;
	CSchemaType*                    m_pTemplateType;
	uint64_t                        m_nFixedBufferCount;
	SchemaCollectionManipulatorFn_t m_pfnManipulator;

	bool operator<(const AtomicTypeInfo_CollectionOfT_t& rhs) const {
		if (m_nAtomicID != rhs.m_nAtomicID)
			return m_nAtomicID < rhs.m_nAtomicID;

		if (m_pTemplateType != rhs.m_pTemplateType)
			return m_pTemplateType < rhs.m_pTemplateType;

		if (m_nFixedBufferCount != rhs.m_nFixedBufferCount)
			return m_nFixedBufferCount < rhs.m_nFixedBufferCount;

		return (void*)m_pfnManipulator < (void*)rhs.m_pfnManipulator;
	}
};

struct AtomicTypeInfo_TT_t {
	bool operator<(const AtomicTypeInfo_TT_t& rhs) const {
		if (m_nAtomicID != rhs.m_nAtomicID)
			return m_nAtomicID < rhs.m_nAtomicID;

		if (m_pTemplateType != rhs.m_pTemplateType)
			return m_pTemplateType < rhs.m_pTemplateType;

		return m_pTemplateType2 < rhs.m_pTemplateType2;
	}

	int          m_nAtomicID;
	CSchemaType* m_pTemplateType;
	CSchemaType* m_pTemplateType2;
};

struct AtomicTypeInfo_I_t {
	bool operator<(const AtomicTypeInfo_I_t& rhs) const {
		if (m_nAtomicID != rhs.m_nAtomicID)
			return m_nAtomicID < rhs.m_nAtomicID;

		return m_nInteger < rhs.m_nInteger;
	}

	int m_nAtomicID;
	int m_nInteger;
};

struct TypeAndCountInfo_t {
	int          m_nElementCount;
	CSchemaType* m_pElementType;
};

template <typename T>
struct SchemaDeclaredTypeEntry_t {
	uint32_t m_Hash1;
	uint32_t m_Hash2;
	uint32_t m_Hash3;
	uint32_t m_Hash4;
	T*       m_pData;
};

// CSchemaTypes

class CSchemaType {
public:
	void*                   vft; // 0x0
	const char*             m_sTypeName; // 0x8
	CSchemaSystemTypeScope* m_pTypeScope; // 0x10
	SchemaTypeCategory_t    m_eTypeCategory; // 0x18
	SchemaAtomicCategory_t  m_eAtomicCategory; // 0x19

	template <typename T>
	inline const T* Cast() const {
		return static_cast<const T*>(this);
	}

	[[nodiscard]] std::string_view GetTypeName() const noexcept {
		return m_sTypeName ? m_sTypeName : "";
	}
};

class CSchemaType_Builtin : public CSchemaType {
public:
	SchemaBuiltinType_t m_eBuiltinType; // int32
	uint8_t             m_nSize;

	[[nodiscard]] uint8_t GetSize() const {
		switch (m_eBuiltinType) {
		case SCHEMA_BUILTIN_TYPE_CHAR:
		case SCHEMA_BUILTIN_TYPE_INT8:
		case SCHEMA_BUILTIN_TYPE_UINT8:
		case SCHEMA_BUILTIN_TYPE_BOOL:
			return sizeof(bool);
		case SCHEMA_BUILTIN_TYPE_INT16:
		case SCHEMA_BUILTIN_TYPE_UINT16:
			return sizeof(uint16_t);
		case SCHEMA_BUILTIN_TYPE_INT32:
		case SCHEMA_BUILTIN_TYPE_UINT32:
		case SCHEMA_BUILTIN_TYPE_FLOAT32:
			return sizeof(uint32_t);
		case SCHEMA_BUILTIN_TYPE_INT64:
		case SCHEMA_BUILTIN_TYPE_UINT64:
		case SCHEMA_BUILTIN_TYPE_FLOAT64:
			return sizeof(uint64_t);
		case SCHEMA_BUILTIN_TYPE_VOID:
		case SCHEMA_BUILTIN_TYPE_INVALID:
		case SCHEMA_BUILTIN_TYPE_COUNT:
			return 0;
		}

		return 0;
	}
};

class CSchemaType_Ptr : public CSchemaType {
public:
	CSchemaType* m_pObjectType;
};

class CSchemaType_Atomic : public CSchemaType {
public:
	SchemaAtomicTypeInfo_t* m_pAtomicInfo;
	int                     m_nAtomicID;
	uint16_t                m_nSize;
	uint8_t                 m_nAlignment;

	[[nodiscard]] std::string_view GetName() const {
		std::string_view szTypeName = m_sTypeName;
		if (auto p = szTypeName.find('<'); p != std::string_view::npos)
			szTypeName = szTypeName.substr(0, p);
		while (!szTypeName.empty() && std::isspace(static_cast<unsigned char>(szTypeName.back())))
			szTypeName.remove_suffix(1);
		return szTypeName;
	}
};

class CSchemaType_Atomic_T : public CSchemaType_Atomic {
public:
	CSchemaType* m_pTemplateType;
};

class CSchemaType_Atomic_CollectionOfT : public CSchemaType_Atomic_T {
public:
	SchemaCollectionManipulatorFn_t m_pfnManipulator;
	uint16_t                        m_nElementSize;
	uint64_t                        m_nFixedBufferCount;
};

class CSchemaType_Atomic_TT : public CSchemaType_Atomic_T {
public:
	CSchemaType* m_pTemplateType2;
};

class CSchemaType_Atomic_I : public CSchemaType_Atomic {
public:
	int m_nInteger;
};

class CSchemaType_DeclaredClass : public CSchemaType {
public:
	CSchemaClassInfo* m_pClassInfo;
	bool              m_bGlobalPromotionRequired;
};

class CSchemaType_DeclaredEnum : public CSchemaType {
public:
	CSchemaEnumInfo* m_pEnumInfo;
	bool             m_bGlobalPromotionRequired;
};

class CSchemaType_FixedArray : public CSchemaType {
public:
	int          m_nElementCount;
	uint16_t     m_nElementSize;
	uint8_t      m_nElementAlignment;
	CSchemaType* m_pElementType;
};

class CSchemaType_Bitfield : public CSchemaType {
public:
	int m_nBitfieldCount;
};

// Info

struct SchemaClassFieldData_t {
	const char* m_pszName;

	CSchemaType* m_pType;

	int m_nSingleInheritanceOffset;

	int                        m_nStaticMetadataCount;
	SchemaMetadataEntryData_t* m_pStaticMetadata;

	[[nodiscard]] std::string GetTypeName() const {
		std::string sTypeName = m_pType->m_sTypeName;
		sTypeName.erase(std::remove(sTypeName.begin(), sTypeName.end(), ' '), sTypeName.end());
		return sTypeName;
	}

	[[nodiscard]] int GetOffset() const noexcept {
		return m_nSingleInheritanceOffset;
	}

	[[nodiscard]] std::span<SchemaMetadataEntryData_t> GetMetadata() const {
		if (m_nStaticMetadataCount == 0 || !m_pStaticMetadata)
			return {};

		return { m_pStaticMetadata, static_cast<size_t>(m_nStaticMetadataCount) };
	}

	[[nodiscard]] std::string_view GetName() const noexcept {
		return m_pszName ? m_pszName : "";
	}
};

struct SchemaClassInfoData_t {
	CSchemaClassInfo* m_pSchemaBinding; // 0x0000

	const char* m_pszName; // 0x0008
	const char* m_pszProjectName; // 0x0010
#ifndef SHADE_GAME_DEADLOCK
	const char* m_pszCPPName; // 0x0018
#endif

	int m_nSize; // 0x0020

	uint16_t m_nFieldCount;
	uint16_t m_nStaticMetadataCount;

	uint8_t m_nAlignment;
	uint8_t m_nBaseClassCount;

	uint16_t m_nMultipleInheritanceDepth;
	uint16_t m_nSingleInheritanceDepth;

	SchemaClassFieldData_t*    m_pFields;
	SchemaBaseClassInfoData_t* m_pBaseClasses;
	datamap_t*                 m_pDataDescMap;
	SchemaMetadataEntryData_t* m_pStaticMetadata;

	CSchemaSystemTypeScope*    m_pTypeScope;
	CSchemaType_DeclaredClass* m_pDeclaredClass;

	SchemaClassFlags_t m_nClassFlags;
	uint32_t           m_nFlags2;

	typedef void* (*SchemaClassManipulatorFn_t)(int eAction, void* pObject);
	SchemaClassManipulatorFn_t m_pfnManipulator;
};

class CSchemaClassInfo : public SchemaClassInfoData_t {
public:
	[[nodiscard]] std::span<SchemaClassFieldData_t> GetFields() const {
		if (!m_pFields || !m_nFieldCount)
			return {};

		return { m_pFields, m_nFieldCount };
	}

	[[nodiscard]] std::string_view GetModuleName() const {
		if (!m_pszProjectName)
			return {};

		return m_pszProjectName;
	}

	[[nodiscard]] std::string GetName() const {
		if (!m_pszName)
			return "";

		std::string sName = m_pszName;
		std::replace(sName.begin(), sName.end(), ':', '_');

		return sName;
	}

	[[nodiscard]] std::string GetBaseClassName() const {
		if (!m_pBaseClasses || !m_pBaseClasses->m_pClass)
			return {};

		return m_pBaseClasses->m_pClass->GetName();
	}

	[[nodiscard]] std::span<const SchemaBaseClassInfoData_t> GetBaseClasses() const {
		if (!m_pBaseClasses || !m_nBaseClassCount)
			return {};

		return { m_pBaseClasses, m_nBaseClassCount };
	}

	[[nodiscard]] uint32_t GetFlags() const noexcept {
		return static_cast<uint32_t>(m_nClassFlags);
	}

	[[nodiscard]] std::span<SchemaMetadataEntryData_t> GetStaticMetaData() const {
		if (!m_pStaticMetadata || !m_nStaticMetadataCount)
			return {};

		return { m_pStaticMetadata, m_nStaticMetadataCount };
	}
};

struct SchemaEnumInfoData_t {
	CSchemaEnumInfo* m_pSchemaBinding;

	const char* m_pszName;
	const char* m_pszProjectName;

	uint8_t m_nSize;
	uint8_t m_nAlignment;

	uint8_t m_nFlags;

	uint16_t m_nEnumeratorCount;
	uint16_t m_nStaticMetadataCount;

	SchemaEnumeratorInfoData_t* m_pEnumerators;
	SchemaMetadataEntryData_t*  m_pStaticMetadata;

	CSchemaSystemTypeScope* m_pTypeScope;

	int64_t m_nMinEnumeratorValue;
	int64_t m_nMaxEnumeratorValue;
};

class CSchemaEnumInfo : public SchemaEnumInfoData_t {
public:
	[[nodiscard]] std::string_view GetModuleName() const {
		if (!m_pszProjectName)
			return {};

		return m_pszProjectName;
	}

	[[nodiscard]] std::string GetName() const {
		if (!m_pszName)
			return "";

		std::string sName = m_pszName;
		std::replace(sName.begin(), sName.end(), ':', '_');

		return sName;
	}

	[[nodiscard]] uint8_t GetFlags() const noexcept {
		return m_nFlags;
	}

	[[nodiscard]] std::vector<std::string> FlagsToString() const {
		std::vector<std::string> flags;

		if (m_nFlags & SCHEMA_EF_MODULE_LOCAL_TYPE_SCOPE)
			flags.emplace_back("Local Type Scope");

		if (m_nFlags & SCHEMA_EF_GLOBAL_TYPE_SCOPE)
			flags.emplace_back("Global Type Scope");

		return flags;
	}

	[[nodiscard]] uint8_t GetSize() const {
		return m_nSize;
	}

	[[nodiscard]] int64_t GetMinValue() const {
		return m_nMinEnumeratorValue;
	}

	[[nodiscard]] std::span<SchemaEnumeratorInfoData_t> GetEnumerators() const {
		if (!m_pEnumerators || !m_nEnumeratorCount)
			return {};

		return { m_pEnumerators, m_nEnumeratorCount };
	}

	[[nodiscard]] std::span<SchemaMetadataEntryData_t> GetStaticMetaData() const {
		if (!m_pStaticMetadata || !m_nStaticMetadataCount)
			return {};

		return { m_pStaticMetadata, m_nStaticMetadataCount };
	}
};

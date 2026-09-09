#pragma once
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "schematypes.hpp"

#include "tier1/utltshash.hpp"
#include "tier1/utlvector.hpp"

static constexpr std::string_view kszSchemaInterface = "SchemaSystem_001";

class ISchemaSystemTypeScope {
public:
};

class CSchemaSystemTypeScope : public ISchemaSystemTypeScope {
public:
	void*                                                                            __vftable; // 0x0000
	char                                                                             m_ScopeName[256]; // 0x0008
	CSchemaSystemTypeScope*                                                          m_pParentScope;
	bool                                                                             m_bBuiltinTypesInitalized; // 0x0110
	CSchemaType_Builtin                                                              m_BuiltinTypes[14]; // 0x0118
	CSchemaPtrMap<CSchemaType*, CSchemaType_Ptr*>                                    m_Ptr; // 0x0348
	CSchemaPtrMap<int, CSchemaType_Atomic*>                                          m_Atomic; // 0x0378
	CSchemaPtrMap<AtomicTypeInfo_T_t, CSchemaType_Atomic_T*>                         m_Atomic_T; // 0x03A8
	CSchemaPtrMap<AtomicTypeInfo_CollectionOfT_t, CSchemaType_Atomic_CollectionOfT*> m_Atomic_CollectionOfT; // 0x03D8
	CSchemaPtrMap<AtomicTypeInfo_TT_t, CSchemaType_Atomic_TT*>                       m_Atomic_TT; // 0x0408
	CSchemaPtrMap<AtomicTypeInfo_I_t, CSchemaType_Atomic_I*>                         m_Atomic_I; // 0x0438
	CSchemaPtrMap<uint16_t, CSchemaType_DeclaredClass*>                              m_DeclaredClass; // 0x0468
	CSchemaPtrMap<uint16_t, CSchemaType_DeclaredEnum*>                               m_DeclaredEnum; // 0x0498
	CSchemaPtrMap<int, const SchemaAtomicTypeInfo_t*>                                m_AtomicTypeInfo; // 0x04C8
	CSchemaPtrMap<TypeAndCountInfo_t, CSchemaType_FixedArray*>                       m_FixedArray; // 0x04F8
	CSchemaPtrMap<int, CSchemaType_Bitfield*>                                        m_Bitfield; // 0x0528
	char                                                                             pad_0558[0x8];
	CUtlTSHash<CSchemaClassInfo*, 256, uint32_t>                                     m_ClassBindings; // 0x0560
	CUtlTSHash<CSchemaEnumInfo*, 256, uint32_t>                                      m_EnumBindings; // 0x1DD0

	[[nodiscard]] std::string_view GetName() const {
		return m_ScopeName;
	}
};

struct ClassBindingScopeBlock_t {
public:
	uint64_t                   m_Hash;
	uint64_t                   m_pUnknown;
	CSchemaType_DeclaredClass* m_pDeclaredClass;
};

enum ESchemaBindingError {
	INVALID_FUNC_ADDRESS,
	BAD_RESULT
};

class ISchemaSystem {
public:
	[[nodiscard]] std::expected<void, ESchemaBindingError> InstallSchemaBinding(HMODULE hModule) {
		using fnInstallSchemaBinding = bool(__cdecl*)(const char*, ISchemaSystem*);

		const auto fn = reinterpret_cast<fnInstallSchemaBinding>(GetProcAddress(hModule, "InstallSchemaBindings"));
		if (!fn)
			return std::unexpected(INVALID_FUNC_ADDRESS);

		if (!fn(kszSchemaInterface.data(), this))
			return std::unexpected(BAD_RESULT);

		return {};
	}
};

class CSchemaSystem : public ISchemaSystem {
public:
	char                                pad[0x190];
	CUtlVector<CSchemaSystemTypeScope*> m_TypeScopes;

	[[nodiscard]] std::span<CSchemaSystemTypeScope*> GetTypeScopes() const {
		if (m_TypeScopes.Count() == 0)
			return {};

		return { m_TypeScopes.m_Memory.m_pMemory, static_cast<size_t>(m_TypeScopes.Count()) };
	}

	[[nodiscard]] CSchemaSystemTypeScope* GetGlobalTypeScope() {
		static CSchemaSystemTypeScope* pGlobalTypeScope = nullptr;

		if (m_TypeScopes.Count() != 0)
			pGlobalTypeScope = m_TypeScopes.Element(0)->m_pParentScope;

		return pGlobalTypeScope;
	}
};

inline CSchemaSystem* g_pSchemaSystem = nullptr;
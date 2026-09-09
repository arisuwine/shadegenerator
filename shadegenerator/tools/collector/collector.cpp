#include "collector.hpp"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "detail.hpp"

#include "sdk/schemasystem/schemasystem.hpp"
#include "sdk/schemasystem/schematypes.hpp"

#include "utils/debug.hpp"

using shade::schema::SchemaTypeName_t;
using shade::tools::collector::ForEachMapValue;

shade::schema::SchemaTypeRef_t shade::tools::CSchemaCollector::AddBuiltinType(schema::ESchemaBuiltinType type) {
	return m_Model.AddType(schema::BuiltinType_t{ type });
}

shade::schema::SchemaTypeRef_t shade::tools::CSchemaCollector::AddInvalidType(std::string szName) {
	if (szName.empty())
		szName = "<unnamed>";

	if (m_ReportedInvalidTypes.emplace(szName).second)
		lg::Warn("collector", "unknown schema type '{}'; preserving it as invalid", szName);

	return m_Model.AddType(schema::InvalidType_t{ .m_szName = std::move(szName) });
}

shade::schema::CSchemaModel shade::tools::CSchemaCollector::Collect(CSchemaSystem& schemaSystem) {
	m_Model = {};
	m_CollectedTypes.clear();
	m_ReportedInvalidTypes.clear();

	const auto pGlobalTypeScope = schemaSystem.GetGlobalTypeScope();
	if (!pGlobalTypeScope) {
		lg::Warn("collector", "invalid global type scope");

		return std::move(m_Model);
	}

	const auto typeScopes = schemaSystem.GetTypeScopes();
	if (typeScopes.empty()) {
		lg::Warn("collector", "schema system has no type scopes");

		return std::move(m_Model);
	}

	std::vector<const CSchemaClassInfo*> classes;
	std::vector<const CSchemaEnumInfo*>  enums;
	std::unordered_set<std::string>      seenClasses;
	std::unordered_set<std::string>      seenEnums;
	std::size_t                          nDuplicateClassCount = 0;
	std::size_t                          nDuplicateEnumCount  = 0;

	for (auto* scope : typeScopes) {
		if (!scope)
			continue;

		std::size_t nClassCount = 0;
		std::size_t nEnumCount  = 0;
		for (const auto pInfo : scope->m_ClassBindings.GetElements()) {
			if (!pInfo)
				continue;

			const SchemaTypeName_t name  = { .m_szModule = std::string(pInfo->GetModuleName()), .m_szName = pInfo->GetName() };
			const std::string      szKey = name.m_szModule + '\0' + name.m_szName;

			if (seenClasses.emplace(szKey).second) {
				classes.push_back(pInfo);
				++nClassCount;
			} else
				++nDuplicateClassCount;
		}

		for (const auto pInfo : scope->m_EnumBindings.GetElements()) {
			if (!pInfo)
				continue;

			const SchemaTypeName_t name  = { .m_szModule = std::string(pInfo->GetModuleName()), .m_szName = pInfo->GetName() };
			const std::string      szKey = name.m_szModule + '\0' + name.m_szName;

			if (seenEnums.emplace(szKey).second) {
				enums.push_back(pInfo);
				++nEnumCount;
			} else
				++nDuplicateEnumCount;
		}

		lg::Info("collector", "{} classes and {} enums collected from {}", nClassCount, nEnumCount, scope->GetName());
	}

	const auto nDuplicateBindingCount = nDuplicateClassCount + nDuplicateEnumCount;
	if (nDuplicateBindingCount != 0)
		lg::Info("collector", "skipped {} duplicate bindings ({} classes and {} enums); kept the first binding", nDuplicateBindingCount,
		         nDuplicateClassCount, nDuplicateEnumCount);

	auto RegisterScopeTypes = [this](const CSchemaSystemTypeScope* pTypeScope) {
		if (!pTypeScope)
			return;

		ForEachMapValue(pTypeScope->m_DeclaredClass, [this](const CSchemaType* pType) {
			if (pType)
				CollectType(pType);
		});

		ForEachMapValue(pTypeScope->m_DeclaredEnum, [this](const auto* type) {
			if (type)
				CollectType(type);
		});
	};

	RegisterScopeTypes(pGlobalTypeScope);
	for (const auto* scope : typeScopes)
		RegisterScopeTypes(scope);

	for (const auto info : classes)
		if (info->m_pDeclaredClass)
			CollectType(info->m_pDeclaredClass);

	for (const auto info : classes)
		m_Model.AddClass(CollectClass(*info));

	for (const auto info : enums)
		m_Model.AddEnum(CollectEnum(*info));

	CollectAtomics(pGlobalTypeScope, typeScopes);

	return std::move(m_Model);
}
